#include "evdsptc.h"

static pthread_mutexattr_t* evdsptc_pmutexattrinitializer = NULL;
static pthread_mutexattr_t evdsptc_mutexattrinitializer;

void evdsptc_list_init(evdsptc_list_t* list){
    list->root.root = NULL;
    list->root.next = NULL;
    list->root.prev = NULL;
}

bool evdsptc_list_isempty(evdsptc_list_t* list){
    return list->root.next == NULL;
}

evdsptc_listelem_t* evdsptc_list_iterator(evdsptc_list_t* list){
    return &list->root;
}

evdsptc_listelem_t* evdsptc_list_getlast(evdsptc_list_t* list){
    return list->root.prev;
}

void evdsptc_listelem_setdestructor(evdsptc_listelem_t* listelem, evdsptc_listelem_destructor_t listelem_destructor){
    listelem->destructor = listelem_destructor;
}

evdsptc_listelem_t* evdsptc_listelem_next(evdsptc_listelem_t* listelem){
    return listelem->next;
}

bool evdsptc_listelem_hasnext(evdsptc_listelem_t* listelem){
    return listelem->next != NULL;
}

evdsptc_listelem_t* evdsptc_listelem_insertnext(evdsptc_listelem_t* listelem, evdsptc_listelem_t* next){
    evdsptc_listelem_t* root = NULL;
    
    if(listelem->root == NULL){
        root = listelem;
        next->prev = NULL;
    }
    else{ 
        root = listelem->root;
        next->prev = listelem;
    }
    next->root = root;

    next->next = listelem->next;
    if(NULL != next->next) next->next->prev = next;
    else root->prev = next;

    listelem->next = next;
    
    return next;
}

evdsptc_listelem_t* evdsptc_list_push(evdsptc_list_t* list, evdsptc_listelem_t* listelem){
    evdsptc_listelem_t* last;
    last = evdsptc_list_getlast(list);
    if(last == NULL) last = evdsptc_list_iterator(list);
    return evdsptc_listelem_insertnext(last, listelem);
}

evdsptc_listelem_t* evdsptc_listelem_remove(evdsptc_listelem_t* listelem){
    evdsptc_listelem_t *ret = NULL;

    if(listelem->root == NULL) return ret;

    if(listelem->root->next == listelem){
        listelem->root->next = listelem->next;
        if(listelem->root->next != NULL) listelem->root->next->prev = NULL;
    }else{
        listelem->prev->next = listelem->next;
    }
    
    if(listelem->root->prev == listelem){
        listelem->root->prev = listelem->prev;
        if(listelem->root->prev != NULL) listelem->root->prev->next = NULL;
    } else{
        listelem->next->prev = listelem->prev;
    }

    listelem->root = NULL;
    listelem->prev = NULL;
    listelem->next = NULL;

    ret = listelem;

    return ret;
}

evdsptc_listelem_t* evdsptc_list_pop(evdsptc_list_t* list){
    if(evdsptc_list_isempty(list)) return NULL;
    return evdsptc_listelem_remove(evdsptc_listelem_next(evdsptc_list_iterator(list)));
}

void evdsptc_list_destroy(evdsptc_list_t* list){
    evdsptc_listelem_t* i = evdsptc_list_iterator(list);
    evdsptc_listelem_t copied;
    evdsptc_listelem_t* removed;

    while(evdsptc_listelem_hasnext(i)){
        i = evdsptc_listelem_next(i);
        copied = *i;
        removed = evdsptc_listelem_remove(i);
        if(removed->destructor != NULL) removed->destructor(removed);
        i = &copied;
    }
    list->root.next = NULL;
    list->root.prev = NULL;
    
    return;
}

int evdsptc_timespec_compare (struct timespec* l, struct timespec* r){
    if(l->tv_sec < r->tv_sec) return -1;
    if(l->tv_sec > r->tv_sec) return 1;
    return (l->tv_nsec - r->tv_nsec);
}

