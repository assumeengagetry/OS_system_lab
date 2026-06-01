#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_VPAGES 8
#define MAX_RESIDENT 4

typedef enum {
    PAGE_EMPTY = 0,
    PAGE_RESIDENT,
    PAGE_SWAPPED
} page_state_t;

typedef struct {
    page_state_t state;
    int dirty;
    unsigned long load_time;
} page_entry_t;

static size_t page_size;
static char *vm_base;
static page_entry_t page_table[NUM_VPAGES];
static int resident_count;
static unsigned long clock_tick;
static int swap_fd = -1;

static void init_vmem(void);
static void destroy_vmem(void);
static void install_handler(void);
static void segv_handler(int sig, siginfo_t *info, void *context);
static void load_page(int page_no);
static void evict_page(int victim);
static int choose_victim_fifo(void);
static int page_of_addr(void *addr);
static void *page_start(int page_no);
static void full_write(int fd, const void *buf, size_t nbyte);
static void full_read(int fd, void *buf, size_t nbyte);

int main(void) {
    init_vmem();

    printf("mini_vmem start: %d virtual pages, only %d resident pages allowed\n",
           NUM_VPAGES, MAX_RESIDENT);

    for (int i = 0; i < NUM_VPAGES; i++) {
        char *p = vm_base + (size_t)i * page_size;
        printf("\nwrite page %d = %c\n", i, (char)('A' + i));
        p[0] = (char)('A' + i);
    }

    printf("\nrevisit page 0, should trigger swap-in if it was evicted\n");
    printf("page 0 value = %c\n", vm_base[0]);

    destroy_vmem();
    return 0;
}

static void init_vmem(void) {
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) {
        perror("sysconf(_SC_PAGESIZE)");
        exit(1);
    }
    page_size = (size_t)ps;

    vm_base = mmap(NULL, page_size * NUM_VPAGES, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (vm_base == MAP_FAILED) {
        perror("mmap vm");
        exit(1);
    }

    swap_fd = open("mini_vmem.swap", O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (swap_fd == -1) {
        perror("open swap");
        exit(1);
    }

    if (ftruncate(swap_fd, (off_t)(page_size * NUM_VPAGES)) == -1) {
        perror("ftruncate swap");
        exit(1);
    }

    install_handler();
}

static void install_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segv_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

static void segv_handler(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;

    int page_no = page_of_addr(info->si_addr);
    if (page_no < 0) {
        fprintf(stderr, "invalid access at %p\n", info->si_addr);
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
    }

    printf("[fault] addr=%p page=%d\n", info->si_addr, page_no);
    load_page(page_no);
}

static int page_of_addr(void *addr) {
    uintptr_t start = (uintptr_t)vm_base;
    uintptr_t end = start + (uintptr_t)(NUM_VPAGES * page_size);
    uintptr_t target = (uintptr_t)addr;

    if (target < start || target >= end) {
        return -1;
    }
    return (int)((target - start) / page_size);
}

static void load_page(int page_no) {
    void *addr = page_start(page_no);
    off_t offset = (off_t)page_no * (off_t)page_size;

    if (resident_count >= MAX_RESIDENT) {
        int victim = choose_victim_fifo();
        if (victim < 0) {
            fprintf(stderr, "no page can be evicted\n");
            exit(1);
        }
        evict_page(victim);
    }

    if (mprotect(addr, page_size, PROT_READ | PROT_WRITE) == -1) {
        perror("mprotect load");
        exit(1);
    }

    if (page_table[page_no].state == PAGE_SWAPPED) {
        if (lseek(swap_fd, offset, SEEK_SET) == (off_t)-1) {
            perror("lseek swap in");
            exit(1);
        }
        full_read(swap_fd, addr, page_size);
        printf("[load] page %d <- swap offset %ld\n", page_no, (long)offset);
    } else {
        memset(addr, 0, page_size);
        printf("[load] page %d <- zero page\n", page_no);
    }

    page_table[page_no].state = PAGE_RESIDENT;
    page_table[page_no].dirty = 1;
    page_table[page_no].load_time = ++clock_tick;
    resident_count++;
}

static void *page_start(int page_no) {
    return vm_base + (size_t)page_no * page_size;
}

static int choose_victim_fifo(void) {
    int victim = -1;
    unsigned long oldest = ULONG_MAX;

    for (int i = 0; i < NUM_VPAGES; i++) {
        if (page_table[i].state == PAGE_RESIDENT &&
            page_table[i].load_time < oldest) {
            oldest = page_table[i].load_time;
            victim = i;
        }
    }

    return victim;
}

static void evict_page(int victim) {
    void *addr = page_start(victim);
    off_t offset = (off_t)victim * (off_t)page_size;

    if (page_table[victim].dirty) {
        if (lseek(swap_fd, offset, SEEK_SET) == (off_t)-1) {
            perror("lseek swap out");
            exit(1);
        }
        full_write(swap_fd, addr, page_size);
    }

    if (mprotect(addr, page_size, PROT_NONE) == -1) {
        perror("mprotect evict");
        exit(1);
    }

    page_table[victim].state = PAGE_SWAPPED;
    page_table[victim].dirty = 0;
    page_table[victim].load_time = 0;
    resident_count--;

    printf("[evict] page %d -> swap offset %ld\n", victim, (long)offset);
}

static void full_write(int fd, const void *buf, size_t nbyte) {
    const char *p = buf;
    size_t done = 0;

    while (done < nbyte) {
        ssize_t n = write(fd, p + done, nbyte - done);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("write swap");
            exit(1);
        }
        done += (size_t)n;
    }
}

static void full_read(int fd, void *buf, size_t nbyte) {
    char *p = buf;
    size_t done = 0;

    while (done < nbyte) {
        ssize_t n = read(fd, p + done, nbyte - done);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("read swap");
            exit(1);
        }
        if (n == 0) {
            fprintf(stderr, "unexpected EOF while reading swap\n");
            exit(1);
        }
        done += (size_t)n;
    }
}

static void destroy_vmem(void) {
    if (vm_base != NULL) {
        munmap(vm_base, page_size * NUM_VPAGES);
    }
    if (swap_fd != -1) {
        close(swap_fd);
    }
    unlink("mini_vmem.swap");
}
