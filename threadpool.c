#include "threadpool.h"
#include "stdlib.h"
#include "stdio.h"
#include <pthread.h>
#include "unistd.h"


/*thread-pool struct is the number of  the active number of threads, it should be smaller than 200 (200 is the max),
 * qsize is the size of the work queue, every time the client connect to the server and the server returned from accept,
 * it pushes it to the queue.*/
threadpool* create_threadpool(int num_threads_in_pool){
    if(num_threads_in_pool>MAXT_IN_POOL || num_threads_in_pool<=0)// Legacy of the parameter.
        return NULL;

    threadpool *th_pool= (threadpool*)malloc(sizeof(threadpool));// Create thread-pool structure
    if(th_pool==NULL){//if malloc failed.
        perror("\n Problem with allocate th_pool, from function: create_threadpool\n");
        exit(0);
    }

    //now I want to initialize the structure of the thread-pool.
    th_pool->num_threads=num_threads_in_pool;//number of active threads
    th_pool->qsize=0; //number in the queue, it means what is the size of the works, every time that there is a connection it will be pushed to the queue. "how many works".
    th_pool->threads=(pthread_t*)malloc(sizeof(pthread_t)*(num_threads_in_pool));//pointer to threads. Array of pthreads.
    if(th_pool->threads==NULL){// if malloc failed.
        perror("\n Problem with allocate th_pool->threads, from function: create_threadpool\n");
        exit(0);
    }
    th_pool->qtail=NULL;//queue tail pointer
    th_pool->qhead=NULL;//queue head pointer
    if(pthread_mutex_init(&(th_pool->qlock), NULL) != 0){//lock on the queue list
        perror("\n Problem with pthread_mutex_init, from function: create_threadpool\n");
        exit(0);
    }
    /*When the queue is empty, the threads can "sleep".
     * When the queue is full ,or when it is not empty, the main will "sleep",
     * the main will be awake when someone will take the work...*/
    if(pthread_cond_init(&(th_pool->q_not_empty), NULL)!=0){
        perror("\n Problem with pthread_cond_init, from function: create_threadpool\n");
        exit(0);
    }
    if(pthread_cond_init(&(th_pool->q_empty),NULL)!=0){
        perror("\n Problem with pthread_cond_init, from function: create_threadpool\n");
        exit(0);
    }
    /*shutdown and dont_accept are flags.
     * at the time that someone called to destroy, dont_accept will turn on.
     * shutdown means that the queue is empty and I start with the destroy process*/
    th_pool->shutdown=0;//1 if the pool is in distruction process
    th_pool->dont_accept=0;//1 if destroy function has begun

    int rc, t;
    for(t=0 ;t<num_threads_in_pool ; t++){
        rc= pthread_create((th_pool->threads)+t, NULL, do_work, (void *)th_pool);
        if(rc){
            printf("Problem with pthread_create, from function: create_threadpool\n");
            exit(-1);
        }
    }

    return th_pool;
}

/*struct that describes a work*/
void* do_work(void* p){
//    printf("%lu\n",pthread_self());
    if(p==NULL){
        return NULL;
    }
    threadpool * p1=(threadpool*)p;//casting from void*.
    work_t *w;
//    printf("\n %d\n", (int)pthread_self());

    while(1){
        //we lock this part because every thread is going to do it.
        pthread_mutex_lock(&(p1->qlock));


        if(p1->shutdown==1){// so don't make a work, the destruction process has begun.
            pthread_mutex_unlock(&(p1->qlock));
            return NULL;
        }

        if(p1->qsize==0){// it the queue is empty so sleep.
            pthread_cond_wait(&(p1->q_not_empty), &(p1->qlock));// get signal when qsize!=0, queue not empty.
        }
        if(p1->shutdown==1){// so don't make a work, the destruction process has begun.
            pthread_mutex_unlock(&(p1->qlock));
            return NULL;
        }

        //so, there is work to do
        w=p1->qhead;
        if(w==NULL){
            return NULL;
        }
        if(w!=NULL) {
            //dequeue
            p1->qhead=p1->qhead->next;
            (p1->qsize)--;
            if(p1->qhead==NULL)//if there is only one work,(p1->qhead->next==NULL).
                p1->qtail=p1->qhead;
            w->routine(w->arg);
            free(w);
            w=NULL;
        }
        if(p1->qsize==0 && p1->dont_accept==1){// give a signal.
            pthread_cond_signal(&(p1->q_empty));
            pthread_mutex_unlock(&(p1->qlock));
            return 0;
        }

        pthread_mutex_unlock(&(p1->qlock));
    }
}

/*create work_t*
 from_me array of threads,
 dispatch_to_here pointer to function,
 arg is the parameter of the function*/
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if(from_me==NULL || dispatch_to_here==NULL){
        return;
    }
    pthread_mutex_lock(&(from_me->qlock));//from me is a pointer to the thread pool.

    if(from_me->dont_accept==1){
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    work_t *w=(work_t*) malloc(sizeof (work_t));
    if(w==NULL){
        perror("\n Problem with allocate w, from function: dispatch\n");
        exit(0);
    }

    w->routine=dispatch_to_here;
    w->arg=arg;
    w->next = NULL;


    //enqueue to the list the new work...
    if((from_me->qsize)==0){// the list is empty.
        from_me->qhead=w;
        from_me->qtail=w;

    }
    else{//there are another works in the list.
        from_me->qtail->next=w;
        from_me->qtail=w;
//        from_me->qtail->next->next = NULL;

    }
    (from_me->qsize)++;
    pthread_cond_signal(&(from_me->q_not_empty ));
    pthread_mutex_unlock(&(from_me->qlock));
}

void destroy_threadpool(threadpool* destroyme){
    if(destroyme==NULL)
        return;

    pthread_mutex_lock(&(destroyme->qlock));// from me is a pointer to the thread pool.


    destroyme->dont_accept=1;// turn on the flag.

    if(destroyme->qsize>0)
        pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock));// get signal when qsize!=0, queue not empty.

    destroyme->shutdown=1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));//awake everyone.

    pthread_mutex_unlock(&(destroyme->qlock));


    int th=0;
    while( th<destroyme->num_threads){
//        printf("\n%d\n", th);
        pthread_join(destroyme->threads[th], NULL);
        th++;
    }

    //free all...
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    free(destroyme->threads);
    destroyme->threads=NULL;
    free(destroyme);
    destroyme=NULL;

}