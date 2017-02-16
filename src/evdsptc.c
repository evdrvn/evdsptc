#include "evdsptc.h"

void evdsptc_list_init(evdsptc_list_t* list){
    list->root.root = NULL;
    list->root.next = NULL;
    list->root.prev = NULL;
}

bool evdsptc_list_is_empty(evdsptc_list_t* list){
    return list->root.next == NULL;
}

evdsptc_listelem_t* evdsptc_list_iterator(evdsptc_list_t* list){
    return &list->root;
}

evdsptc_listelem_t* evdsptc_list_getlast(evdsptc_list_t* list){
    return list->root.prev;
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
    if(evdsptc_list_is_empty(list)) return NULL;
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

static void* evdsptch_thread_routine(void* arg){
    evdsptc_context_t* context = (evdsptc_context_t*)arg;
    evdsptc_event_t* event;
    bool finalize = false;
    while(1){
        pthread_mutex_lock(&context->mtx);
        while(context->state == EVDSPTC_STATUS_RUNNING && evdsptc_list_is_empty(&context->list) == true){
            pthread_cond_wait(&context->cv, &context->mtx);
        }
        if(context->state == EVDSPTC_STATUS_RUNNING) event = (evdsptc_event_t*)evdsptc_list_pop(&context->list);
        else finalize = true;
        pthread_mutex_unlock(&context->mtx);
        
        if(finalize == true) break;
        
        if(context->started_callback != NULL) context->started_callback(event);
        if(event->handler != NULL) event->is_done = event->handler(event->param);
        else event->is_done = true;
        __sync_synchronize(); 
        if(context->done_callback != NULL) context->done_callback(event);
        if(event->block_to_done == true && event->is_done == true) sem_post(&event->sem);
        if(event->auto_destruct_in_done && event->listelem.destructor != NULL) 
            event->listelem.destructor((evdsptc_listelem_t*)event);
    }
    return NULL;
}

evdsptc_error_t evdsptc_create (evdsptc_context_t* context,
        evdsptc_event_callback_t queued_callback,
        evdsptc_event_callback_t started_callback,
        evdsptc_event_callback_t done_callback)
{
    evdsptc_error_t ret = EVDSPTC_ERROR_FAIL_CREATE_THREAD;
    
    pthread_mutex_init(&context->mtx, NULL);
    pthread_cond_init(&context->cv, NULL);
    
    pthread_mutex_lock(&context->mtx);

    evdsptc_list_init(&context->list);
    context->state = EVDSPTC_STATUS_RUNNING;
    context->queued_callback = queued_callback;
    context->started_callback = started_callback;
    context->done_callback = done_callback; 

    if(0 != pthread_create(&context->th, NULL, &evdsptch_thread_routine, (void*) context)){
        ret = EVDSPTC_ERROR_FAIL_CREATE_THREAD;
        goto ERROR;
    }

    ret = EVDSPTC_ERROR_NONE;
    goto DONE;

ERROR:
    context->state = EVDSPTC_STATUS_ERROR;
DONE:
    pthread_mutex_unlock(&context->mtx);
    return ret;
}

evdsptc_error_t evdsptc_destory (evdsptc_context_t* context, bool join){
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;
    void* arg = NULL;
    
    pthread_mutex_lock(&context->mtx);
    if(context->state == EVDSPTC_STATUS_RUNNING){
        pthread_cond_signal(&context->cv);
        context->state = EVDSPTC_STATUS_DESTROYING;
    }
    pthread_mutex_unlock(&context->mtx);
    
    if(join) pthread_join(context->th, &arg);
    else pthread_detach(context->th);  
 
    evdsptc_list_destroy(&context->list);
   
    return ret;
}

evdsptc_error_t evdsptc_post (evdsptc_context_t* context, evdsptc_event_t* event) 
{
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;

    pthread_mutex_lock(&context->mtx);
    if(context->state == EVDSPTC_STATUS_RUNNING){
        pthread_cond_signal(&context->cv);
        event->context = context;
        evdsptc_list_push(&context->list, &event->listelem);
        if(context->queued_callback != NULL) context->queued_callback(event);
    } else ret = EVDSPTC_ERROR_INVALID_STATE;
    pthread_mutex_unlock(&context->mtx);

    if(event->block_to_done == true){
        while(-1 == sem_wait(&event->sem) && errno == EINTR) continue;
        __sync_synchronize();
        if(event->is_canceled == true) ret = EVDSPTC_ERROR_CANCELED;
    }        

    return ret;
}

static void evdsptc_event_abort (evdsptc_listelem_t* listelem){
        evdsptc_event_t* event = (evdsptc_event_t*)listelem;
        event->is_canceled = true;
        __sync_synchronize();
        if(event->block_to_done) sem_post(&event->sem);
        if(event->event_destructor != NULL) event->event_destructor((evdsptc_listelem_t*)event);
}

evdsptc_error_t evdsptc_event_init (evdsptc_event_t* event, 
        evdsptc_handler_t event_handler,
        void* event_param,
        bool block_to_done,
        bool auto_destruct_in_done,
        evdsptc_listelem_destructor_t event_destructor)
{
    evdsptc_error_t ret = EVDSPTC_ERROR_NONE;

    event->handler = event_handler;
    event->param = event_param;
    event->block_to_done = block_to_done;
    event->is_done = false;
    event->is_canceled = false;
    if(block_to_done == true) sem_init(&event->sem, 0, 0);
    event->listelem.destructor = evdsptc_event_abort;
    event->event_destructor = event_destructor;
    event->auto_destruct_in_done = auto_destruct_in_done;

    return ret;
}

void* evdsptc_event_getparam(evdsptc_event_t* event){
    return event->param; 
}

void evdsptc_event_free (evdsptc_listelem_t* event){
    free(event);
}