static void* evdsptc_thread_routine(void* arg){
    evdsptc_context_t* context = (evdsptc_context_t*)arg;
    evdsptc_event_t* event;
    bool finalize = false;
    bool auto_destruct = false;
    struct timespec now;
    struct timespec next;
    bool wakeup = false;
    evdsptc_list_t periodic_events_handled;
    int ret = 0;

    while(1){
        pthread_mutex_lock(&context->mtx);
        while(context->state == EVDSPTC_STATUS_RUNNING){
            if(context->type == EVDSPTC_TYPE_PERIODIC){
                if(!wakeup){
                    evdsptc_list_init(&periodic_events_handled);
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    next = evdsptc_timespec_add(&now, &context->interval);
                    ret = EINTR;
                    while(ret == EINTR) ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
                    next = evdsptc_timespec_add(&next, &context->interval);
                    context->period_count = 0; 
                    wakeup = true;
                    __sync_synchronize();
                }
                if(evdsptc_list_isempty(&context->list)){
                    while(!evdsptc_list_isempty(&periodic_events_handled)) 
                        evdsptc_list_push(&context->list, evdsptc_list_pop(&periodic_events_handled));
                    ret = EINTR;
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    pthread_mutex_unlock(&context->mtx);
                    
                    while(ret == EINTR) ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
                    
                    pthread_mutex_lock(&context->mtx);
                    context->period_count++; 
                    __sync_synchronize();
                    if(evdsptc_timespec_compare(&next, &now) < 0) next = evdsptc_timespec_add(&now, &context->interval);
                    else next = evdsptc_timespec_add(&next, &context->interval);
                    event = NULL;
                    break;
                }else{
                    event = (evdsptc_event_t*)evdsptc_list_pop(&context->list);
                    break;
                }
            }else{
                if(evdsptc_list_isempty(&context->list) && evdsptc_list_isempty(&context->timer_list)) 
                    pthread_cond_wait(&context->cv, &context->mtx);
                else if(!evdsptc_list_isempty(&context->timer_list)){
                    event = (evdsptc_event_t*)evdsptc_listelem_next(evdsptc_list_iterator(&context->timer_list));
                    clock_gettime(CLOCK_REALTIME, &now);
                    if(evdsptc_timespec_compare(&event->timer, &now) <= 0){
                        event = (evdsptc_event_t*)evdsptc_list_pop(&context->timer_list);
                        break;
                    }
                    else if(!evdsptc_list_isempty(&context->list)){
                        event = (evdsptc_event_t*)evdsptc_list_pop(&context->list);
                        break;
                    }
                    else pthread_cond_timedwait(&context->cv, &context->mtx, &event->timer);
                }
                else{
                    event = (evdsptc_event_t*)evdsptc_list_pop(&context->list);
                    break;
                }
            }
        }
        if(context->state != EVDSPTC_STATUS_RUNNING) finalize = true;
        pthread_mutex_unlock(&context->mtx);
        
        if(finalize == true) break;
        else if(NULL == event) continue;
        
        if(context->begin_callback != NULL) context->begin_callback(event);
        if(event->handler != NULL) event->is_done = event->handler(event);
        else event->is_done = true;
        __sync_synchronize(); 
        if(context->end_callback != NULL) context->end_callback(event);
        auto_destruct = event->auto_destruct;
        if(event->is_done == true) sem_post(&event->sem);
        else if(context->type == EVDSPTC_TYPE_PERIODIC) evdsptc_list_push(&periodic_events_handled, (evdsptc_listelem_t*)event);
        if(auto_destruct && event->destructor != NULL && event->is_done == true) 
            event->destructor(event);
    }
    return NULL;
}

