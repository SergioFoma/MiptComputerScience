#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "extra.h"

// ================== SAVING RULES OF USERS DATA =================
// [time][recipient_name][sender_name][sender_id]<message>
// ===============================================================

const int MOD = 0644;                 // mod in function open

int main(int argc, char** argv) {
  
  if (argc != 3) {
    printf("argc: Incorrect number of arguments\n");
    return 0;
  }
  
  int error_code = 0;
 
  if (error_code != 0) {
    printf("GetUserData: Error!\n");
    return 0;
  }

  pid_t current_pid = fork();
  
  if (current_pid == -1) {
    printf("fork(): Error!\n");
  } else if (current_pid == 0) {
    user_info_t user_info = GetUserData(argv, &error_code);
    error_code = ReadingData(&user_info);
  } else {
    user_info_t user_info = GetUserData(argv, &error_code);
    error_code = SaveData(&user_info);
  }
    
  return 0;
}

user_info_t GetUserData(char** argv, int* error_code) {
  assert(argv);
  assert(error_code);

  user_info_t user_info = {};

  user_info.sender_name = argv[1];
  user_info.sender_id = getpid();
  user_info.file_fd = OpenFile(argv[2], error_code);

  return user_info;
}

int OpenFile(const char* file_path, int* error_code) {
  assert(file_path);
  assert(error_code);
  
  int file_fd = open(file_path, O_RDWR | O_CREAT | O_APPEND, MOD);

  if (file_fd == -1) {
    printf("open(): Error!\n");
    *error_code = -1;
  }

  return file_fd;
}

int ReadingData(user_info_t* user_info) {
  assert(user_info);
    
  int time_sleep = 3;
  time_t last_read_time = 0;

  while (1) {
    size_t file_sz = 0;
    
    char* buf = LoadFileToBuffer(user_info->file_fd, &file_sz);
    
    if (!buf) {
      sleep(time_sleep);
      continue; 
    }

    // find last message
    time_t max_time_in_file = ProcessNewMessages(buf, last_read_time, user_info->sender_name);

    // clear old messages
    CleanOldMessages(user_info->file_fd, buf, last_read_time);

    // update last message
    last_read_time = max_time_in_file;

    free(buf);
    sleep(time_sleep);
  }

  return 0;
}

char* LoadFileToBuffer(int file_fd, size_t* out_file_sz) {
  assert(out_file_sz);

  lseek(file_fd, 0, SEEK_SET);
  size_t file_sz = GetFileSize(file_fd);
  *out_file_sz = file_sz;

  if (file_sz == 0) {
    return NULL;
  }

  char* buf = (char*)calloc(file_sz + 1, sizeof(char));
  if (!buf) {
    return NULL;
  }

  ssize_t res = read(file_fd, buf, file_sz);
  if (res <= 0) {
    free(buf);
    return NULL;
  }

  buf[res] = '\0';
  return buf;
}

time_t ProcessNewMessages(const char* buf, time_t last_read_time, const char* current_user) {
  assert(buf);
  assert(current_user);

  const char* chunk_begin = buf;
  time_t max_time_in_file = last_read_time;

  while (chunk_begin && *chunk_begin != '\0') {
    if (strchr(chunk_begin, '[') && strchr(chunk_begin, '<')) {

      message_info_t tokens = ScanMetadata((char*)chunk_begin);

      if (tokens.sending_time > last_read_time) {
        if (tokens.recipient_name && strcmp(current_user, tokens.recipient_name) == 0) {
          printf("%s\n", tokens.message);
          fflush(stdout);
        }
        
        if (tokens.sending_time > max_time_in_file) {
          max_time_in_file = tokens.sending_time;
        }
      }
      FreeTokens(&tokens);
    }
    chunk_begin = strchr(chunk_begin, '\n');
    if (chunk_begin) ++chunk_begin;
  }

  return max_time_in_file;
}

void CleanOldMessages(int file_fd, const char* buf, time_t last_read_time) {
  assert(buf);

  lseek(file_fd, 0, SEEK_SET);
  
  const char* chunk_begin = buf;
  off_t new_file_size = 0;

  while (chunk_begin && *chunk_begin != '\0') {
    const char* next_chunk = strchr(chunk_begin, '\n');
    size_t line_len = next_chunk ? (size_t)(next_chunk - chunk_begin + 1) : strlen(chunk_begin);

    if (strchr(chunk_begin, '[') && strchr(chunk_begin, '<')) {
      message_info_t tokens = ScanMetadata((char*)chunk_begin);

      if (tokens.sending_time > last_read_time) {
        write(file_fd, chunk_begin, line_len);
        new_file_size += line_len;
      }
      FreeTokens(&tokens);
    }
    
    chunk_begin = next_chunk ? next_chunk + 1 : NULL;
  }

  // Обрезаем файл под новый размер оставшихся сообщений
  ftruncate(file_fd, new_file_size);
}

