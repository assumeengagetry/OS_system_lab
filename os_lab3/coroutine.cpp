#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define MAX_COROUTINES 10
#define STACK_SIZE (1024 * 64)

typedef enum {
    FREE,
    RUNNABLE,
    RUNNING,
    SUSPEND,
    SLEEPING
} State;

typedef struct {
    ucontext_t ctx;            // 保存该协程的寄存器现场、栈信息和返回去向
    char stack[STACK_SIZE];    // 该协程独享的栈空间
    State state;               // 协程当前状态
    void (*func)(void);        // 协程真正执行的任务函数
    time_t wake_up_time;
} Coroutine;

struct Scheduler {
    ucontext_t main_ctx;                       // 主调度器上下文
    Coroutine coroutines[MAX_COROUTINES];      // 协程槽位表
    int current_id;                            // 当前正在运行的协程 ID，-1 表示在 main_ctx 中

} scheduler;


/*
 * 初始化调度器
 */
void scheduler_init() {
    scheduler.current_id = -1;


    // TODO 1：将所有协程状态初始化为 FREE
    for(int i = 0; i < MAX_COROUTINES; i++){
      scheduler.coroutines[i].state = FREE;
    }
}


/*
 * 协程入口包装函数
 *
 * 作用：
 * 1. 第一次 resume 某个协程时，实际会先进入 coroutine_wrapper。
 * 2. wrapper 再调用真正的业务函数 func。
 * 3. func 执行结束后，wrapper 负责清理协程状态并切回调度器。
 */
void coroutine_wrapper() {
    int id = scheduler.current_id;

    // TODO 2：调用 scheduler.coroutines[id] 绑定的 func()
    scheduler.coroutines[id].func();    

    // TODO 3：func 返回后，将当前协程状态置为 FREE，并将 current_id 置为 -1
    scheduler.coroutines[id].state = FREE;
    scheduler.current_id = -1;

    // TODO 4：使用 setcontext(&scheduler.main_ctx) 切回主调度器
    
setcontext(&scheduler.main_ctx); }


/*
 * 创建一个新协程
 *
 * 返回值：
 * 成功：返回协程 ID
 * 失败：返回 -1
 */
int coroutine_spawn(void (*func)(void)) {
    int id = -1;

    // TODO 5：遍历槽位，找到一个状态为 FREE 的协程 ID
    // 如果找不到，直接返回 -1

    for(int i = 0; i < MAX_COROUTINES; i++){
     if( scheduler.coroutines[i].state == FREE){
       id = i;
       break;
       }
    }
    if(id == -1 ) return -1;
    
    Coroutine *co = &scheduler.coroutines[id];

    co->state = RUNNABLE;
    co->func = func;

    /*
     * getcontext 先把当前 CPU 上下文保存到 co->ctx。
     * 后面我们会把这个上下文改造成一个新的协程上下文。
     */
    getcontext(&co->ctx);

    // TODO 6：设置协程独立栈
    // co->ctx.uc_stack.ss_sp = ...
    // co->ctx.uc_stack.ss_size = ...
    // co->ctx.uc_stack.ss_flags = ...
    
     co->ctx.uc_stack.ss_sp = co->stack;
     co->ctx.uc_stack.ss_size = STACK_SIZE;
     co->ctx.uc_stack.ss_flags = 0;
    
    // TODO 7：设置 uc_link
    // 建议设置为 &scheduler.main_ctx
    co->ctx.uc_link = &scheduler.main_ctx;
    // TODO 8：使用 makecontext 让协程从 coroutine_wrapper 开始执行
    makecontext(&co->ctx,coroutine_wrapper,0);
    return id;
}


/*
 * 恢复/启动某个协程
 */
void coroutine_resume(int id) {
    if (id < 0 || id >= MAX_COROUTINES) {
        return;
    }

    Coroutine *co = &scheduler.coroutines[id];

    // TODO 9：如果状态不是 RUNNABLE 或 SUSPEND，直接 return
    if(co->state!=RUNNABLE && co->state!=SUSPEND) return;
    co->state = RUNNING;
    scheduler.current_id = id;

    // TODO 10：使用 swapcontext 保存 main_ctx，并切换到 co->ctx
    swapcontext(&scheduler.main_ctx,&co->ctx);
}


/*
 * 当前协程主动让出 CPU
 */
void coroutine_yield() {
    int id = scheduler.current_id;

    // TODO 11：如果当前处于调度器环境，也就是 id == -1，直接 return

    if(id == -1) return ;
    Coroutine *co = &scheduler.coroutines[id];

    // TODO 12：将当前协程状态改为 SUSPEND，并将 current_id 重置为 -1
    co->state = SUSPEND;
    scheduler.current_id= -1;
    // TODO 13：使用 swapcontext 保存当前协程上下文，并切换回 scheduler.main_ctx
    swapcontext(&co->ctx, &scheduler.main_ctx);
}


void coroutine_sleep(int seconds){


    int id = scheduler.current_id;
    if(id  == -1) return;
    Coroutine *co = &scheduler.coroutines[id];

    co-> wake_up_time = seconds + time(nullptr);
    co->state = SLEEPING;
    scheduler.current_id = -1;
    
        printf("[Sleep]  协程 %d 睡眠 %d 秒，唤醒时刻 = %ld\n",  id, seconds, (long)co->wake_up_time);

swapcontext(&co->ctx, &scheduler.main_ctx);

}
void task_sleeper() {
    printf("[Sleeper] 第 1 阶段，即将睡眠 2 秒\n");
    coroutine_sleep(2);
    printf("[Sleeper] 第 2 阶段，睡醒了！再睡 1 秒\n");
    coroutine_sleep(1);
    printf("[Sleeper] 全部完成\n");
}

void monitor_task() {
    for (int round = 1; round <= 5; round++) {
        printf("[Monitor] scheduler is still responsive, round %d\n", round);

        // 主动让出 CPU，让 long_task 继续执行
        coroutine_yield();
    }
}


/*
 * 主函数：极简轮转调度器
 */
int main() {
    scheduler_init();

  coroutine_spawn(task_sleeper);
    coroutine_spawn(monitor_task);

    /*
     * 极简 Round-Robin 调度器：
     * 不断扫描协程表，只要发现没有结束的协程，就 resume 它。
     */
    while(1){


        int  all_done = 1;


        for(int i = 0; i < MAX_COROUTINES; i ++){

        State s = scheduler.coroutines[i].state;
        if (s == FREE) continue;

        all_done = 0;

        if(s == SLEEPING){
            if(time(nullptr) >= scheduler.coroutines[i].wake_up_time ){
                scheduler.coroutines[i].state = RUNNABLE;
                coroutine_resume(i);
            }

       continue;
          
        }

        coroutine_resume(i);
        
    }

if(all_done) break;
    } 
         printf("All coroutines finished.\n");
     

    return 0;
}