static evdsptc_error_t evdsptc_create_impl (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback,
        int threads_num,
        evdsptc_type_t type
        )
{
    evdsptc_error_t ret = EVDSPTC_ERROR_FAIL_CREATE_THREAD;
    int i;

    if(threads_num < 1 || EVDSPTC_MAX_THREADS < threads_num){
        ret = EVDSPTC_ERROR_INVALID;
        goto ERROR;
    }else{
        context->threads_num = threads_num;
    }
   
    if(0 != pthread_mutex_init(&context->mtx, evdsptc_pmutexattrinitializer)) return EVDSPTC_ERROR_FAIL_INIT_MUTEX;
    if(0 != pthread_cond_init(&context->cv, NULL)) return EVDSPTC_ERROR_FAIL_INIT_COND;
    
    pthread_mutex_lock(&context->mtx);

    evdsptc_list_init(&context->list);
    evdsptc_list_init(&context->timer_list);
    context->state = EVDSPTC_STATUS_RUNNING;
    context->queued_callback = queued_callback;
    context->begin_callback = begin_callback;
    context->end_callback = end_callback; 
    context->type = type;

    for(i = 0; i < context->threads_num; i++){
        if(0 != pthread_create(&context->th[i], NULL, &evdsptc_thread_routine, (void*) context)){
            ret = EVDSPTC_ERROR_FAIL_CREATE_THREAD;
            goto ERROR;
        }
    }

    ret = EVDSPTC_ERROR_NONE;
    goto DONE;

ERROR:
    context->state = EVDSPTC_STATUS_ERROR;
DONE:
    pthread_mutex_unlock(&context->mtx);
    return ret;
}

evdsptc_error_t evdsptc_create (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback)
{
    return evdsptc_create_impl(context, queued_callback, begin_callback, end_callback, 1, EVDSPTC_TYPE_NORMAL);
} 

evdsptc_error_t evdsptc_create_threadpool (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback,
        int threads_num)
{
    return evdsptc_create_impl(context, queued_callback, begin_callback, end_callback, threads_num, EVDSPTC_TYPE_NORMAL);
} 

evdsptc_error_t evdsptc_create_periodic (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t begin_callback,
        evdsptc_event_callback_t end_callback,
        struct timespec* interval)
{
    context->interval = *interval;
    return evdsptc_create_impl(context, queued_callback, begin_callback, end_callback, 1, EVDSPTC_TYPE_PERIODIC);
} 


evdsptc_error_t evdsptc_destory (evdsptc_context_t* context, bool join){
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;
    void* arg = NULL;
    int i;

    pthread_mutex_lock(&context->mtx);
    if(context->state == EVDSPTC_STATUS_RUNNING){
        context->state = EVDSPTC_STATUS_DESTROYING;
        pthread_cond_broadcast(&context->cv);
    }
    pthread_mutex_unlock(&context->mtx);

    for(i = 0; i < context->threads_num; i++){
        if(join) pthread_join(context->th[i], &arg);
        else pthread_detach(context->th[i]);  
    }

    evdsptc_list_destroy(&context->list);
    evdsptc_list_destroy(&context->timer_list);
   
    return ret;
}

void evdsptc_event_cancel (evdsptc_event_t* event){
    event->is_canceled = true;
    __sync_synchronize();
    sem_post(&event->sem);
    if(event->auto_destruct && event->destructor != NULL) event->destructor(event);
}

static void evdsptc_listelem_cancel (evdsptc_listelem_t* listelem){
    evdsptc_event_t* event = (evdsptc_event_t*)listelem;
    evdsptc_event_cancel (event);
}

static bool evdsptc_event_isnearer (evdsptc_event_t* event, evdsptc_event_t* other){
    return 0 > evdsptc_timespec_compare(&event->timer, &other->timer);
}

struct timespec evdsptc_timespec_add (struct timespec* a, struct timespec* b){
    struct timespec ret;
    ret = *a;
    ret.tv_sec  += b->tv_sec;
    ret.tv_nsec += b->tv_nsec;
    long nsec_is_1sec = 1000L * 1000L * 1000L;
    while(nsec_is_1sec <= ret.tv_nsec){
        ret.tv_sec++;
        ret.tv_nsec -= nsec_is_1sec;
    }
    return ret;
}

