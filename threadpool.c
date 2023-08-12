#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


const int ALTER_UNIT = 2;

typedef struct __task {
	Handler handle;
	void* arg;
} Task;

typedef struct __task_queue {
	Task* q;
	int qHead;
	int qTail;
	int size;
	int capacity;
} TaskQueue;

struct __thread_pool {
	TaskQueue taskQueue;
	int corePoolSize;
	int maximumPoolSize;
	pthread_t managerID;
	pthread_t *threadIDs;
	int busyNum;
	int liveNum;
	int exitNum;
	pthread_mutex_t poolLock;
	pthread_mutex_t busynumLock;
	pthread_cond_t notFullCond;
	pthread_cond_t notEmptyCond;

	int shutdown;
};

int taskQueueInit(TaskQueue *tq, int capacity);
int taskQueuePush(TaskQueue *tq, Handler pFunc, void* arg);
Task* taskQueuePop(TaskQueue *tq);
void taskQueueDeInit(TaskQueue *tq);

void* manager(void* p_args);
void* worker(void* p_args);

void threadExit(ThreadPool *pool);

/******** 线程池操作 **********/

ThreadPool* threadPoolCreate(int corePoolSize, int maximumPoolSize, int taskQueueCapacity) {
	ThreadPool* pool = NULL;
	do {
		pool = (ThreadPool*) malloc(sizeof(ThreadPool));
		if (pool == NULL) {
			printf("malloc threadPool failed!\n");
			break;
		}
		pool->threadIDs = (pthread_t*) malloc(sizeof(pthread_t) * maximumPoolSize);
		if (pool->threadIDs == NULL) {
			printf("malloc threadIDs failed!\n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * maximumPoolSize);
		pool->corePoolSize = corePoolSize;
		pool->maximumPoolSize = maximumPoolSize;
		pool->busyNum = 0;
		pool->liveNum = corePoolSize;
		pool->exitNum = 0;
		// pool->poolLock = PTHREAD_MUTEX_INITIALIZER;
		// pool->busynumLock = PTHREAD_MUTEX_INITIALIZER;
		// pool->notFullCond = PTHREAD_COND_INITIALIZER;
		// pool->notEmptyCond = PTHREAD_COND_INITIALIZER;
		if (pthread_mutex_init(&(pool->poolLock), NULL) != 0 ||
			pthread_mutex_init(&(pool->busynumLock), NULL) != 0 ||
			pthread_cond_init(&(pool->notFullCond), NULL) != 0 ||
			pthread_cond_init(&(pool->notEmptyCond), NULL) != 0) 
		{
			printf("mutex and cond init failed!\n");
			break;
		}
		taskQueueInit(&(pool->taskQueue), taskQueueCapacity);
		pool->shutdown = 0;
		pthread_create(&(pool->managerID), NULL, manager, pool);
		for (int i = 0; i < corePoolSize; ++i) {
			pthread_create(&(pool->threadIDs[i]), NULL, worker, pool);
		}
		return pool;
	} while (0);
	if (pool != NULL) {
		if (pool->threadIDs != NULL) {
			free(pool->threadIDs);
			pool->threadIDs = NULL;
		}
		taskQueueDeInit(&pool->taskQueue);
		free(pool);
	}
	return NULL;
}
void threadPoolAddTask(ThreadPool *pool, Handler handle, void *arg) {
	pthread_mutex_lock(&(pool->poolLock));
	while (pool->taskQueue.size == pool->taskQueue.capacity && !pool->shutdown) {
		pthread_cond_wait(&(pool->notFullCond), &(pool->poolLock));
	}
	if (pool->shutdown) {
		pthread_mutex_unlock(&(pool->poolLock));
		return ;
	}
	taskQueuePush(&(pool->taskQueue), handle, arg);
	pthread_cond_signal(&(pool->notEmptyCond));
	pthread_mutex_unlock(&(pool->poolLock));
}

int threadPoolAliveNum(ThreadPool* pool) {
	pthread_mutex_lock(&(pool->poolLock));
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&(pool->poolLock));
	return liveNum;
}

int threadPoolBusyNum(ThreadPool* pool) {
	pthread_mutex_lock(&(pool->busynumLock));
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&(pool->busynumLock));
	return busyNum;
}

int threadPoolDestroy(ThreadPool* pool) {
	if (pool == NULL) return -1;
	pool->shutdown = 1;
	pthread_join(pool->managerID, NULL);
	for (size_t i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&(pool->notEmptyCond));
	}
	taskQueueDeInit(&(pool->taskQueue));
	if (pool->threadIDs) {
		free(pool->threadIDs);
		pool->threadIDs = NULL;
	}
	pthread_mutex_destroy(&(pool->poolLock));
	pthread_mutex_destroy(&(pool->busynumLock));
	pthread_cond_destroy(&(pool->notEmptyCond));
	pthread_cond_destroy(&(pool->notFullCond));
	free(pool);
	pool = NULL;
	return 0;
}

