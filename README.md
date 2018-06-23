evdsptc
=======

evdsptc is an event dispatching framework. Its main features and design principles are:

* Selectable sync type
    * sync event
    * async event
* Selectable timer type
    * relative
    * absolute
    * periodic (low jitter)
    * immediate (normal)
* Suitable for real-time system
    * avoid malloc
* Selectable threading model
    * single thread
    * thread pool

## Getting Started

* Build libpthread from source

    For Linux:

    ```shell
    cd build
    cmake ..
    make
    ```
    For MinGW on Windows:

    ```shell
    cd build
    cmake -G "MinGW Makefiles" ..
    mingw32-make
    ```

* Compile minimum program

    ```c
    #include "evdsptc.h"
    
    static bool hello(evdsptc_event_t* event){
        printf("hello %s\n", (char*)evdsptc_event_getparam(event));
        return true;
    }
    
    int main(int ac, char** av){
        evdsptc_context_t ctx;
        evdsptc_event_t ev;
    
        char param[10] = "world";
    
        evdsptc_create(&ctx, NULL, NULL, NULL);
    
        evdsptc_event_init(&ev, hello, (void*)param, false, NULL);
        evdsptc_post(&ctx, &ev);
        evdsptc_event_waitdone(&ev);
    
        evdsptc_destroy(&ctx, true);
    
        return 0;
    }
    
    ```

