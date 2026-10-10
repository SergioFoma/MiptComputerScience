/*
 * Инструкция по запуску:
 *   1. Собрать на Linux:  gcc -Wall extra.c -o extra
 *      (macOS не поддерживает System V shm — запускать только на Linux)
 *   2. Создать файл-метку для генерации IPC-ключа:  touch chat.key
 *      Содержимое файла не важно — ftok() использует только inode и номер
 *      устройства. ВАЖНО: всем пользователям нужен ОДИН и тот же файл.
 *   3. Каждый пользователь запускает свою копию в отдельном терминале:
 *        ./extra <свое_имя> chat.key
 *      После fork(): родительский процесс — только пишет, дочерний — только читает.
 */
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "extra.h"

// ================== ПРОТОКОЛ ХРАНЕНИЯ ДАННЫХ ПОЛЬЗОВАТЕЛЕЙ =================
// [время][имя_получателя][имя_отправителя][pid_отправителя]<сообщение>
// ===========================================================================

// Сегмент создаётся с IPC_CREAT, первый процесс его инициализирует
// (новый сегмент разделяемой памяти обнулён: lock = 0, write_index = 0).

// Занять межпроцессный спинлок: крутимся, пока не сумели первыми
// атомарно поставить lock = 1 (защита от одновременной записи двух писателей)
static void LockChat(chat_shm_t* chat) {
  assert(chat);

  // активное ожидание: крутимся, пока не сумели первыми поставить lock = 1
  while (__sync_lock_test_and_set(&chat->lock, 1)) {
    usleep(1000);
  }
}

// Освободить спинлок (атомарно обнулить lock)
static void UnlockChat(chat_shm_t* chat) {
  assert(chat);
  __sync_lock_release(&chat->lock);
}

int main(int argc, char** argv) {

  if (argc != 3) {
    printf("argc: Incorrect number of arguments\n");
    return 0;
  }

  int error_code = 0;
  user_info_t user_info = GetUserData(argv, &error_code);

  if (error_code != 0) {
    printf("GetUserData: Error!\n");
    return 0;
  }

  pid_t current_pid = fork();

  if (current_pid == -1) {
    printf("fork(): Error!\n");
    shmdt(user_info.chat);
  } else if (current_pid == 0) {
    // поток-читатель: только читает сообщения, адресованные мне
    error_code = ReadingData(&user_info);
  } else {
    // поток-писатель: только пишет мои сообщения в разделяемую память
    error_code = SaveData(&user_info);
    shmdt(user_info.chat);
  }

  return error_code;
}

// Собрать данные пользователя: имя из argv[1], pid процесса и
// подключить разделяемую память (база данных чата)
user_info_t GetUserData(char** argv, int* error_code) {
  assert(argv);
  assert(error_code);

  user_info_t user_info = {};

  user_info.sender_name = argv[1];
  user_info.sender_id = getpid();
  user_info.chat = AttachSharedMemory(argv[2], &user_info.shm_id, error_code);

  return user_info;
}

// Создать (если ещё нет) и подключить сегмент разделяемой памяти:
// ftok -> shmget -> shmat. Все пользователи с одним key_path попадают
// в один и тот же сегмент. Возвращает указатель на структуру чата.
chat_shm_t* AttachSharedMemory(const char* key_path, int* shm_id, int* error_code) {
  assert(key_path);
  assert(shm_id);
  assert(error_code);

  // ключ генерируется из имени существующего файла; все пользователи
  // должны передавать один и тот же путь (как раньше передавали один chat.txt)
  key_t key = ftok(key_path, 0);

  if (key == (key_t)-1) {
    printf("ftok(): Error! Does file '%s' exist?\n", key_path);
    *error_code = -1;
    return NULL;
  }

  int shmid = shmget(key, sizeof(chat_shm_t), 0666 | IPC_CREAT);

  if (shmid == -1) {
    printf("shmget(): Error!\n");
    *error_code = -1;
    return NULL;
  }

  chat_shm_t* chat = (chat_shm_t*)shmat(shmid, NULL, 0);

  if (chat == (chat_shm_t*)-1) {
    printf("shmat(): Error!\n");
    *error_code = -1;
    return NULL;
  }

  *shm_id = shmid;
  return chat;
}

