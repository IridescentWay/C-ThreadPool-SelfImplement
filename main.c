#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "threadpool.h"

void task(void* p_arg) {
    int num = *((int*) p_arg);
    printf("thread %ld is running, taskNum = %d\n", pthread_self(), num);
    sleep(1);
    return ;
}

int main() {
    ThreadPool* pool = threadPoolCreate(3, 10, 100);
    if (pool == NULL) {
        printf("threadpool create failed!\n");
        return -1;
    }
    for (size_t i = 0; i < 120; ++i) {
        int* num = (int*) malloc(sizeof(int));
        *num = i + 1000;
        threadPoolAddTask(pool, task, num);
    }
    sleep(30);
    threadPoolDestroy(pool);
    return 0;
}