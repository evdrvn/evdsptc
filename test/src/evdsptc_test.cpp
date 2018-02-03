#include "evdsptc.h"

#include <stdio.h>

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>

#define USLEEP_TIMES (5 * 1000 * 5)
#define NUM_OF_USLEEP (10)
#define TIMER_INTERVAL_NS (50 * 1000)

static volatile int sem_event_queued_count = 0;
static volatile int sem_event_begin_count = 0;
static volatile int sem_event_handled_count = 0;
static volatile int sem_event_end_count = 0;
static volatile int blocking = 0;
static volatile int inc_event_count = 0;

TEST_GROUP(evdsptc_test_group){
    void setup(){
        sem_event_queued_count = 0;
        sem_event_begin_count = 0;
        sem_event_handled_count = 0;
        sem_event_end_count = 0;
        blocking = 0;
        inc_event_count = 0;
    }
    void teardown(){
        mock().checkExpectations();
        mock().clear();
    }
};

TEST_GROUP(evdsptc_list_test_group){
    void setup(){
    }
    void teardown(){
        mock().checkExpectations();
        mock().clear();
    }
};


static bool handle_sem_event(evdsptc_event_t *event){
    sem_t* sem = (sem_t*)evdsptc_event_getparam(event);
    mock().actualCall("handle_sem_event").onObject(event);
    sem_event_handled_count++;
    while (sem_wait(sem) == -1 && errno == EINTR) continue;  
    return true;
}

static bool handle_inc_event(evdsptc_event_t *event){
    (void)event;
    inc_event_count++;
    return true;
}

static bool handle_periodic_event(evdsptc_event_t *event){
    int* count = (int*)evdsptc_event_getparam(event);
    (*count)--;
    __sync_synchronize();
    inc_event_count++;
    if(0 == *count) return true;
    return false;
}

static void sem_event_queued(evdsptc_event_t* event){
    mock().actualCall("sem_event_queued").onObject(event);
    sem_event_queued_count++;
}

static void sem_event_begin(evdsptc_event_t* event){
    mock().actualCall("sem_event_begin").onObject(event);
    sem_event_begin_count++;
}

static void sem_event_end(evdsptc_event_t* event){
    mock().actualCall("sem_event_end").onObject(event);
    sem_event_end_count++;
}

static evdsptc_error_t init_sem_event (evdsptc_event_t** event, evdsptc_handler_t event_handler, sem_t** sem, bool free){
    evdsptc_error_t ret;
    evdsptc_event_destructor_t destructor = NULL;
    bool auto_destruct = false;

    *sem = (sem_t*)malloc(sizeof(sem_t));
    *event = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
    sem_init(*sem,0,0);
    if(free){
        destructor = evdsptc_event_free; 
        auto_destruct = true;
    }
    ret = evdsptc_event_init(*event, event_handler, (void*)*sem, auto_destruct, destructor);
    return ret;
}

static evdsptc_error_t init_inc_event (evdsptc_event_t** event, evdsptc_handler_t event_handler, bool free){
    evdsptc_error_t ret;
    evdsptc_event_destructor_t destructor = NULL;
    bool auto_destruct = false;

    *event = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
    if(free){
        destructor = evdsptc_event_free; 
        auto_destruct = true;
    }
    ret = evdsptc_event_init(*event, event_handler, NULL, auto_destruct, destructor);
    return ret;
}

static evdsptc_error_t init_periodic_event (evdsptc_event_t** event, evdsptc_handler_t event_handler, int** count, int left, bool free){
    evdsptc_error_t ret;
    evdsptc_event_destructor_t destructor = NULL;
    bool auto_destruct = false;

    *count = (int*)malloc(sizeof(int));
    *event = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
    **count = left;
    if(free){
        destructor = evdsptc_event_free; 
        auto_destruct = true;
    }
    ret = evdsptc_event_init(*event, event_handler, (void*)*count, auto_destruct, destructor);
    return ret;
}

static evdsptc_error_t init_timed_sem_event (evdsptc_event_t** event, evdsptc_handler_t event_handler, sem_t** sem, bool free, struct timespec* timer, evdsptc_timertype_t type){
    evdsptc_error_t ret;
    ret = init_sem_event (event, event_handler, sem, free);
    evdsptc_event_settimer(*event, timer, type);
    return ret;
}