evdsptc_error_t evdsptc_post (evdsptc_context_t* context, evdsptc_event_t* event) 
{
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;
    evdsptc_listelem_t* current = NULL;
    evdsptc_listelem_t* next = NULL;
    struct timespec now;

    pthread_mutex_lock(&context->mtx);
    if(context->state == EVDSPTC_STATUS_RUNNING){
        pthread_cond_broadcast(&context->cv);
        event->context = context;
        if(EVDSPTC_TIMERTYPE_IMMEDIATE == event->timertype){
            evdsptc_list_push(&context->list, &event->listelem);
        }else{
            if(EVDSPTC_TIMERTYPE_RELATIVE == event->timertype){
                clock_gettime(CLOCK_REALTIME, &now);
                event->timer = evdsptc_timespec_add(&now, &event->timer);
            }
            current = evdsptc_list_iterator(&context->timer_list);
            while(evdsptc_listelem_hasnext(current)){
                next = evdsptc_listelem_next(current);
                if(evdsptc_event_isnearer(event, (evdsptc_event_t*)next)) break;
                current = next;
            }
            evdsptc_listelem_insertnext(current, (evdsptc_listelem_t*)event);
        }
        if(context->queued_callback != NULL) context->queued_callback(event);
    } else ret = EVDSPTC_ERROR_INVALID;
    pthread_mutex_unlock(&context->mtx);

    if(ret != EVDSPTC_ERROR_NONE) evdsptc_event_cancel(event);

    return ret;
}

evdsptc_error_t evdsptc_event_waitdone (evdsptc_event_t* event) 
{
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;
    errno = 0;
    while(-1 == sem_wait(&event->sem) && errno == EINTR) continue;
    __sync_synchronize();
    
    if(errno == EINVAL) ret = EVDSPTC_ERROR_INVALID; 
    else if(event->is_canceled == true) ret = EVDSPTC_ERROR_CANCELED;

    return ret;
}

evdsptc_error_t evdsptc_event_trywaitdone (evdsptc_event_t* event) 
{
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;
    errno = 0;
    while(-1 == sem_trywait(&event->sem) && errno == EINTR) continue;
    __sync_synchronize();
    
    if(errno == EINVAL) ret = EVDSPTC_ERROR_INVALID; 
    else if(errno == EAGAIN) ret = EVDSPTC_ERROR_NOT_DONE; 
    else if(event->is_canceled == true) ret = EVDSPTC_ERROR_CANCELED;

    return ret;
}

evdsptc_error_t evdsptc_event_init (evdsptc_event_t* event, 
        evdsptc_handler_t event_handler,
        void* event_param,
        bool auto_destruct,
        evdsptc_event_destructor_t destructor)
{
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;

    event->handler = event_handler;
    event->param = event_param;
    event->is_canceled = false;
    event->is_done = false;
    sem_init(&event->sem, 0, 0);
    event->listelem.destructor = evdsptc_listelem_cancel;
    event->destructor = destructor;
    event->auto_destruct = auto_destruct;
    event->timer.tv_sec = 0;
    event->timer.tv_nsec = 0;
    event->timertype = EVDSPTC_TIMERTYPE_IMMEDIATE;

    return ret;
}

void evdsptc_event_settimer (evdsptc_event_t* event, struct timespec* timer, evdsptc_timertype_t type)
{
    event->timer = *timer;
    event->timertype = type;
}

void* evdsptc_event_getparam(evdsptc_event_t* event){
    return event->param; 
}

void evdsptc_event_free (evdsptc_event_t* event){
    free(event);
}

pthread_t* evdsptc_getthreads(evdsptc_context_t* context){
    return context->th;
}

pthread_mutex_t* evdsptc_getmutex(evdsptc_context_t* context){
    return &context->mtx;
}

void evdsptc_setmutexattrinitializer(pthread_mutexattr_t* attr){
    evdsptc_mutexattrinitializer = *attr;
    evdsptc_pmutexattrinitializer = &evdsptc_mutexattrinitializer;
}

void evdsptc_event_makedone (evdsptc_event_t* event){
    event->is_done = true;
    __sync_synchronize();
    sem_post(&event->sem);
}

bool evdsptc_event_isdone (evdsptc_event_t* event){
    __sync_synchronize();
    return event->is_done;
}

void evdsptc_event_destroy (evdsptc_event_t* event){
    if(event->destructor != NULL) event->destructor(event);
}

void evdsptc_event_setdestructor (evdsptc_event_t* event, evdsptc_event_destructor_t destructor){
    event->destructor = destructor;
}

void evdsptc_event_setautodestruct (evdsptc_event_t* event, bool auto_destruct){
    event->auto_destruct = auto_destruct;
}
