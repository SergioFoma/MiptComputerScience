#ifndef EXTRA_H_
#define EXTRA_H_

#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#define MAX_NAME_LEN    64              // максимальная длина имени пользователя
#define MAX_MESSAGE_LEN 512             // максимальная длина текста сообщения
#define MAX_MESSAGES    256             // размер кольцевого буфера в разделяемой памяти

typedef struct shm_message_s {
  volatile int used;                    // слот содержит корректное сообщение
  time_t sending_time;                  // когда сообщение было отправлено
  pid_t sender_id;                      // pid процесса-отправителя
  char recipient_name[MAX_NAME_LEN];    // имя получателя
  char sender_name[MAX_NAME_LEN];       // имя отправителя
  char message[MAX_MESSAGE_LEN];        // текст сообщения от отправителя
} shm_message_t;

// Сегмент разделяемой памяти: кольцевой буфер сообщений + спинлок
typedef struct chat_shm_s {
  volatile int lock;                    // межпроцессный спинлок (мьютекс)
  uint64_t write_index;                 // сколько всего сообщений записано
  shm_message_t messages[MAX_MESSAGES]; // кольцевой буфер сообщений
} chat_shm_t;

typedef struct user_info_s {
  const char* sender_name;              // имя отправителя
  pid_t sender_id;                      // pid отправителя
  int shm_id;                           // id сегмента разделяемой памяти (для shmctl)
  chat_shm_t* chat;                     // подключённая разделяемая память (база данных)
} user_info_t;

typedef struct message_info_s {
  char* recipient_name;                 // имя получателя
  char* message;                        // сообщение от отправителя
  time_t sending_time;                  // когда сообщение было отправлено
} message_info_t;

// Собрать данные пользователя и подключить разделяемую память
user_info_t GetUserData(char** argv, int* error_code);

// Создать/открыть сегмент (ftok -> shmget -> shmat) и подключить его
chat_shm_t* AttachSharedMemory(const char* key_path, int* shm_id, int* error_code);

// Цикл читателя: печатает новые сообщения, адресованные текущему пользователю
int ReadingData(user_info_t* user_info);

// Цикл писателя: читает stdin и отправляет сообщения в разделяемую память
int SaveData(user_info_t* user_info);

// Разобрать строку вида "Pavel: текст сообщения"
message_info_t ParseLine(char* line, ssize_t len);

// Записать одно сообщение в следующий слот кольцевого буфера под спинлоком
void SendMessage(user_info_t* user_info, message_info_t* tokens);

// Освободить временные строки, выделенные в ParseLine
void FreeTokens(message_info_t* tokens);

// Пройти слоты [begin_index, end_index) и напечатать сообщения для current_user
void ProcessNewMessages(chat_shm_t* chat, uint64_t begin_index,
                        uint64_t end_index, const char* current_user);

#endif