static evdsptc_error_t post (evdsptc_context_t* context, evdsptc_event_t* event, bool block_to_done){
    evdsptc_error_t ret;
    if(block_to_done)blocking++;
    ret = evdsptc_post(context, event);
    if(block_to_done) ret = evdsptc_event_waitdone(event);
    if(block_to_done)blocking--;
    return ret;
}

TEST(evdsptc_test_group, post_test){
    evdsptc_context_t ctx;
    sem_t* sem[3];
    evdsptc_event_t* event[3];
    int i = 0;

    init_sem_event(&event[0], handle_sem_event, &sem[0], false);
    init_sem_event(&event[1], handle_sem_event, &sem[1], false);
    init_sem_event(&event[2], handle_sem_event, &sem[2], false);
    
    mock().expectOneCall("sem_event_queued").onObject(event[0]);
    mock().expectOneCall("sem_event_begin").onObject(event[0]);
    mock().expectOneCall("handle_sem_event").onObject(event[0]);
    
    mock().expectOneCall("sem_event_queued").onObject(event[1]);
    mock().expectOneCall("sem_event_queued").onObject(event[2]);

    evdsptc_create(&ctx, sem_event_queued, sem_event_begin, sem_event_end);
    post(&ctx, event[0], false);
    i = 0;
    while(sem_event_handled_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    
    post(&ctx, event[1], false);
    post(&ctx, event[2], false);
    
    mock().checkExpectations();
    
    mock().expectOneCall("sem_event_end").onObject(event[0]);

    mock().expectOneCall("sem_event_begin").onObject(event[1]);
    mock().expectOneCall("handle_sem_event").onObject(event[1]);
    mock().expectOneCall("sem_event_end").onObject(event[1]);

    mock().expectOneCall("sem_event_begin").onObject(event[2]);
    mock().expectOneCall("handle_sem_event").onObject(event[2]);
    mock().expectOneCall("sem_event_end").onObject(event[2]);
    
    sem_post(sem[0]);

    i = 0;
    while(sem_event_end_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    sem_post(sem[1]);
    sem_post(sem[2]);

    i = 0;
    while(sem_event_end_count < 3 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    mock().checkExpectations();
    POINTERS_EQUAL(NULL, ctx.list.root.prev);
    POINTERS_EQUAL(NULL, ctx.list.root.next);

    evdsptc_destory(&ctx, true); 

    free(sem[0]);
    free(sem[1]);
    free(sem[2]);
    free(event[0]);
    free(event[1]);
    free(event[2]);
}

TEST(evdsptc_test_group, destroy_test){
    evdsptc_context_t ctx;
    sem_t* sem[3];
    evdsptc_event_t* event[3];
    int i = 0;

    init_sem_event(&event[0], handle_sem_event, &sem[0], false);
    init_sem_event(&event[1], handle_sem_event, &sem[1], true);
    init_sem_event(&event[2], handle_sem_event, &sem[2], true);

    mock().expectOneCall("sem_event_queued").onObject(event[0]);
    mock().expectOneCall("sem_event_begin").onObject(event[0]);
    mock().expectOneCall("handle_sem_event").onObject(event[0]);

    mock().expectOneCall("sem_event_queued").onObject(event[1]);
    mock().expectOneCall("sem_event_queued").onObject(event[2]);

    mock().expectOneCall("sem_event_end").onObject(event[0]);

    evdsptc_create(&ctx, sem_event_queued, sem_event_begin, sem_event_end);
    post(&ctx, event[0], false);
    i = 0;
    while(sem_event_handled_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    post(&ctx, event[1], false);
    post(&ctx, event[2], false);

    evdsptc_destory(&ctx, false);

    sem_post(sem[0]);

    i = 0;
    while(sem_event_end_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    mock().checkExpectations();
    POINTERS_EQUAL(NULL, ctx.list.root.prev);
    POINTERS_EQUAL(NULL, ctx.list.root.next);

    free(sem[0]);
    free(sem[1]);
    free(sem[2]);
    free(event[0]);
}

struct async_post_param {
    evdsptc_context_t* context;
    evdsptc_event_t* event;
    bool block_to_done;
};

static void* async_post_routine (void* _param){
    struct async_post_param *param = (struct async_post_param*)_param;
    return (void*)post(param->context, param->event, param->block_to_done);
}

static void async_post (pthread_t *th, struct async_post_param* param){
    pthread_create(th, NULL, &async_post_routine, (void*) param);
}

TEST(evdsptc_test_group, block_to_done_test){
    evdsptc_context_t ctx;
    evdsptc_error_t ret;
    sem_t* sem[3];
    evdsptc_event_t* event[3];
    int i = 0;
    struct async_post_param param[3];
    pthread_t th[3];
    
    init_sem_event(&event[0], handle_sem_event, &sem[0], false);
    init_sem_event(&event[1], handle_sem_event, &sem[1], false);
    init_sem_event(&event[2], handle_sem_event, &sem[2], false);

    param[0].context = &ctx;
    param[0].event = event[0];
    param[0].block_to_done = false;
    param[1].context = &ctx;
    param[1].event = event[1];
    param[1].block_to_done = true;
    param[2].context = &ctx;
    param[2].event = event[2];
    param[2].block_to_done = true;
    
    mock().expectOneCall("sem_event_queued").onObject(event[0]);
    mock().expectOneCall("sem_event_begin").onObject(event[0]);
    mock().expectOneCall("handle_sem_event").onObject(event[0]);
    
    mock().expectOneCall("sem_event_queued").onObject(event[1]);
    mock().expectOneCall("sem_event_queued").onObject(event[2]);

    ret = evdsptc_create(&ctx, sem_event_queued, sem_event_begin, sem_event_end);
    async_post(&th[0], &param[0]);
    
    i = 0;
    while(sem_event_handled_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    async_post(&th[1], &param[1]);
    i = 0;
    while(sem_event_queued_count < 2 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(1, blocking);
    
    async_post(&th[2], &param[2]);
    i = 0;
    while(sem_event_queued_count < 3 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(2, blocking);

    mock().checkExpectations();
    mock().expectOneCall("sem_event_end").onObject(event[0]);
    mock().expectOneCall("sem_event_begin").onObject(event[1]);
    mock().expectOneCall("handle_sem_event").onObject(event[1]);

    sem_post(sem[0]);
    
    i = 0;
    while(sem_event_end_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    i = 0;
    while(sem_event_begin_count < 2 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    POINTERS_EQUAL(event[2], (evdsptc_event_t*)ctx.list.root.next);
    POINTERS_EQUAL(NULL, ctx.list.root.next->next);
   
    i = 0;
    while(sem_event_queued_count < 3 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(2, blocking);
 
    ret = evdsptc_destory(&ctx, false); 
    CHECK_EQUAL(false, event[0]->is_canceled);
    CHECK_EQUAL(false, event[1]->is_canceled);
    CHECK_EQUAL(true , event[2]->is_canceled);
    
    mock().checkExpectations();
    mock().expectOneCall("sem_event_end").onObject(event[1]);

    sem_post(sem[1]);

    i = 0;
    while(blocking > 0 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(0, blocking);
    mock().checkExpectations();
    
    pthread_join(th[0], (void**)&ret);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);
    pthread_join(th[1], (void**)&ret);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);
    pthread_join(th[2], (void**)&ret);
    CHECK_EQUAL(EVDSPTC_ERROR_CANCELED, ret);
    
    POINTERS_EQUAL(NULL, ctx.list.root.next);
    POINTERS_EQUAL(NULL, ctx.list.root.prev);

    free(evdsptc_event_getparam(event[0]));
    free(evdsptc_event_getparam(event[1]));
    free(evdsptc_event_getparam(event[2]));
    free(event[0]);
    free(event[1]);
    free(event[2]);
}

static int count_forward(evdsptc_list_t* list){
    int ret = 0;
    evdsptc_listelem_t* i = evdsptc_list_iterator(list);
    while(evdsptc_listelem_hasnext(i)){
        i = evdsptc_listelem_next(i);
        ret++;
    }
    return ret;
}

static int count_reverse(evdsptc_list_t* list){
    int ret = 0;
    evdsptc_listelem_t* i = evdsptc_list_getlast(list);
    if(i == NULL) return ret;
    ret++;
    while((i = i->prev) != NULL){
        ret++;
    }
    return ret;
}

TEST(evdsptc_list_test_group, list_test){
    evdsptc_list_t list;
    evdsptc_listelem_t elem[3];

    evdsptc_list_init(&list);
    evdsptc_list_push(&list, &elem[0]);
    evdsptc_list_push(&list, &elem[1]);
    evdsptc_list_push(&list, &elem[2]);

    CHECK_EQUAL(3, count_forward(&list));
    CHECK_EQUAL(3, count_reverse(&list));

    evdsptc_listelem_remove(&elem[1]);

    CHECK_EQUAL(2, count_forward(&list));
    CHECK_EQUAL(2, count_reverse(&list));
    
    evdsptc_listelem_insertnext(&elem[0], &elem[1]);

    CHECK_EQUAL(3, count_forward(&list));
    CHECK_EQUAL(3, count_reverse(&list));

    evdsptc_listelem_remove(&elem[0]);

    CHECK_EQUAL(2, count_forward(&list));
    CHECK_EQUAL(2, count_reverse(&list));

    evdsptc_listelem_insertnext(evdsptc_list_iterator(&list), &elem[0]);

    CHECK_EQUAL(3, count_forward(&list));
    CHECK_EQUAL(3, count_reverse(&list));
}

TEST(evdsptc_test_group, post_timer_test){
    evdsptc_context_t ctx;
    sem_t* sem[3];
    evdsptc_event_t* event[3];
    int i = 0;

    struct timespec intv = {0, TIMER_INTERVAL_NS};
    struct timespec timer1, timer2, timer3;
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now); 
    timer1 = evdsptc_timespec_add(&now, &intv);
    init_timed_sem_event(&event[0], handle_sem_event, &sem[0], false, &timer1, EVDSPTC_TIMERTYPE_ABSOLUTE);
    init_timed_sem_event(&event[1], handle_sem_event, &sem[1], false, &intv, EVDSPTC_TIMERTYPE_RELATIVE);
    init_timed_sem_event(&event[2], handle_sem_event, &sem[2], false, &intv, EVDSPTC_TIMERTYPE_RELATIVE);
   
    mock().expectOneCall("sem_event_queued").onObject(event[0]);
    mock().expectOneCall("sem_event_begin").onObject(event[0]);
    mock().expectOneCall("handle_sem_event").onObject(event[0]);
    
    mock().expectOneCall("sem_event_queued").onObject(event[1]);
    mock().expectOneCall("sem_event_queued").onObject(event[2]);
    
    evdsptc_create(&ctx, sem_event_queued, sem_event_begin, sem_event_end);
    
    post(&ctx, event[0], false);
  
    i = 0;
    while(sem_event_handled_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    clock_gettime(CLOCK_REALTIME, &now);
    CHECK(evdsptc_timespec_compare(&timer1, &now) <= 0);

    clock_gettime(CLOCK_REALTIME, &now); 
    timer2 = evdsptc_timespec_add(&now, &intv);
    post(&ctx, event[1], false);

    clock_gettime(CLOCK_REALTIME, &now); 
    timer3 = evdsptc_timespec_add(&now, &intv);
    post(&ctx, event[2], false);
   
    mock().checkExpectations();

    mock().expectOneCall("sem_event_end").onObject(event[0]);

    mock().expectOneCall("sem_event_begin").onObject(event[1]);
    mock().expectOneCall("handle_sem_event").onObject(event[1]);
    mock().expectOneCall("sem_event_end").onObject(event[1]);

    mock().expectOneCall("sem_event_begin").onObject(event[2]);
    mock().expectOneCall("handle_sem_event").onObject(event[2]);
    mock().expectOneCall("sem_event_end").onObject(event[2]);
    
    sem_post(sem[0]);

    i = 0;
    while(sem_event_end_count < 1 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    i = 0;
    while(sem_event_handled_count < 2 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    clock_gettime(CLOCK_REALTIME, &now);
    CHECK(evdsptc_timespec_compare(&timer2, &now) <= 0);
    
    sem_post(sem[1]);

    i = 0;
    while(sem_event_handled_count < 3 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);
    clock_gettime(CLOCK_REALTIME, &now);
    CHECK(evdsptc_timespec_compare(&timer3, &now) <= 0);
    
    sem_post(sem[2]);

    i = 0;
    while(sem_event_end_count < 3 && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    mock().checkExpectations();
    POINTERS_EQUAL(NULL, ctx.list.root.prev);
    POINTERS_EQUAL(NULL, ctx.list.root.next);

    evdsptc_destory(&ctx, true); 

    for(i = 0; i < 3; i++){
        free(sem[i]);
        free(event[i]);
    }
}

TEST(evdsptc_test_group, post_timer_mixed_test){
    evdsptc_context_t ctx;
    sem_t* sem[3];
    evdsptc_event_t* event[100];
    int i = 0;
    int inc_event_posted = 0;

    struct timespec intv = {0, TIMER_INTERVAL_NS};
    struct timespec timer1, timer2, timer3;
    struct timespec now;

    for(i = 3; i < 100; i++){
        init_inc_event(&event[i], handle_inc_event, false);
    }

    clock_gettime(CLOCK_REALTIME, &now); 
    timer1 = evdsptc_timespec_add(&now, &intv);

    init_timed_sem_event(&event[0], handle_sem_event, &sem[0], false, &timer1, EVDSPTC_TIMERTYPE_ABSOLUTE);
    init_timed_sem_event(&event[1], handle_sem_event, &sem[1], false, &intv, EVDSPTC_TIMERTYPE_RELATIVE);
    init_timed_sem_event(&event[2], handle_sem_event, &sem[2], false, &intv, EVDSPTC_TIMERTYPE_RELATIVE);

    sem_post(sem[0]);
    sem_post(sem[1]);
    sem_post(sem[2]);
   
    mock().expectOneCall("handle_sem_event").onObject(event[0]);
    mock().expectOneCall("handle_sem_event").onObject(event[1]);
    mock().expectOneCall("handle_sem_event").onObject(event[2]);
    
    evdsptc_create(&ctx, NULL, NULL, NULL);

    while(inc_event_posted < 7){
        post(&ctx, event[3 + inc_event_posted], false);
        inc_event_posted++;
    }  
    post(&ctx, event[0], false);
    i = 0;
    while(sem_event_handled_count < 1 && i++ < USLEEP_TIMES) {
        if(inc_event_posted < 10){
            post(&ctx, event[3 + inc_event_posted], false);
            inc_event_posted++;
        }
        if(i % 5 == 0) usleep(NUM_OF_USLEEP);
    }
    clock_gettime(CLOCK_REALTIME, &now);
    //printf("\nposted= %03d", inc_event_posted);
    //printf("\ncount = %03d", inc_event_count);
    //printf("\nnow   = %010d.%09d", (int)now.tv_sec, (int)now.tv_nsec);
    //printf("\ntimer1= %010d.%09d", (int)timer1.tv_sec, (int)timer1.tv_nsec);
    //printf("\nevent1= %010d.%09d\n", (int)event[0]->timer.tv_sec, (int)event[0]->timer.tv_nsec);
    CHECK(evdsptc_timespec_compare(&timer1, &now) <= 0);

    while(inc_event_posted < 20){
        post(&ctx, event[3 + inc_event_posted], false);
        inc_event_posted++;
    }  
    
    clock_gettime(CLOCK_REALTIME, &now); 
    timer2 = evdsptc_timespec_add(&now, &intv);
    post(&ctx, event[1], false);
    
    usleep(NUM_OF_USLEEP);
    
    clock_gettime(CLOCK_REALTIME, &now); 
    timer3 = evdsptc_timespec_add(&now, &intv);
    post(&ctx, event[2], false);

    while(inc_event_posted < 50){
        post(&ctx, event[3 + inc_event_posted], false);
        inc_event_posted++;
    }

    i = 0;
    while(sem_event_handled_count < 2 && i++ < USLEEP_TIMES) {
        if(inc_event_posted < 80){
            post(&ctx, event[3 + inc_event_posted], false);
            inc_event_posted++;
        }
        if(i % 5 == 0) usleep(NUM_OF_USLEEP);
    }
    clock_gettime(CLOCK_REALTIME, &now);
    //printf("\nposted= %03d", inc_event_posted);
    //printf("\ncount = %03d", inc_event_count);
    //printf("\ntimer2= %010d.%09d", (int)timer2.tv_sec, (int)timer2.tv_nsec);
    //printf("\nevent2= %010d.%09d\n", (int)event[1]->timer.tv_sec, (int)event[1]->timer.tv_nsec);
    CHECK(evdsptc_timespec_compare(&timer2, &now) <= 0);

    i = 0;
    while(sem_event_handled_count < 3 && i++ < USLEEP_TIMES) {
        if(inc_event_posted < 90){
            post(&ctx, event[3 + inc_event_posted], false);
            inc_event_posted++;
        }
        if(i % 5 == 0) usleep(NUM_OF_USLEEP);
    }
    clock_gettime(CLOCK_REALTIME, &now);
    //printf("\nposted= %03d", inc_event_posted);
    //printf("\ncount = %03d", inc_event_count);
    //printf("\nnow   = %010d.%09d", (int)now.tv_sec, (int)now.tv_nsec);
    //printf("\ntimer3= %010d.%09d", (int)timer3.tv_sec, (int)timer3.tv_nsec);
    //printf("\nevent3= %010d.%09d\n", (int)event[2]->timer.tv_sec, (int)event[2]->timer.tv_nsec);
    CHECK(evdsptc_timespec_compare(&timer3, &now) <= 0);

    mock().checkExpectations();
    
    while(inc_event_posted < 97){
        post(&ctx, event[3 + inc_event_posted], false);
        inc_event_posted++;
    }

    i = 0;
    while(inc_event_count < inc_event_posted && i++ < USLEEP_TIMES) usleep(NUM_OF_USLEEP);

    POINTERS_EQUAL(NULL, ctx.list.root.prev);
    POINTERS_EQUAL(NULL, ctx.list.root.next);

    evdsptc_destory(&ctx, true); 

    for(i = 0; i < 100; i++){
        if(i < 3) free(sem[i]);
        free(event[i]);
    }
}

TEST(evdsptc_test_group, periodic_test){
    evdsptc_context_t ctx;
    int* count[3];
    evdsptc_event_t* event[3];
    struct timespec intv = {0, 1000 * 1000};
    int i = 0;
    struct timespec target, now;
    
    init_periodic_event(&event[0], handle_periodic_event, &count[0], 10, false);
    init_periodic_event(&event[1], handle_periodic_event, &count[1], 20, false);
    init_periodic_event(&event[2], handle_periodic_event, &count[2], 30, false);

    evdsptc_create_periodic(&ctx, NULL, NULL, NULL, &intv);
    clock_gettime(CLOCK_MONOTONIC, &target);
    post(&ctx, event[0], false);
    post(&ctx, event[1], false);
    post(&ctx, event[2], false);

    for(i = 0; i < 10; i++){
        int j = 0;
        while(*(count[0]) > (10 - 1 - i) && j++ < (USLEEP_TIMES / 5)) usleep(NUM_OF_USLEEP);
        target = evdsptc_timespec_add(&target, &intv);
        clock_gettime(CLOCK_MONOTONIC, &now);
        CHECK(evdsptc_timespec_compare(&target, &now) <= 0);
    }

    i = 0;
    while(inc_event_count < 60 && i++ < (USLEEP_TIMES / 5)) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(60, inc_event_count);

    evdsptc_destory(&ctx, true); 

    for(i = 0; i < 3; i++){
        free(count[i]);
        free(event[i]);
    }
}

TEST(evdsptc_test_group, periodic_destroy_test){
    evdsptc_context_t ctx;
    int* count[3];
    evdsptc_event_t* event[3];
    struct timespec intv = {0, 1000 * 1000};
    int i = 0;
    struct timespec target, now;
    
    init_periodic_event(&event[0], handle_periodic_event, &count[0], 10, false);
    init_periodic_event(&event[1], handle_periodic_event, &count[1], 20, false);
    init_periodic_event(&event[2], handle_periodic_event, &count[2], 30, false);

    evdsptc_create_periodic(&ctx, NULL, NULL, NULL, &intv);
    clock_gettime(CLOCK_MONOTONIC, &target);
    post(&ctx, event[0], false);
    post(&ctx, event[1], false);
    post(&ctx, event[2], false);

    for(i = 0; i < 10; i++){
        int j = 0;
        while(*(count[0]) > (10 - 1 - i) && j++ < (USLEEP_TIMES / 5)) usleep(NUM_OF_USLEEP);
        target = evdsptc_timespec_add(&target, &intv);
        clock_gettime(CLOCK_MONOTONIC, &now);
        CHECK(evdsptc_timespec_compare(&target, &now) <= 0);
    }

    evdsptc_destory(&ctx, true); 

    for(i = 0; i < 3; i++){
        free(count[i]);
        free(event[i]);
    }
}

int main(int ac, char** av){
    return CommandLineTestRunner::RunAllTests(ac, av);
}


