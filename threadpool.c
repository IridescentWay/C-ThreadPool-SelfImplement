#include "threadpool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef void (*Handler)(void*);

typedef struct __task {
	Handler handle;
	void* arg;
} Task;

typedef struct __task_queue {
	Task** q;
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

/******** 任务队列操作 ********/

int taskQueueInit(TaskQueue *tq, int capacity) {
	tq->q = (Task**) malloc(sizeof(Task*) * capacity);
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

int taskQueuePush(TaskQueue *tq, Task* task) {
	if (tq->size + 1 > tq->capacity) return -1;
	tq->q[tq->qTail] = task;
	tq->qTail = (tq->qTail + 1) % tq->capacity;
	++tq->size;
	return 0;
}

Task* taskQueuePop(TaskQueue *tq) {
	if (tq->size == 0) return NULL;
	Task *ret = tq->q[tq->qHead];
	tq->qHead = (tq->qHead + 1) % tq->capacity;
	--tq->size;
	return ret;
}

void taskQueueDeInit(TaskQueue *tq) {
	if (tq->q != NULL) free(tq->q);
}

/***** 管理者线程与任务线程 *****/

void* manager(void* p_args) {
	return NULL;
}

void* worker(void* p_args) {
	return NULL;
}


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
		if (pthread_mutex_init(&pool->poolLock, NULL) != 0 ||
			pthread_mutex_init(&pool->busynumLock, NULL) != 0 ||
			pthread_cond_init(&pool->notFullCond, NULL) != 0 ||
			pthread_cond_init(&pool->notEmptyCond, NULL) != 0) 
		{
			printf("mutex and cond init failed!\n");
			break;
		}
		taskQueueInit(&pool->taskQueue, taskQueueCapacity);
		pool->shutdown = 0;
		pthread_create(&pool->managerID, NULL, manager, NULL);
		for (int i = 0; i < corePoolSize; ++i) {
			pthread_create(&pool->threadIDs[i], NULL, worker, NULL);
		}
		return pool;
	} while (0);
	if (pool != NULL) {
		if (pool->threadIDs != NULL) {
			free(pool->threadIDs);
		}
		taskQueueDeInit(&pool->taskQueue);
		free(pool);
	}
	return NULL;
}