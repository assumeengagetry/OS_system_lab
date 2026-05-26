//
// Created by assumeengage on 2026/5/26.
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>

/* ===== 颜色输出 ===== */
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"
#define RESET  "\033[0m"

/* ===== 测试框架宏 ===== */
static int pass_count = 0;
static int fail_count = 0;

#define TEST(name) void test_##name()
#define RUN(name) do { \
    printf(CYAN "[ RUN ] " RESET "%s\n", #name); \
    test_##name(); \
} while(0)

#define CHECK(expr) do { \
    if (expr) { \
        printf(GREEN "  [PASS] " RESET "%s\n", #expr); \
        pass_count++; \
    } else { \
        printf(RED "  [FAIL] " RESET "%s  (line %d)\n", #expr, __LINE__); \
        fail_count++; \
    } \
} while(0)

/* ===== 声明被测函数 ===== */
void *my_malloc(size_t size);
void  my_free(void *ptr);

/* ===== TEST 1: 基本分配与非 NULL ===== */
TEST(basic_alloc) {
    void *p1 = my_malloc(8);
    void *p2 = my_malloc(64);
    void *p3 = my_malloc(1);
    CHECK(p1 != NULL);
    CHECK(p2 != NULL);
    CHECK(p3 != NULL);
    my_free(p1);
    my_free(p2);
    my_free(p3);
}

/* ===== TEST 2: 边界输入 ===== */
TEST(edge_input) {
    /* size=0 应返回 NULL（按照代码中 size<=0 的判断） */
    void *p = my_malloc(0);
    CHECK(p == NULL);

    /* free(NULL) 不应崩溃 */
    my_free(NULL);
    CHECK(1); /* 能走到这里说明没崩 */
}

/* ===== TEST 3: 写入后读回一致性 ===== */
TEST(read_write) {
    char *buf = (char *)my_malloc(128);
    CHECK(buf != NULL);
    memset(buf, 0xAB, 128);
    int ok = 1;
    for (int i = 0; i < 128; i++)
        if ((unsigned char)buf[i] != 0xAB) { ok = 0; break; }
    CHECK(ok);

    strcpy(buf, "hello, my_malloc!");
    CHECK(strcmp(buf, "hello, my_malloc!") == 0);
    my_free(buf);
}

/* ===== TEST 4: 多块互不重叠 ===== */
TEST(no_overlap) {
    int N = 8;
    void *ptrs[8];
    size_t sizes[8] = {8, 16, 32, 64, 128, 7, 3, 200};

    for (int i = 0; i < N; i++) {
        ptrs[i] = my_malloc(sizes[i]);
        CHECK(ptrs[i] != NULL);
        memset(ptrs[i], i + 1, sizes[i]);
    }

    /* 检查每块数据未被破坏 */
    int ok = 1;
    for (int i = 0; i < N; i++) {
        unsigned char *p = (unsigned char *)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++)
            if (p[j] != (unsigned char)(i + 1)) { ok = 0; break; }
    }
    CHECK(ok);

    /* 检查地址范围不交叉（任意两块） */
    int no_overlap = 1;
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++) {
            uintptr_t a0 = (uintptr_t)ptrs[i], a1 = a0 + sizes[i];
            uintptr_t b0 = (uintptr_t)ptrs[j], b1 = b0 + sizes[j];
            if (!(a1 <= b0 || b1 <= a0)) { no_overlap = 0; }
        }
    CHECK(no_overlap);

    for (int i = 0; i < N; i++) my_free(ptrs[i]);
}

/* ===== TEST 5: free 后块可被复用（链表回收） ===== */
TEST(reuse_after_free) {
    void *p1 = my_malloc(64);
    CHECK(p1 != NULL);
    uintptr_t addr1 = (uintptr_t)p1;
    my_free(p1);

    /* 再申请相同大小，应复用刚释放的块（First-Fit） */
    void *p2 = my_malloc(64);
    CHECK(p2 != NULL);
    uintptr_t addr2 = (uintptr_t)p2;
    CHECK(addr2 == addr1);   /* 地址相同说明确实复用了 */
    my_free(p2);
}

