#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MB (1024UL * 1024)
#define OBSERVE_SIZE (512UL * MB)
#define PROGRESS_STEP (64UL * MB)
#define PROGRESS_DELAY_SEC 1

static void wait_for_input(void) {
    char line[32];
    if (fgets(line, sizeof(line), stdin) == NULL) {
        exit(0);
    }
}

static void check_pagemap(void *addr) {
    uint64_t value = 0;
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open pagemap failed");
        exit(1);
    }

    uint64_t vpn = (uint64_t)((uintptr_t)addr / page_size);
    off_t offset = (off_t)(vpn * sizeof(uint64_t));

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("lseek failed");
        close(fd);
        exit(1);
    }

    if (read(fd, &value, sizeof(value)) != sizeof(value)) {
        perror("read failed");
        close(fd);
        exit(1);
    }
    close(fd);

    int present = (int)((value >> 63) & 1ULL);
    uint64_t pfn = value & ((1ULL << 55) - 1ULL);

    printf("addr=%p raw=0x%016lx present=%d pfn=0x%lx\n",
           addr, value, present, pfn);
}

int main(void) {
    size_t size = OBSERVE_SIZE;
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);

    printf("vm_observe pid = %d\n", getpid());
    printf("page size = %zu bytes = %zu KB\n", page_size, page_size / 1024);

    printf("\n=== 阶段 0：mmap 前 ===\n");
    printf("[现在观察] 用 sudo htop -p <上方 PID> 记录当前 VIRT 与 RES/RSS。\n");
    printf("[下一步] 按回车后，程序将调用 mmap 申请 %zu MB 虚拟内存。\n", size / MB);
    printf("[预测] VIRT 是否会明显增加？RES/RSS 是否会同步增加？\n");
    wait_for_input();

    printf("准备申请 %zu MB 虚拟内存...\n", size / MB);
    char *ptr = mmap(NULL, size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    printf("mmap 成功，起始地址: %p\n", ptr);

    printf("\n=== 阶段 1：mmap 后，但尚未访问页面 ===\n");
    check_pagemap(ptr);
    printf("[现在观察] 记录当前 VIRT、RES/RSS 和上方 pagemap 输出。\n");
    printf("[下一步] 按回车后，程序将只写入 ptr[0] 所在的首个页面。\n");
    printf("[预测] pagemap 的 present 是否会变化？RES/RSS 是否只小幅变化？\n");
    wait_for_input();

    printf("\n=== 阶段 2：写入首个页面 ===\n");
    ptr[0] = 'X';
    check_pagemap(ptr);
    printf("[现在观察] 记录写入首个页面后的 pagemap、VIRT 和 RES/RSS。\n");
    printf("[下一步] 按回车后，程序将按 page_size 步长逐页写完整段内存。\n");
    printf("[预测] RES/RSS 会随着页面被写入而逐步增长。\n");
    wait_for_input();

    printf("\n=== 阶段 3：逐页写入整段内存 ===\n");
    printf("每写入 %zu MB 暂停 %d 秒，方便在 htop 中观察 RES/RSS 的增长。\n",
           PROGRESS_STEP / MB, PROGRESS_DELAY_SEC);

    for (size_t i = 0; i < size; i += page_size) {
        ptr[i] = 'Y';
        size_t visited = i + page_size;
        if (visited % PROGRESS_STEP == 0 || visited == size) {
            printf("已访问: %zu MB / %zu MB\n", visited / MB, size / MB);
            sleep(PROGRESS_DELAY_SEC);
        }
    }

    printf("[现在观察] 逐页访问完成，请记录释放前的 VIRT 与 RES/RSS。\n");
    printf("[下一步] 按回车后，程序将释放 mmap 区域；重点观察 VIRT/RES 是否下降。\n");
    wait_for_input();

    if (munmap(ptr, size) == -1) {
        perror("munmap failed");
        return 1;
    }

    printf("[现在观察] mmap 区域已释放。请记录释放后的 VIRT 与 RES/RSS。\n");
    printf("[结束] 按回车后退出程序。\n");
    wait_for_input();

    return 0;
}
