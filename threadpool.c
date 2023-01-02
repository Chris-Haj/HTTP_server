#include "threadpool.h"
#include <stdlib.h>

#define alloc(type, size) (type *) malloc(sizeof(type)*size)

threadpool *create_threadpool(int num_threads_in_pool) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
        return NULL;
    threadpool *pool = alloc(threadpool,1);
    pool->num_threads = num_threads_in_pool;
    pool->qsize = 0;
    pool->threads = alloc(pthread_t, num_threads_in_pool);
    pool->qhead = NULL;
    pool->qtail = NULL;
    pool->shutdown = pool->dont_accept = 0;
    pthread_mutex_init(&pool->qlock, NULL);
    pthread_cond_init(&pool->q_not_empty, NULL);
    pthread_cond_init(&pool->q_empty, NULL);
    for (int i = 0; i < num_threads_in_pool; i++) {
        pthread_create(&pool->threads[i], NULL, do_work, pool);
    }
    return pool;
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg) {
    pthread_mutex_lock(&from_me->qlock);
    while (from_me->dont_accept) {
        pthread_cond_wait(&from_me->q_empty, &from_me->qlock);
    }
    work_t *work = alloc(work_t, 1);
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;
    if (from_me->qsize == 0) {
        from_me->qhead = from_me->qtail = work;
    } else {
        from_me->qtail->next = work;
        from_me->qtail = work;
    }
    from_me->qsize++;
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock);
}

void *do_work(void *p) {
    threadpool *pool = (threadpool *) p;
    while(1){
        pthread_mutex_lock(&pool->qlock);
        while(pool->qsize == 0 && !pool->shutdown){
            pthread_cond_wait(&pool->q_not_empty, &pool->qlock);
        }
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->qlock);
            pthread_exit(NULL);
        }
        work_t *work = pool->qhead;
        pool->qhead = work->next;
        pool->qsize--;
        if(pool->qsize == 0){
            pool->qtail = NULL;
        }
        if(pool->qsize == 0 && pool->dont_accept){
            pthread_cond_signal(&pool->q_empty);
        }
        pthread_mutex_unlock(&pool->qlock);
        work->routine(work->arg);
        free(work);
    }
}

void destroy_threadpool(threadpool *destroyme) {
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;
    while(destroyme->qsize != 0){
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);
    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);
    }
    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_cond_destroy(&destroyme->q_empty);
    free(destroyme->threads);
    free(destroyme);
}
