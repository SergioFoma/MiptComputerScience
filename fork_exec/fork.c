#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  
  int a = 0;
  pid_t pid = 0,  ppid = 0;
  pid_t current_pid = fork();

  if (current_pid < 0) {
    printf("Error!\n");
  } else if(current_pid == 0) {                             // child
    ++a;
    pid = getpid();
    ppid = getppid();
    printf("pid = %d, ppid = %d, a = %d\n", pid, ppid, a);
  } else {                                                 // parent
    pid = getpid();
    ppid = getppid();

    printf("pid = %d, ppid = %d, a = %d\n", pid, ppid, a);
  }
  
  return 0;
}
