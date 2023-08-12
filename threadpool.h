#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

typedef void (*Handler)(void*);
typedef struct __thread_pool ThreadPool;

ThreadPool* threadPoolCreate(int corePoolSize, int maximumPoolSize, int taskQueueCapacity); 
void threadPoolAddTask(ThreadPool* pool, Handler handle, void* arg);
int threadPoolAliveNum(ThreadPool* pool);
int threadPoolBusyNum(ThreadPool* pool);
int threadPoolDestroy(ThreadPool* pool);

#endif
