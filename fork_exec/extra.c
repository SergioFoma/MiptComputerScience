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
    
  ssize_t res = 0;
  int time_sleep = 3;

  while (1) {
    lseek(user_info->file_fd, 0, SEEK_SET);

    size_t file_sz = GetFileSize(user_info->file_fd);
  
    if (file_sz == 0) {
      sleep(time_sleep);
      continue;
    }

    char* buf = (char*)calloc(file_sz + 1, sizeof(char));
    if (!buf) {
      return -1;
    }

    res = read(user_info->file_fd, buf, file_sz);
    buf[file_sz] = '\0';

    if (res <= 0) {
      free(buf);
      continue;
    }

    time_t current_time = time(NULL);

    ScanMessHistory(file_sz, buf, user_info);

    free(buf);

    sleep(time_sleep);
  }

  return 0;
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
