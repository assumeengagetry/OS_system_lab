#include <unistd.h>
#include <string.h>
#include <stdio.h>

typedef struct block_meta {
    size_t size;
    int free;              /* 空闲为 1，占用为 0 */
    struct block_meta *next;
} block_meta;

#define META_SIZE sizeof(block_meta)

void *global_base = NULL;

/* ===== 1. 寻找空闲块 ===== */
block_meta *find_free_block(block_meta **last, size_t size) {
    block_meta *current =(block_meta *)global_base;
    /* TODO 1: First-Fit 遍历，同时维护 *last 指向链表尾部 */
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

/* ===== 2. 向 OS 申请新堆空间 ===== */
block_meta *request_space(block_meta *last, size_t size) {
    block_meta *block;
    /* TODO 2: sbrk(0) 获取当前堆顶作为新块起始地址 */
    block = (block_meta *)sbrk(0);
    /* TODO 3: sbrk 扩展堆，申请失败时返回 NULL */
    void *request = sbrk(META_SIZE + size);
    if (request == (void *)-1)
        return NULL;
    /* TODO 4: 初始化元数据 */
    block->size = size;
    block->free = 0;
    block->next = NULL;
    /* TODO 5: 将新块接到链表尾部 */
    if (last)
        last->next = block;
    return block;
}

/* ===== 3. my_malloc ===== */
void *my_malloc(size_t size) {
    if (size <= 0) return NULL;
    block_meta *block;
    if (!global_base) {
        /* TODO 6: 第一次分配，初始化 global_base */
        block = request_space(NULL, size);
        if (!block) return NULL;
        global_base = block;
    } else {
        block_meta *last = (block_meta *)global_base;
        /* TODO 7: 尝试复用空闲块，找不到时扩容 */
        block = find_free_block(&last, size);
        if (!block) {
            block = request_space(last, size);
            if (!block) return NULL;
        } else {
            block->free = 0;   /* 标记为占用 */
        }
    }
    /* TODO 8: 跳过元数据头部，返回用户数据区指针 */
    return (block + 1);
}

/* ===== 4. my_free ===== */
void my_free(void *ptr) {
    if (!ptr) return;
    /* TODO 9: 反推元数据地址，标记为空闲 */
    block_meta *block = (block_meta *)ptr - 1;
    block->free = 1;
}