/* ===== TEST 6: free 后块可被更小的请求复用（split_block） ===== */
TEST(split_block) {
    void *big = my_malloc(256);
    CHECK(big != NULL);
    uintptr_t big_addr = (uintptr_t)big;
    my_free(big);

    /* 申请小块，应从刚释放的大块中切出 */
    void *small = my_malloc(32);
    CHECK(small != NULL);
    CHECK((uintptr_t)small == big_addr);

    /* 剩余部分也能被分配（split 成功的话） */
    void *rest = my_malloc(64);
    CHECK(rest != NULL);
    /* rest 应在 small 之后（同一个大块切出来的） */
    CHECK((uintptr_t)rest > big_addr);

    my_free(small);
    my_free(rest);
}

/* ===== TEST 7: 大量小对象——压力测试 ===== */
TEST(stress_small) {
    #define STRESS_N 512
    void *ptrs[STRESS_N];
    int all_ok = 1;

    for (int i = 0; i < STRESS_N; i++) {
        ptrs[i] = my_malloc(16);
        if (!ptrs[i]) { all_ok = 0; break; }
        *(int *)ptrs[i] = i;
    }
    CHECK(all_ok);

    /* 读回验证 */
    int read_ok = 1;
    for (int i = 0; i < STRESS_N; i++)
        if (*(int *)ptrs[i] != i) { read_ok = 0; break; }
    CHECK(read_ok);

    for (int i = 0; i < STRESS_N; i++) my_free(ptrs[i]);
    #undef STRESS_N
}

/* ===== TEST 8: 交错 malloc/free（碎片场景） ===== */
TEST(interleaved_alloc_free) {
    void *a = my_malloc(32);
    void *b = my_malloc(32);
    void *c = my_malloc(32);
    CHECK(a && b && c);

    my_free(b);           /* 中间块释放，产生碎片 */
    void *d = my_malloc(32);
    CHECK(d != NULL);
    /* d 应复用 b 的位置 */
    CHECK((uintptr_t)d == (uintptr_t)b);

    my_free(a);
    my_free(c);
    my_free(d);
}

/* ===== TEST 9: 对齐检查 ===== */
TEST(alignment) {
    int ok = 1;
    for (size_t s = 1; s <= 512; s *= 2) {
        void *p = my_malloc(s);
        if (!p) { ok = 0; break; }
        /* 至少 8 字节对齐 */
        if ((uintptr_t)p % 8 != 0) { ok = 0; my_free(p); break; }
        my_free(p);
    }
    CHECK(ok);
}

/* ===== TEST 10: double free 后链表完整性 ===== */
TEST(free_then_alloc_chain) {
    /* 释放所有块后重新分配，验证链表仍可用 */
    void *p[4];
    for (int i = 0; i < 4; i++) p[i] = my_malloc(48);
    for (int i = 0; i < 4; i++) my_free(p[i]);

    void *q = my_malloc(48);
    CHECK(q != NULL);
    memset(q, 0x5A, 48);
    unsigned char *b = (unsigned char *)q;
    int ok = 1;
    for (int i = 0; i < 48; i++) if (b[i] != 0x5A) { ok = 0; break; }
    CHECK(ok);
    my_free(q);
}

/* ===== 主入口 ===== */
int main(void) {
    printf("\n");
    printf("══════════════════════════════════════\n");
    printf("       my_malloc 测试套件\n");
    printf("══════════════════════════════════════\n\n");

    RUN(basic_alloc);
    RUN(edge_input);
    RUN(read_write);
    RUN(no_overlap);
    RUN(reuse_after_free);
    RUN(split_block);
    RUN(stress_small);
    RUN(interleaved_alloc_free);
    RUN(alignment);
    RUN(free_then_alloc_chain);

    printf("\n══════════════════════════════════════\n");
    printf("结果: " GREEN "%d passed" RESET "  " RED "%d failed" RESET "\n",
           pass_count, fail_count);
    printf("══════════════════════════════════════\n\n");
    return fail_count > 0 ? 1 : 0;
}