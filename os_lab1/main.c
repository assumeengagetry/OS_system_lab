#include <stdio.h>
#include "sys_info.h"

int main() {
    printf("--- OS Lab 1: System Monitor ---\n");
    print_kernel_version();
    print_username();   // 补全的调用
    return 0;
}
