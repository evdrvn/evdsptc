evdsptc
=======

sync/async event dispatcher for C/C++

## Getting Started

* Building libevdsptc from source

    ```shell
    cd build
    cmake ..
    make
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
    
        evdsptc_destory(&ctx, true);
    
        return 0;
    }
    
    ```

* Link libevdsptc and libpthread
* Run

## APIs

```c
evdsptc_error_t evdsptc_create (evdsptc_context_t* context,
    evdsptc_event_callback_t queued_callback,
    evdsptc_event_callback_t begin_callback,
    evdsptc_event_callback_t end_callback);
```
    creates event dispatcher. dispatcher has a event dispatcher thread and an event queue.
    queued_callback is function pointer called by event publisher thread when the event queued. if set NULL, call nothing.
    begin_callback is function pointer called by event dispatcher thread when the event handler begin. if set NULL, call nothing.
    end_callback is function pointer called by event dispatcher thread when the handler event end. if set NULL, call nothing.
```c
void evdsptc_event_destroy (evdsptc_event_t* event);
```
```c
evdsptc_error_t evdsptc_event_init (evdsptc_event_t* event,
    evdsptc_handler_t event_handler,
    void* event_param,
    bool auto_destruct,
    evdsptc_listelem_destructor_t event_destructor);
```
```c
evdsptc_error_t evdsptc_post (evdsptc_context_t* context, evdsptc_event_t* event);
```
```c
evdsptc_error_t evdsptc_event_waitdone (evdsptc_event_t* event);
```
```c
void* evdsptc_event_getparam(evdsptc_event_t* event);
```
```c
void evdsptc_event_free (evdsptc_listelem_t* event);
```
```c
pthread_t* evdsptc_getthread(evdsptc_context_t* context);
```
```c
pthread_mutex_t* evdsptc_getmutex(evdsptc_context_t* context);
```
```c
void evdsptc_event_done (evdsptc_event_t* event);
```
```c
evdsptc_error_t evdsptc_destory (evdsptc_context_t* context, bool join);
```
```c
void evdsptc_list_init(evdsptc_list_t* list);
```
```c
bool evdsptc_list_is_empty(evdsptc_list_t* list);
```
```c
evdsptc_listelem_t* evdsptc_list_iterator(evdsptc_list_t* list);
```
```c
evdsptc_listelem_t* evdsptc_list_getlast(evdsptc_list_t* list);
```
```c
evdsptc_listelem_t* evdsptc_listelem_next(evdsptc_listelem_t* listelem);
```
```c
bool evdsptc_listelem_hasnext(evdsptc_listelem_t* listelem);
```
```c
evdsptc_listelem_t* evdsptc_listelem_insertnext(evdsptc_listelem_t* listelem, evdsptc_listelem_t* next);
```
```c
evdsptc_listelem_t* evdsptc_list_push(evdsptc_list_t* list, evdsptc_listelem_t* listelem);
```
```c
evdsptc_listelem_t* evdsptc_listelem_remove(evdsptc_listelem_t* listelem);
```
```c
evdsptc_listelem_t* evdsptc_list_pop(evdsptc_list_t* list);
```
```c
void evdsptc_list_destroy(evdsptc_list_t* list);
```

## Examples

* async event
    * See async_event_example, test/src/example.cpp 

* sync event
    * See sync_event_example, test/src/example.cpp 

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
