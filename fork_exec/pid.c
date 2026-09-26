#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


int main() {
  
  pid_t current_pid = getpid();
  pid_t parent_pid = getppid();

  printf("current pid: %d\nparent pid: %d\n", current_pid, parent_pid);

}
