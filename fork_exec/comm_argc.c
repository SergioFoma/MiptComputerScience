#include <stdio.h>

int main(int argc, char** arg, char** envp) {

  for (int i = 0; envp[i] != NULL; ++i) {
    printf("end[%d]: %s\n", i, envp[i]);
  }

  return 0;
}