// Цикл читателя (дочерний процесс): раз в 3 секунды печатает новые
// сообщения, адресованные текущему пользователю. Ничего не пишет.
int ReadingData(user_info_t* user_info) {
  assert(user_info);
  assert(user_info->chat);

  int time_sleep = 3;
  uint64_t last_index = 0;

  while (1) {
    // снимок количества сообщений под блокировкой
    LockChat(user_info->chat);
    uint64_t write_index = user_info->chat->write_index;
    UnlockChat(user_info->chat);

    if (write_index > last_index) {
      // если кольцевой буфер уже затёр старые сообщения,
      // начинаем с самого старого оставшегося в памяти
      uint64_t begin_index = last_index;
      if (write_index - begin_index > MAX_MESSAGES) {
        begin_index = write_index - MAX_MESSAGES;
      }

      ProcessNewMessages(user_info->chat, begin_index, write_index,
                         user_info->sender_name);

      // обновляем последнее прочитанное сообщение
      last_index = write_index;
    }

    sleep(time_sleep);
  }

  return 0;
}

// Обойти слоты кольцевого буфера [begin_index, end_index) и напечатать
// те, где получатель == current_user (аналог ProcessNewMessages из file-версии)
void ProcessNewMessages(chat_shm_t* chat, uint64_t begin_index,
                        uint64_t end_index, const char* current_user) {
  assert(chat);
  assert(current_user);

  for (uint64_t i = begin_index; i < end_index; ++i) {
    shm_message_t* slot = &chat->messages[i % MAX_MESSAGES];

    if (!slot->used) {
      continue;
    }

    if (strcmp(current_user, slot->recipient_name) == 0) {
      printf("%s\n", slot->message);
      fflush(stdout);
    }
  }
}

// Цикл писателя (родительский процесс): читает строки из stdin и отправляет
// их в разделяемую память. Ничего не читает из памяти.
int SaveData(user_info_t* user_info) {
  assert(user_info);
  assert(user_info->chat);

  char* line = NULL;
  size_t cap = 0;
  ssize_t result = 0;

  while ((result = getline(&line, &cap, stdin)) > 0) {
    if (line[result - 1] == '\n') {
      line[result - 1] = '\0';
    }

    message_info_t tokens = ParseLine(line, result);

    SendMessage(user_info, &tokens);

    FreeTokens(&tokens);
  }

  free(line);
  return 0;
}

// Разобрать строку вида "Pavel: Hello, Pavel. What do you do?":
// выделить получателя (до ':'), текст (после ': ') и подставить время отправки
message_info_t ParseLine(char* line, ssize_t len) {
  assert(line);

  // шаблон строки: 'Pavel: Hello, Pavel. What do you do?'

  message_info_t info = {};

  char* colon = strchr(line, ':');
  char* whitespace = strchr(line, ' ');

  if (!colon || !whitespace || whitespace < colon) {
    printf("ParseLine: Incorrect format, message skipped\n");
    return info;
  }

  info.sending_time = time(NULL);
  info.recipient_name = strndup(line, colon - line);
  info.message = strdup(whitespace + 1);

  return info;
}

// Под спинлоком записать сообщение в следующий слот кольцевого буфера
// (write_index % MAX_MESSAGES) и увеличить write_index
void SendMessage(user_info_t* user_info, message_info_t* tokens) {
  assert(user_info);
  assert(tokens);

  if (!tokens->recipient_name || !tokens->message) {
    return;
  }

  chat_shm_t* chat = user_info->chat;

  LockChat(chat);

  // создаём запись в следующем свободном слоте кольцевого буфера
  shm_message_t* slot = &chat->messages[chat->write_index % MAX_MESSAGES];

  slot->used = 0;
  slot->sending_time = tokens->sending_time;
  slot->sender_id = user_info->sender_id;
  strncpy(slot->recipient_name, tokens->recipient_name, MAX_NAME_LEN - 1);
  slot->recipient_name[MAX_NAME_LEN - 1] = '\0';
  strncpy(slot->sender_name, user_info->sender_name, MAX_NAME_LEN - 1);
  slot->sender_name[MAX_NAME_LEN - 1] = '\0';
  strncpy(slot->message, tokens->message, MAX_MESSAGE_LEN - 1);
  slot->message[MAX_MESSAGE_LEN - 1] = '\0';
  slot->used = 1;

  ++chat->write_index;

  UnlockChat(chat);
}

// Освободить временные строки, выделенные в ParseLine (strndup/strdup)
void FreeTokens(message_info_t* tokens) {
  assert(tokens);

  free(tokens->recipient_name);
  free(tokens->message);
}
