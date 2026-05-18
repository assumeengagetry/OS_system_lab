#define _GNU_SOURCE

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>

typedef struct block_meta {
    size_t size;
    int free;
    struct block_meta *next;
} block_meta;

void *my_malloc(size_t size);
void my_free(void *ptr);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *malloc(size_t size) {
    const char msg[] = "Haha! I intercepted malloc!\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    pthread_mutex_lock(&lock);
    void *ptr = my_malloc(size);
    pthread_mutex_unlock(&lock);
    return ptr;
}

void free(void *ptr) {
    const char msg[] = "Haha! I intercepted free!\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    if (ptr == NULL) return;
    pthread_mutex_lock(&lock);
    my_free(ptr);
    pthread_mutex_unlock(&lock);
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) return NULL;
    size_t total = nmemb * size;
    pthread_mutex_lock(&lock);
    void *ptr = my_malloc(total);
    pthread_mutex_unlock(&lock);
    if (ptr != NULL) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    block_meta *block = (block_meta *)ptr - 1;
    if (block->size >= size) return ptr;
    void *new_ptr = malloc(size);
    if (new_ptr == NULL) return NULL;
    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}
