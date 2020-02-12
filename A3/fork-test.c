#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

int main(int arg, char** argv) {
  printf("Starting from %d\n", getpid());
  int rc = fork();
  if (rc == 0) {
    printf("Exiting from child and my PID is %d and RC is %d\n", getpid(), rc);
    sleep(3);
    char* my_args[3]; // HOW DO I KNOW THIS?
    my_args[0] = "/bin/ls";
    my_args[1] = "-l";
    my_args[2] = NULL;
    int exec_rc = execv("/bin/ls", my_args);
    printf("Done with exec!!!\n");
  } else {
    int wait_rc = wait(NULL);
    printf("Exiting from parent and my PID is %d and RC is %d\n", getpid(), rc);
  }

  return rc;
}
