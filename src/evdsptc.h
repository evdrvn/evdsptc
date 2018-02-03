#ifndef __EVDSPTC_H__ 
#define __EVDSPTC_H__ 

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#define EVDSPTC_MAX_THREADS (256)

#ifdef EVDSPTRACE
#define EVDSPTC_TRACE(fmt, ...) printf("##TRACE## %p: " fmt "\n", (void*)pthread_self(), ##__VA_ARGS__); fflush(stdout)
#else
#define EVDSPTC_TRACE(fmt, ...) (void)sizeof(printf(fmt,##__VA_ARGS__))
#endif

typedef enum{
    EVDSPTC_ERROR_NONE = 0,
    EVDSPTC_ERROR_FAIL_CREATE_THREAD,
    EVDSPTC_ERROR_CANCELED,
    EVDSPTC_ERROR_INVALID,
    EVDSPTC_ERROR_NOT_DONE,
    EVDSPTC_ERROR_FAIL_INIT_MUTEX,
    EVDSPTC_ERROR_FAIL_INIT_COND
} evdsptc_error_t;

typedef enum{
    EVDSPTC_STATUS_ERROR = -1,
    EVDSPTC_STATUS_RUNNING = 0,
    EVDSPTC_STATUS_DESTROYING,
    EVDSPTC_STATUS_DESTROYED
} evdsptc_status_t;

typedef enum{
    EVDSPTC_TIMERTYPE_IMMEDIATE = 0,
    EVDSPTC_TIMERTYPE_RELATIVE ,
    EVDSPTC_TIMERTYPE_ABSOLUTE
} evdsptc_timertype_t;

typedef enum{
    EVDSPTC_TYPE_NORMAL = 0,
    EVDSPTC_TYPE_PERIODIC
} evdsptc_type_t;

typedef struct evdsptc_list evdsptc_list_t;
typedef struct evdsptc_listelem evdsptc_listelem_t;
typedef struct evdsptc_event evdsptc_event_t;
typedef struct evdsptc_context evdsptc_context_t;
typedef bool (*evdsptc_handler_t)(evdsptc_event_t* event);
typedef void (*evdsptc_event_callback_t)(evdsptc_event_t* event);
typedef void (*evdsptc_listelem_destructor_t)(evdsptc_listelem_t* listelem);
typedef void (*evdsptc_event_destructor_t)(evdsptc_event_t* event);

struct evdsptc_listelem {
    evdsptc_listelem_t* root;
    evdsptc_listelem_t* prev;
    evdsptc_listelem_t* next;
    evdsptc_listelem_destructor_t destructor;
};

struct evdsptc_list {
    evdsptc_listelem_t root;
};

struct evdsptc_event {
    evdsptc_listelem_t listelem;
    evdsptc_context_t* context;
    evdsptc_handler_t handler;
    void* param;
    bool is_done;
    bool is_canceled;
    sem_t sem;
    bool auto_destruct;
    evdsptc_event_destructor_t destructor;
    struct timespec timer;
    evdsptc_timertype_t timertype;
};

struct evdsptc_context {
    evdsptc_list_t list;
    evdsptc_list_t timer_list;
    int threads_num;
    pthread_t th[EVDSPTC_MAX_THREADS];
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    evdsptc_status_t state;
    evdsptc_type_t type;
    evdsptc_event_callback_t queued_callback;
    evdsptc_event_callback_t begin_callback;
    evdsptc_event_callback_t end_callback;
    struct timespec interval;
    unsigned long long int period_count; 
    bool period_overrun;
};

extern int evdsptc_timespec_compare (struct timespec* a, struct timespec* b);
extern struct timespec evdsptc_timespec_add (struct timespec* a, struct timespec* b);
extern void evdsptc_list_init(evdsptc_list_t* list);
extern bool evdsptc_list_isempty(evdsptc_list_t* list);
extern evdsptc_listelem_t* evdsptc_list_iterator(evdsptc_list_t* list);
extern evdsptc_listelem_t* evdsptc_list_getlast(evdsptc_list_t* list);
extern void evdsptc_listelem_setdestructor(evdsptc_listelem_t* listelem, evdsptc_listelem_destructor_t listelem_destructor);
extern evdsptc_listelem_t* evdsptc_listelem_next(evdsptc_listelem_t* listelem);
extern bool evdsptc_listelem_hasnext(evdsptc_listelem_t* listelem);
extern evdsptc_listelem_t* evdsptc_listelem_insertnext(evdsptc_listelem_t* listelem, evdsptc_listelem_t* next);
extern evdsptc_listelem_t* evdsptc_list_push(evdsptc_list_t* list, evdsptc_listelem_t* listelem);
extern evdsptc_listelem_t* evdsptc_listelem_remove(evdsptc_listelem_t* listelem);
extern evdsptc_listelem_t* evdsptc_list_pop(evdsptc_list_t* list);
extern void evdsptc_list_destroy(evdsptc_list_t* list);
extern evdsptc_error_t evdsptc_create (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback
        );
extern evdsptc_error_t evdsptc_create_threadpool (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback,
        int threads_num
        );
extern evdsptc_error_t evdsptc_create_periodic (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback,
        struct timespec* interval
        );
extern evdsptc_error_t evdsptc_destory (evdsptc_context_t* context, bool join);
extern evdsptc_error_t evdsptc_post (evdsptc_context_t* context, evdsptc_event_t* event);
extern evdsptc_error_t evdsptc_event_waitdone (evdsptc_event_t* event);
extern evdsptc_error_t evdsptc_event_trywaitdone (evdsptc_event_t* event);
extern evdsptc_error_t evdsptc_event_init (evdsptc_event_t* event,
        evdsptc_handler_t event_handler,
        void* event_param,
        bool auto_destruct,
        evdsptc_event_destructor_t event_destructor);
extern void* evdsptc_event_getparam(evdsptc_event_t* event);
extern void evdsptc_event_free (evdsptc_event_t* event);
extern pthread_t* evdsptc_getthreads(evdsptc_context_t* context);
extern pthread_mutex_t* evdsptc_getmutex(evdsptc_context_t* context);
extern void evdsptc_setmutexattrinitializer(pthread_mutexattr_t* attr);
extern void evdsptc_event_makedone (evdsptc_event_t* event);
extern bool evdsptc_event_isdone (evdsptc_event_t* event);
extern void evdsptc_event_destroy (evdsptc_event_t* event);
extern void evdsptc_event_cancel (evdsptc_event_t* event);
extern void evdsptc_event_setdestructor (evdsptc_event_t* event, evdsptc_event_destructor_t destructor);
extern void evdsptc_event_setautodestruct (evdsptc_event_t* event, bool auto_destruct);
extern void evdsptc_event_settimer (evdsptc_event_t* event, struct timespec* timer, evdsptc_timertype_t type);
extern unsigned long long int evdsptc_getperiodcount(evdsptc_context_t* context);
extern bool evdsptc_isperiodoverrun(evdsptc_context_t* context);

#ifdef __cplusplus
}
#endif

#endif
