#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "sys_info.h"

void print_kernel_version() {
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        printf("Kernel Version: %s\n", buffer.release);
    }
}

void print_username() {
    char *user = getenv("USER");
    if (user == NULL) user = getlogin();
    printf("Current User: %s\n", user ? user : "unknown");
}