size_t GetFileSize(int file_fd) {
  struct stat st = {};

  if (fstat(file_fd, &st) == -1) {
    printf("fstat(): error!\n");
    return 0;
  }

  return st.st_size;
}

void ScanMessHistory(size_t file_sz, char* buf, user_info_t* user_info) {
  assert(buf);
  assert(user_info);
  
  const char* user_name = user_info->sender_name;

  char* chunk_begin = buf;
   
  while (chunk_begin && strchr(chunk_begin, '[')) {
    message_info_t tokens = ScanMetadata(chunk_begin);
    

    if (strlen(user_name) == strlen(tokens.recipient_name) && 
        strcmp(user_name, tokens.recipient_name) == 0) {
      printf("%s\n", tokens.message);
      fflush(stdout);
    }

    chunk_begin = strchr(chunk_begin, '\n');
    if (chunk_begin) ++chunk_begin;

    FreeTokens(&tokens);
  }
}

message_info_t ScanMetadata(char* line) {
  assert(line);

  char* front_bracket = strchr(line, '[');
  char* back_bracket = strchr(front_bracket, ']');
  int time_mes = 0;

  ++front_bracket;
  while (front_bracket < back_bracket) {
    time_mes = time_mes * 10 + (*front_bracket - '0');
    ++front_bracket;
  }
  
  front_bracket = strchr(back_bracket, '[');
  ++front_bracket;
  back_bracket = strchr(front_bracket, ']');
  char* name = strndup(front_bracket, back_bracket - front_bracket);

  front_bracket = strchr(line, '<');
  ++front_bracket;
  back_bracket = strchr(front_bracket, '>');
  char* message = strndup(front_bracket, back_bracket - front_bracket);
  
  message_info_t info =  {name, message, time_mes};
  return info;
}

int SaveData(user_info_t* user_info) {
  assert(user_info);
  
  char* line = NULL;
  size_t cap = 0;
  ssize_t result = 0;
  int chunk_sz = 0;

  while ((result = getline(&line, &cap, stdin)) > 0) {
    if (line[result - 1] == '\n') {
      line[result - 1] = '\0';
    }

    message_info_t tokens = ParseLine(line, result);
  
    char* chunk = CreateChunk(user_info, &tokens, &chunk_sz);

    write(user_info->file_fd, chunk, chunk_sz);
    //printf("success write chunk: %s\n", chunk);
    
    if (chunk) free(chunk);
    FreeTokens(&tokens);
  }

  return 0;
}

message_info_t ParseLine(char* line, ssize_t len) {
  assert(line);
  
  // pattern = 'Pavel: Hello, Pavel. What do you do?'

  message_info_t info = {};
  
  char* colon = strchr(line, ':');
  char* whitespace = strchr(line, ' '); 

  info.sending_time = time(NULL);
  info.recipient_name = strndup(line, colon - line);
  info.message = strdup(whitespace + 1);
  
  return info;
}

char* CreateChunk(user_info_t* user_info, message_info_t* tokens, int* chunk_sz) {
  assert(user_info);
  assert(tokens);
  assert(chunk_sz);
  
  char* chunk = (char*)0;
  *chunk_sz = 0;
  
  size_t sz = 0;
  int buf_len = snprintf(chunk, sz, "[%ld][%s][%s][%d]<%s>\n",
                         tokens->sending_time, tokens->recipient_name,
                         user_info->sender_name, user_info->sender_id, tokens->message);

  chunk = (char*)calloc(buf_len + 1, sizeof(char));
  if (!chunk) {
    return NULL;
  }
  
  // snprintf write (capacity - 1) символов
  *chunk_sz = snprintf(chunk, buf_len + 1, "[%ld][%s][%s][%d]<%s>\n",
                    tokens->sending_time, tokens->recipient_name,
                    user_info->sender_name, user_info->sender_id, tokens->message);

  if (*chunk_sz != buf_len) {
    return NULL;
  }

  return chunk;
}

void FreeTokens(message_info_t* tokens) {
  assert(tokens);

  free(tokens->recipient_name);
  free(tokens->message);
}
