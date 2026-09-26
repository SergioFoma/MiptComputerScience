#ifndef EXTRA_H_
#define EXTRA_H_

#include <sys/types.h>
#include <unistd.h>

typedef struct user_info_s {
  const char* sender_name;            // sender name
  pid_t sender_id;                    // recipient pid
  int file_fd;                        // file descriptor (data base)
} user_info_t;

typedef struct message_info_s {
  char* recipient_name;               // recipient name
  char* message;                      // message from sender
  time_t sending_time;                // when the message was sent
} message_info_t;

user_info_t GetUserData(char** argv, int* error_code);

int OpenFile(const char* file_path, int* error_code);

int ReadingData(user_info_t* user_info);

int SaveData(user_info_t* user_info);

message_info_t ParseLine(char* line, ssize_t len);

char* CreateChunk(user_info_t* user_info, message_info_t* tokens, int* chunk_sz);

void FreeTokens(message_info_t* tokens);

message_info_t ScanMetadata(char* line);

size_t GetFileSize(int file_fd);

void ScanMessHistory(size_t file_sz, char* buf, user_info_t* user_info);

#endif
