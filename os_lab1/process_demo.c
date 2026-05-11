#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        printf("I am child process, PID: %d\n", getpid());
    } else {
        printf("I am parent process, waiting for child %d\n", pid);
        wait(NULL);
        printf("Child finished.\n");
    }
    return 0;
}
