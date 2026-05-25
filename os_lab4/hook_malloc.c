#define _GNU_SOURCE

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/*
 * 必须和 my_malloc.c 中的 block_meta 保持完全一致。
 * 因为 realloc 需要通过用户指针 ptr 反推出 block_meta，
 * 然后读取 block->size，知道旧块有多大。
 */
typedef struct block_meta {
  size_t size;
  int free; // 空闲为 1，占用为 0
  struct block_meta *next;
} block_meta;

/*
 * 声明你在 my_malloc.c 中已经实现好的函数。
 */
void *my_malloc(size_t size);
void my_free(void *ptr);

/*
 * 互斥锁，防止多线程同时进入分配器。
 */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * 拦截系统 malloc。
 */
void *malloc(size_t size) {
  const char msg[] = "Haha! I intercepted malloc!\n";

  write(STDOUT_FILENO, msg, sizeof(msg) - 1);

  pthread_mutex_lock(&lock);

  void *ptr = my_malloc(size);

  pthread_mutex_unlock(&lock);

  return ptr;
}

/*
 * 拦截系统 free。
 */
void free(void *ptr) {
  const char msg[] = "Haha! I intercepted free!\n";

  write(STDOUT_FILENO, msg, sizeof(msg) - 1);

  if (ptr == NULL) {
    return;
  }

  pthread_mutex_lock(&lock);

  my_free(ptr);

  pthread_mutex_unlock(&lock);
}

/*
 * 拦截系统 calloc。
 *
 * calloc 的语义：
 * 1. 分配 nmemb * size 字节
 * 2. 将分配到的内存清零
 */
void *calloc(size_t nmemb, size_t size) {

  /*
   * 防止 nmemb * size 溢出
   */
  if (nmemb != 0 && size > SIZE_MAX / nmemb) {
    return NULL;
  }

  size_t total = nmemb * size;

  pthread_mutex_lock(&lock);

  void *ptr = my_malloc(total);

  pthread_mutex_unlock(&lock);

  if (ptr != NULL) {
    memset(ptr, 0, total);
  }

  const char msg[] = "Haha! I intercepted calloc!\n";

  write(STDOUT_FILENO, msg, sizeof(msg) - 1);

  return ptr;
}

/*
 * 拦截系统 realloc。
 *
 * realloc 的语义：
 * realloc(NULL, size) 等价于 malloc(size)
 * realloc(ptr, 0) 等价于 free(ptr)
 */
void *realloc(void *ptr, size_t size) {

  const char msg[] = "Haha! I intercepted realloc!\n";

  write(STDOUT_FILENO, msg, sizeof(msg) - 1);

  /*
   * realloc(NULL, size) == malloc(size)
   */
  if (ptr == NULL) {
    return malloc(size);
  }

  /*
   * realloc(ptr, 0) == free(ptr)
   */
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  /*
   * 通过用户指针反推出 block_meta
   */
  block_meta *block = (block_meta *)ptr - 1;

  /*
   * 如果旧块已经够大，直接复用
   */
  if (block->size >= size) {
    return ptr;
  }

  /*
   * 旧块不够大，申请新块
   */
  void *new_ptr = malloc(size);

  if (new_ptr == NULL) {
    return NULL;
  }

  /*
   * 拷贝旧数据
   */
  memcpy(new_ptr, ptr, block->size);

  /*
   * 释放旧块
   */
  free(ptr);

  return new_ptr;
}
