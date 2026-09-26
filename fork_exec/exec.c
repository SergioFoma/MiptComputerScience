#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
  pid_t pid = 0, ppid = 0, fork_res = 0;
  int a = 0, result = 0;

  fork_res = fork();

  if (fork_res == -1) {
    printf("Error!\n");
  } else if (fork_res > 0) {                                  // parent
    ++a;
    pid = getpid();
    ppid = getppid();
  
    printf("pid = %d, ppid = %d, a = %d\n", pid, ppid, a);
    wait(NULL);
  } else {                                                    // child
    result = execl("/bin/cat", "/bin/cat", "fork.c", NULL);   /// выводим содержимое fork.c

    if (result < 0) {
      printf("Execl return negative value\n");
    }
  }

  return 0;
}
