#include "evdsptc.h"

#include <stdio.h>

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>

#define USLEEP_PERIOD (10000)
#define NUM_OF_USLEEP (10)

static volatile int sem_event_queued_count = 0;
static volatile int sem_event_begin_count = 0;
static volatile int sem_event_handled_count = 0;
static volatile int sem_event_end_count = 0;
static volatile int blocking = 0;

TEST_GROUP(evdsptc_test_group){
    void setup(){
        sem_event_queued_count = 0;
        sem_event_begin_count = 0;
        sem_event_handled_count = 0;
        sem_event_end_count = 0;
        blocking = 0;
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
    while(sem_event_handled_count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    
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

    while(sem_event_end_count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);

    sem_post(sem[1]);
    sem_post(sem[2]);

    while(sem_event_end_count < 3 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);

    mock().checkExpectations();
    CHECK(ctx.list.root.prev == NULL);
    CHECK(ctx.list.root.next == NULL);

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
    while(sem_event_handled_count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);

    post(&ctx, event[1], false);
    post(&ctx, event[2], false);

    evdsptc_destory(&ctx, false);

    sem_post(sem[0]);

    while(sem_event_end_count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);

    mock().checkExpectations();
    CHECK(ctx.list.root.prev == NULL);
    CHECK(ctx.list.root.next == NULL);

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
    
    while(sem_event_handled_count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);

    async_post(&th[1], &param[1]);
    while(sem_event_queued_count < 2 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(1, blocking);
    
    async_post(&th[2], &param[2]);
    while(sem_event_queued_count < 3 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(2, blocking);

    mock().checkExpectations();
    mock().expectOneCall("sem_event_end").onObject(event[0]);
    mock().expectOneCall("sem_event_begin").onObject(event[1]);
    mock().expectOneCall("handle_sem_event").onObject(event[1]);

    sem_post(sem[0]);
    
    while(sem_event_end_count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    while(sem_event_begin_count < 2 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(event[2], (evdsptc_event_t*)ctx.list.root.next);
    CHECK(NULL == ctx.list.root.next->next);
    
    ret = evdsptc_destory(&ctx, false); 
    CHECK_EQUAL(false, event[0]->is_canceled);
    CHECK_EQUAL(false, event[1]->is_canceled);
    CHECK_EQUAL(true , event[2]->is_canceled);
    
    mock().checkExpectations();
    mock().expectOneCall("sem_event_end").onObject(event[1]);

    sem_post(sem[1]);

    while(blocking > 0 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(0, blocking);
    mock().checkExpectations();
    
    pthread_join(th[0], (void**)&ret);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);
    pthread_join(th[1], (void**)&ret);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);
    pthread_join(th[2], (void**)&ret);
    CHECK_EQUAL(EVDSPTC_ERROR_CANCELED, ret);
    
    CHECK(ctx.list.root.next == NULL);
    CHECK(ctx.list.root.prev == NULL);

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

int main(int ac, char** av){
    return CommandLineTestRunner::RunAllTests(ac, av);
}


