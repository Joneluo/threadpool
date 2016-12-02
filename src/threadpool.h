#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

typedef struct threadPool threadPool;

threadPool *threadPoolInit(int thread_nums,int que_size);

int threadPoolAddTask(threadPool *pool,void(*runTask)(void *),void *arg);

int threadPoolDestroy(threadPool **pool);

#ifdef __cplusplus
    }
#endif // __cplusplus

#endif // _THREADPOOL_H_
