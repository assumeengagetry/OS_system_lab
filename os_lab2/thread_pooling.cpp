#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

// =========================
// 任务结构体
// =========================
struct Task {
    void (*function)(void *);
    void *argument;
    Task *next;
};

// =========================
// 线程池结构体
// =========================
struct ThreadPool {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    Task *task_queue_head;
    Task *task_queue_tail;
    int thread_count;
    int shutdown;
};

// =========================
// Worker 线程函数
// =========================
void *thread_pool_worker(void *arg) {
    ThreadPool *pool = static_cast<ThreadPool *>(arg);

    while (1) {
        pthread_mutex_lock(&pool->lock);

        // 队列为空且线程池未关闭时，进入等待
        while (pool->task_queue_head == nullptr && !pool->shutdown) {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }

        // 如果线程池已关闭且任务队列为空，则退出线程
        if (pool->shutdown && pool->task_queue_head == nullptr) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(nullptr);
        }

        // 从队头取任务
        Task *task = nullptr;
        if (pool->task_queue_head != nullptr) {
            task = pool->task_queue_head;
            pool->task_queue_head = task->next;

            // 如果取完后队列为空，tail 也要同步清空
            if (pool->task_queue_head == nullptr) {
                pool->task_queue_tail = nullptr;
            }
        }

        // 立刻解锁，避免拿着锁执行耗时任务
        pthread_mutex_unlock(&pool->lock);

        // 在锁外执行任务
        if (task != nullptr) {
            task->function(task->argument);
            std::free(task);
        }
    }

    return nullptr;
}

// =========================
// 提交任务
// 成功返回 0，失败返回 -1
// =========================
int thread_pool_submit(ThreadPool *pool, void (*function)(void *), void *argument) {
    if (pool == nullptr || function == nullptr) {
        return -1;
    }

    Task *task = static_cast<Task *>(std::malloc(sizeof(Task)));
    if (task == nullptr) {
        return -1;
    }

    task->function = function;
    task->argument = argument;
    task->next = nullptr;

    pthread_mutex_lock(&pool->lock);

    // 持锁检查 shutdown，防止并发关闭时错误入队
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        std::free(task);
        return -1;
    }

    // 尾插入队
    if (pool->task_queue_head == nullptr) {
        pool->task_queue_head = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }

    // 唤醒一个等待中的 worker
    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}

// =========================
// 销毁线程池：优雅退出
// =========================
void thread_pool_destroy(ThreadPool *pool) {
    if (pool == nullptr) {
        return;
    }

    pthread_mutex_lock(&pool->lock);

    // 避免重复销毁
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        return;
    }

    // 标记关闭
    pool->shutdown = 1;

    // 唤醒所有正在等待的线程
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    // 必须在解锁后 join，避免死锁
    for (int i = 0; i < pool->thread_count; ++i) {
        pthread_join(pool->threads[i], nullptr);
    }

    // 清理资源
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);
    std::free(pool->threads);
    std::free(pool);
}

// =========================
// 测试任务
// =========================
void my_task(void *arg) {
    int id = *static_cast<int *>(arg);
    std::printf("Thread %lu 正在执行任务 %d...\n",
                (unsigned long)pthread_self(), id);
    usleep(100000); // 0.1 秒
    std::free(arg); // 释放任务参数
}

// =========================
// main：实验基础验证
// =========================
int main() {
    ThreadPool *pool = static_cast<ThreadPool *>(std::malloc(sizeof(ThreadPool)));
    if (pool == nullptr) {
        std::fprintf(stderr, "分配线程池失败\n");
        return 1;
    }

    pool->thread_count = 4;
    pool->shutdown = 0;
    pool->task_queue_head = nullptr;
    pool->task_queue_tail = nullptr;

    if (pthread_mutex_init(&pool->lock, nullptr) != 0) {
        std::fprintf(stderr, "pthread_mutex_init 失败\n");
        std::free(pool);
        return 1;
    }

    if (pthread_cond_init(&pool->notify, nullptr) != 0) {
        std::fprintf(stderr, "pthread_cond_init 失败\n");
        pthread_mutex_destroy(&pool->lock);
        std::free(pool);
        return 1;
    }

    pool->threads = static_cast<pthread_t *>(
        std::malloc(sizeof(pthread_t) * pool->thread_count));
    if (pool->threads == nullptr) {
        std::fprintf(stderr, "分配线程数组失败\n");
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        std::free(pool);
        return 1;
    }

    for (int i = 0; i < pool->thread_count; ++i) {
        if (pthread_create(&pool->threads[i], nullptr, thread_pool_worker, pool) != 0) {
            std::fprintf(stderr, "创建线程 %d 失败\n", i);

            // 已创建的线程需要回收
            pthread_mutex_lock(&pool->lock);
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->notify);
            pthread_mutex_unlock(&pool->lock);

            for (int j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], nullptr);
            }

            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->notify);
            std::free(pool->threads);
            std::free(pool);
            return 1;
        }
    }

    std::printf("=== 线程池启动，投入 15 个任务 ===\n");
    for (int i = 1; i <= 15; ++i) {
        int *arg = static_cast<int *>(std::malloc(sizeof(int)));
        if (arg == nullptr) {
            std::fprintf(stderr, "任务参数分配失败：%d\n", i);
            continue;
        }

        *arg = i;
        if (thread_pool_submit(pool, my_task, arg) != 0) {
            std::fprintf(stderr, "任务提交失败：%d\n", i);
            std::free(arg);
        }
    }

    thread_pool_destroy(pool);
    return 0;
}
