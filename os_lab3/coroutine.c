#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ucontext.h>

#define MAX_COROUTINES 10
#define STACK_SIZE (1024 * 64)

typedef enum { FREE, RUNNABLE, RUNNING, SUSPEND, SIGNALRM } State;

typedef struct {
    ucontext_t ctx;           // 保存该协程的寄存器现场、栈信息和返回去向
    char stack[STACK_SIZE];   // 该协程独享的栈空间
    State state;              // 协程当前所处状态
    void (*func)(void);       // 协程真正要执行的任务函数
} Coroutine;

struct Scheduler {
    ucontext_t main_ctx;                      // 主调度器上下文，yield/结束后都会切回这里
    Coroutine coroutines[MAX_COROUTINES];     // 协程槽位表
    int current_id;                           // 当前正在运行的协程 ID，-1 表示当前在 main_ctx 中
    int in_scheduler; 
} scheduler;

void haddle_preemptive_scheduling(int sig){
    int id = scheduler.current_id;
    if(id == -1 || scheduler.in_scheduler[id].state == SUSPEND){
        
        
    }





    swapcontext(1 2);
}

void scheduler_init() {
    scheduler.current_id = -1;
    // ★ TODO 1：将所有协程状态初始化为 FREE
    for (int i = 0; i < MAX_COROUTINES; i++) {
        scheduler.coroutines[i].state = FREE;
    }
}

void coroutine_wrapper() {
    int id = scheduler.current_id;

    // ★ TODO 2：调用 scheduler.coroutines[id] 绑定的 func()
    scheduler.coroutines[id].func();

    // ☆ TODO 3：将当前协程状态置为 FREE，并将 current_id 置为 -1
    scheduler.coroutines[id].state = FREE;
    scheduler.current_id = -1;

    // ★ TODO 4：使用 setcontext(&scheduler.main_ctx) 切回主调度器
    setcontext(&scheduler.main_ctx);
}

int coroutine_spawn(void (*func)(void)) {
    int id = -1;
    // ★ TODO 5：遍历槽位，找到一个状态为 FREE 的协程 ID
    // 如果找不到，直接返回 -1
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (scheduler.coroutines[i].state == FREE) {
            id = i;
            break;
        }
    }
    if (id == -1) return -1;

    Coroutine *co = &scheduler.coroutines[id];
    co->state = RUNNABLE;
    co->func = func;

    // getcontext 先把"当前 CPU 上下文"保存到 co->ctx，后面再把它改造成一个新协程入口。
    getcontext(&co->ctx);

    // ★ TODO 6：设置协程独立栈
    co->ctx.uc_stack.ss_sp   = co->stack;
    co->ctx.uc_stack.ss_size = STACK_SIZE;
    co->ctx.uc_stack.ss_flags = 0;

    // ☆ TODO 7：设置 uc_link
    co->ctx.uc_link = &scheduler.main_ctx;

    // ★ TODO 8：使用 makecontext 让协程从 coroutine_wrapper 开始执行
    makecontext(&co->ctx, coroutine_wrapper, 0);

    return id;
}

void coroutine_resume(int id) {
    if (id < 0 || id >= MAX_COROUTINES) return;
    Coroutine *co = &scheduler.coroutines[id];

    // ★ TODO 9：如果状态不是 RUNNABLE 或 SUSPEND，直接 return
    if (co->state != RUNNABLE && co->state != SUSPEND) return;

    co->state = RUNNING;
    scheduler.current_id = id;

    // ★ TODO 10：使用 swapcontext 保存当前(main)上下文并切换到协程上下文
    swapcontext(&scheduler.main_ctx, &co->ctx);
}

void coroutine_yield() {
    int id = scheduler.current_id;
    // ★ TODO 11：如果当前处于调度器环境（id == -1），直接 return
    if (id == -1) return;

    Coroutine *co = &scheduler.coroutines[id];

    // ☆ TODO 12：将当前协程状态改为 SUSPEND，current_id 重置为 -1
    co->state = SUSPEND;
    scheduler.current_id = -1;

    // ★ TODO 13：使用 swapcontext 切换回调度器上下文（main_ctx）
    swapcontext(&co->ctx, &scheduler.main_ctx);
}

void long_task() {
    int total = 0;
    for (int batch = 1; batch <= 5; batch++) {
        for (int i = 0; i < 1000000; i++) {
            total += i % 7;
        }
        printf("[Long Task] finished batch %d, total = %d\n", batch, total);
        coroutine_yield();
    }
    printf("[Long Task] all batches done\n");
}

void monitor_task() {
    for (int round = 1; round <= 5; round++) {
        printf("[Monitor] scheduler is still responsive, round %d\n", round);
        coroutine_yield();
    }
}

int main() {
    scheduler_init();
    coroutine_spawn(long_task);
    coroutine_spawn(monitor_task);

    while (1) {
        int all_done = 1;
        for (int i = 0; i < MAX_COROUTINES; i++) {
            if (scheduler.coroutines[i].state != FREE) {
                all_done = 0;
                coroutine_resume(i);
            }
        }
        if (all_done) break;
    }

    printf("All coroutines finished.\n");
    return 0;
}