/***** 管理者线程与任务线程 *****/

void* manager(void* p_args) {
	ThreadPool* pool = (ThreadPool*) p_args;
	while (!(pool->shutdown)) {
		sleep(3);
		pthread_mutex_lock(&(pool->poolLock));
		int qSize = pool->taskQueue.size;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&(pool->poolLock));

		pthread_mutex_lock(&(pool->busynumLock));
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&(pool->busynumLock));

		// taskNum > liveNum && liveNum < maximumNum 添加线程
		if (qSize > liveNum && liveNum < pool->maximumPoolSize) {
			size_t ctr = 0;
			pthread_mutex_lock(&(pool->poolLock));
			for (size_t i = 0; i < pool->maximumPoolSize && ctr < ALTER_UNIT
				&& pool->liveNum < pool->maximumPoolSize; ++i)
			{
				if (pool->threadIDs[i] == 0) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					++ctr, ++(pool->liveNum);
				}
			}
			pthread_mutex_unlock(&(pool->poolLock));
		}

		// busyNum*2 < liveNum && liveNum > pool->corePoolSize 销毁线程
		if (busyNum * 2 < liveNum && liveNum > pool->corePoolSize) {
			pthread_mutex_lock(&(pool->poolLock));
			pool->exitNum = ALTER_UNIT;
			pthread_mutex_unlock(&(pool->poolLock));
			for (size_t i = 0; i < ALTER_UNIT; ++i) {
				pthread_cond_signal(&(pool->notEmptyCond));
			}
		}
	}
	return NULL;
}

void* worker(void* p_args) {
	ThreadPool* pool = (ThreadPool*) p_args;
	while (1) {
		pthread_mutex_lock(&(pool->poolLock));
		while (pool->taskQueue.size == 0 && !pool->shutdown) {
			pthread_cond_wait(&(pool->notEmptyCond), &(pool->poolLock));
			if (pool->exitNum > 0) {
				--(pool->exitNum);
				if (pool->liveNum > pool->corePoolSize) {
					--(pool->liveNum);
					pthread_mutex_unlock(&(pool->poolLock));
					threadExit(pool);
				}
			}
		}
		if (pool->shutdown) {
			pthread_mutex_unlock(&(pool->poolLock));
			threadExit(pool);
		}
		Task *task = taskQueuePop(&(pool->taskQueue));
		pthread_mutex_unlock(&(pool->poolLock));
		pthread_mutex_lock(&(pool->busynumLock));
		++(pool->busyNum);
		pthread_mutex_unlock(&(pool->busynumLock));
		task->handle(task->arg);
		pthread_cond_signal(&(pool->notFullCond));
		pthread_mutex_lock(&(pool->busynumLock));
		--(pool->busyNum);
		pthread_mutex_unlock(&(pool->busynumLock));
	}
	return NULL;
}



/******** 任务队列操作 ********/

int taskQueueInit(TaskQueue *tq, int capacity) {
	tq->q = (Task*) malloc(sizeof(Task) * capacity);
	if (tq->q == NULL) {
		printf("malloc task queue failed!\n");
		return -1;
	}
	tq->capacity = capacity;
	tq->qHead = 0;
	tq->qTail = 0;
	tq->size = 0;
	return 0;
}

int taskQueuePush(TaskQueue *tq, Handler pFunc, void* arg) {
	if (tq->size + 1 > tq->capacity) return -1;
	tq->q[tq->qTail].handle = pFunc;
	tq->q[tq->qTail].arg = arg;
	tq->qTail = (tq->qTail + 1) % tq->capacity;
	++(tq->size);
	return 0;
}

Task* taskQueuePop(TaskQueue *tq) {
	if (tq->size == 0) return NULL;
	Task *ret = &(tq->q[tq->qHead]);
	tq->qHead = (tq->qHead + 1) % tq->capacity;
	--(tq->size);
	return ret;
}

void taskQueueDeInit(TaskQueue *tq) {
	if (tq->q != NULL) {
		free(tq->q);
		tq->q = NULL;
	}
}

void threadExit(ThreadPool *pool) {
	pthread_t tid = pthread_self();
	printf("threadExit called, %ld exiting...\n", tid);
	for (size_t i = 0; i < pool->maximumPoolSize; ++i) {
		if (pool->threadIDs[i] == tid) {
			pool->threadIDs[i] = 0;
			break;
		}
	}
	pthread_exit(NULL);
}