* Link libevdsptc and libpthread
    
    * For Windows, install [pthreads-w32](https://sourceforge.net/projects/mingw/files/MinGW/Base/pthreads-w32/)

* Run

## Examples

* async event
    * See async_event_example, test/src/example.cpp 

* sync event
    * See sync_event_example, test/src/example.cpp 

* timer event
    * See timer_example, test/src/example.cpp 

* periodic event
    * See periodic_example, test/src/example.cpp 

* list 
    * See list_bubble_sort_example, test/src/example.cpp 

* thread pool
    * See async_event_threadpool_example, test/src/example.cpp 

* and more
    * See test/src/evdsptc_test.cpp

## Running Tests

* Building Cpputest
    ```shell
    git submodule init
    git submodule update
    cd test
    sh ./build_cpputest.sh
    ```

* Run
    ```sh
    make
    ```

## API Reference

### evdsptc_create
```c
evdsptc_error_t evdsptc_create (evdsptc_context_t* context,
    evdsptc_event_callback_t queued_callback,
    evdsptc_event_callback_t begin_callback,
    evdsptc_event_callback_t end_callback);
```
creates a event dispatcher. event dispatcher has a event dispatcher thread and an event queue.
* queued_callback is function pointer called by event publisher thread when the event queued. if set NULL, call nothing.
* begin_callback is function pointer called by event dispatcher thread when the event handler begin. if set NULL, call nothing.
* end_callback is function pointer called by event dispatcher thread when the handler event end. if set NULL, call nothing.
* callback order is the following:
    * queued_callback
    * begin_callback
    * event handler
    * end_callback

### evdsptc_create_threadpool
```c
evdsptc_error_t evdsptc_create_threadpool (evdsptc_context_t* context,
    evdsptc_event_callback_t queued_callback,
    evdsptc_event_callback_t begin_callback,
    evdsptc_event_callback_t end_callback
    int threads_num);
```
creates a event dispatcher. event dispatcher have multiple event dispatcher threads and an event queue.
* threads_num is number of worker threads, and its maximum is 256.
* The other arguments are similar to evdsptc_create.

### evdsptc_create_periodic
```c
evdsptc_error_t evdsptc_create_periodic (evdsptc_context_t* context,
    evdsptc_event_callback_t queued_callback,
    evdsptc_event_callback_t begin_callback,
    evdsptc_event_callback_t end_callback
    struct timespec* interval);
```
creates a event dispatcher. event dispatcher has a periodic event dispatcher thread and an event queue. you get better performance to run  as root via RT-Preempt.
* interval is the intervel of periodic dispaching. events are removed from the queue when it done (when the event handler returns true). In other words, events that its handler returns false continue to be dispatched. 
* evdsptc_event_settimer() is not supported. 

### evdsptc_cancel
```c
evdsptc_error_t evdsptc_cancel (evdsptc_context_t* context);
```
stops the event dispater. events in the queue are canceled. 

### evdsptc_destroy
```c
evdsptc_error_t evdsptc_destroy (evdsptc_context_t* context, bool join);
```
destroys the event dispater. events in the queue are canceled. 

### evdsptc_event_init
```c
evdsptc_error_t evdsptc_event_init (evdsptc_event_t* event,
    evdsptc_handler_t event_handler,
    void* event_param,
    bool auto_destruct,
    evdsptc_event_destructor_t destructor);
```
initailizes the event object.
* event_handler return value is TRUE means if the event is done. 
* event_param is User-defined event context.
* if auto_destruct is true, the event is destroyed automatically by its destructor called when event handler returns true (means the event is done) or the event canceled.
* destructor is function pointer that frees the event.  

### evdsptc_post
```c
evdsptc_error_t evdsptc_post (evdsptc_context_t* context, evdsptc_event_t* event);
```
posts the event.

### event_waitdone
```c
evdsptc_error_t evdsptc_event_waitdone (evdsptc_event_t* event);
```
blocking-waits until the event is done.
* if the event canceled, returns EVDSPTC_ERROR_CANCELED.

### event_trywaitdone
```c
evdsptc_error_t evdsptc_event_trywaitdone (evdsptc_event_t* event);
```
returns if the event is done.
* if the event is done, returns EVDSPTC_ERROR_NONE.
* if the event is not done, returns EVDSPTC_ERROR_NOT_DONE.
* if the event canceled, returns EVDSPTC_ERROR_CANCELED.

### evdsptc_event_getparam
```c
void* evdsptc_event_getparam(evdsptc_event_t* event);
```
returns event_param.

### evdsptc_event_destroy
```c
void evdsptc_event_destroy (evdsptc_event_t* event);
```
calls destructor.

### evdsptc_event_free
```c
void evdsptc_event_free (evdsptc_event_t* event);
```
is typical event destructor, frees the event, but not its event_param.

### evdsptc_getthreads
```c
pthread_t* evdsptc_getthreads(evdsptc_context_t* context);
```
returns event dispatcher threads (is a pointer to pthread_t array).

### evdsptc_getmutex
```c
pthread_mutex_t* evdsptc_getmutex(evdsptc_context_t* context);
```
returns event dispatcher mutex. 

### evdsptc_event_makedone
```c
void evdsptc_event_makedone (evdsptc_event_t* event);
```
marks the event done.

### evdsptc_event_isdone
```c
void evdsptc_event_isdone (evdsptc_event_t* event);
```
returns true if the event is done.

### evdsptc_event_cancel
```c
void evdsptc_event_cancel (evdsptc_event_t* event);
```
cancel the event.

### evdsptc_event_setdestructor
```c
void evdsptc_event_setdestructor (evdsptc_event_t* event, evdsptc_event_destructor_t destructor);
```
sets destructor to the event.

### evdsptc_event_setautodestruct
```c
void evdsptc_event_setautodestruct (evdsptc_event_t* event, bool auto_destruct);
```
sets auto destruction to the event.

### evdsptc_event_settimer
```c
void evdsptc_event_settimer (evdsptc_event_t* event, struct timespec* timer, evdsptc_timertype_t type);
```
sets timer  to the event.
* type is selectable from EVDSPTC_TIMERTYPE_ABSOLUTE or EVDSPTC_TIMERTYPE_RELATIVE.  

### evdsptc_getperiodcount
```c
unsigned long long int evdsptc_getperiodcount(evdsptc_context_t* context);
```
gets period count.

### evdsptc_isperiodoverrun
```c
extern bool evdsptc_isperiodoverrun(evdsptc_context_t* context);
```
returns true when the previous period is longer then the periodic interval.

## Utility Reference

### evdsptc_timespec_compare
```c
int evdsptc_timespec_compare (struct timespec* a, struct timespec* b);
```

### evdsptc_timespaec_add
```c
struct timespec evdsptc_timespec_add (struct timespec* a, struct timespec* b);
```

### evdsptc_list_init
```c
void evdsptc_list_init(evdsptc_list_t* list);
```

### evdsptc_list_isempty
```c
bool evdsptc_list_isempty(evdsptc_list_t* list);
```

### evdsptc_list_iterator
```c
evdsptc_listelem_t* evdsptc_list_iterator(evdsptc_list_t* list);
```

### evdsptc_list_getlast
```c
evdsptc_listelem_t* evdsptc_list_getlast(evdsptc_list_t* list);
```

### evdsptc_listelem_setdestructor
```c
void evdsptc_listelem_setdestructor(evdsptc_listelem_t* listelem, evdsptc_listelem_destructor_t listelem_destructor);
```

### evdsptc_listelem_next
```c
evdsptc_listelem_t* evdsptc_listelem_next(evdsptc_listelem_t* listelem);
```

### evdsptc_listelem_hasnext
```c
bool evdsptc_listelem_hasnext(evdsptc_listelem_t* listelem);
```

### evdsptc_listelem_insertnext
```c
evdsptc_listelem_t* evdsptc_listelem_insertnext(evdsptc_listelem_t* listelem, evdsptc_listelem_t* next);
```

### evdsptc_list_push
```c
evdsptc_listelem_t* evdsptc_list_push(evdsptc_list_t* list, evdsptc_listelem_t* listelem);
```

### evdsptc_listelem_remove
```c
evdsptc_listelem_t* evdsptc_listelem_remove(evdsptc_listelem_t* listelem);
```

### evdsptc_list_pop
```c
evdsptc_listelem_t* evdsptc_list_pop(evdsptc_list_t* list);
```

### evdsptc_list_destroy
```c
void evdsptc_list_destroy(evdsptc_list_t* list);
```

