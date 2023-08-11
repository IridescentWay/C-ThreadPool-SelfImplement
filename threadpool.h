#include <pthread.h>

#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

typedef struct __thread_pool ThreadPool;

ThreadPool* threadPoolCreate(int corePoolSize, int maximumPoolSize, int taskQueueCapacity); 

#endif
