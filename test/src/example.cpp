#include "evdsptc.h"

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>

#define USLEEP_PERIOD (10000)
#define NUM_OF_USLEEP (10)

static volatile int sum = 0;
static volatile int count = 0;

static bool hello(evdsptc_event_t* event){
    printf("hello %s\n", (char*)evdsptc_event_getparam(event));
    return true;
}

static bool add_int(evdsptc_event_t* event){
    sum += (int)evdsptc_event_getparam(event);
    return true; // set true if done
}

TEST_GROUP(example_group){
    void setup(){
        sum = 0;
        count = 0;
    }
    void teardown(){
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(example_group, hello_world_example){
    evdsptc_context_t ctx;
    evdsptc_event_t ev;

    char param[10] = "world";

    evdsptc_create(&ctx, NULL, NULL, NULL);

    evdsptc_event_init(&ev, hello, (void*)param, false, NULL);
    evdsptc_post(&ctx, &ev);
    evdsptc_event_waitdone(&ev);

    evdsptc_destory(&ctx, true);
}

TEST(example_group, async_event_example){

    evdsptc_context_t ctx;
    evdsptc_event_t* ev;
    int i = 0;
    int sum_expected = 0;

    evdsptc_create(&ctx, NULL, NULL, NULL);

    for(i = 0; i <= 10; i++){
        ev = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
        evdsptc_event_init(ev, add_int, (void*)i, true, evdsptc_event_free);
        evdsptc_post(&ctx, ev);
        sum_expected += i;
    }

    //wait to have be handled async event
    i = 0;
    while(sum < sum_expected && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(sum_expected, sum);

    evdsptc_destory(&ctx, true); 
}

TEST(example_group, sync_event_example){

    evdsptc_context_t ctx;
    evdsptc_event_t ev[11];
    int i = 0;
    int sum_expected = 0;

    evdsptc_create(&ctx, NULL, NULL, NULL);

    for(i = 0; i <= 10; i++){
        evdsptc_event_init(&ev[i], add_int, (void*)i, false, NULL);
        evdsptc_post(&ctx, &ev[i]);
        evdsptc_event_waitdone(&ev[i]);
        sum_expected += i;
        CHECK_EQUAL(sum_expected, sum);
    }

    evdsptc_destory(&ctx, true); 
}

static bool add_int_and_suspend(evdsptc_event_t* event){
    sum += (int)evdsptc_event_getparam(event);
    return false; // set false if suspend 
}

struct send_target {
    evdsptc_context_t* ctx;
    evdsptc_handler_t handler;
    evdsptc_event_t* ev;
};

static bool post_and_wait_to_done(evdsptc_event_t* event){
    struct send_target *target;

    target = (struct send_target*)evdsptc_event_getparam(event);
    evdsptc_event_init(target->ev, target->handler, (void*)(count + 1), true, evdsptc_event_free);
    evdsptc_post(target->ctx, target->ev);
    evdsptc_event_waitdone(target->ev);
    count++;

    return true;
}


TEST(example_group, async_event_done_example){

    evdsptc_context_t ctx_send;
    evdsptc_context_t ctx_recv;
    evdsptc_event_t ev_parent[2];
    evdsptc_event_t* ev_child[2];
    struct send_target no_suspended;
    struct send_target suspended;
    int i = 0;
 
    ev_child[0] = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
    ev_child[1] = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
    
    evdsptc_create(&ctx_recv, NULL, NULL, NULL);
    evdsptc_create(&ctx_send, NULL, NULL, NULL);

    no_suspended.ctx = &ctx_recv;
    no_suspended.handler =  add_int;
    no_suspended.ev = ev_child[0];

    suspended.ctx = &ctx_recv;
    suspended.handler =  add_int_and_suspend;
    suspended.ev = ev_child[1];    
    
    evdsptc_event_init(&ev_parent[0], post_and_wait_to_done, (void*)&no_suspended, false, NULL);
    evdsptc_post(&ctx_send, &ev_parent[0]);

    evdsptc_event_init(&ev_parent[1], post_and_wait_to_done, (void*)&suspended, false, NULL);
    evdsptc_post(&ctx_send, &ev_parent[1]);

    //wait to have be handled async event
    i = 0;
    while(sum < 3 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(3, sum);

    //check done count
    i = 0;
    while(count < 1 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(1, count);

    evdsptc_event_done(suspended.ev);

    //check done count
    i = 0;
    while(count < 2 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(2, count);

    evdsptc_destory(&ctx_recv, true); 
    evdsptc_destory(&ctx_send, true); 
    
    evdsptc_event_destroy(suspended.ev); 
}

typedef struct {
    evdsptc_listelem_t listelem;
    int* pint;
} int_listelem_t;

static void int_listelem_destructor(evdsptc_listelem_t* listelem){
    int_listelem_t* pint_listelem = (int_listelem_t*)listelem;
    free(pint_listelem->pint);
    free(pint_listelem);
}

TEST(example_group, list_bubble_sort_example){
    int list_size = 10;
    int i, j;
    evdsptc_list_t list;
    evdsptc_listelem_t* iterator;
    evdsptc_listelem_t* next;
    int_listelem_t* pint_listelem;

    evdsptc_list_init(&list);

    for(i = 0; i < list_size; i++){
       pint_listelem = (int_listelem_t*)malloc(sizeof(int_listelem_t));
       evdsptc_listelem_setdestructor(&pint_listelem->listelem, int_listelem_destructor);
       pint_listelem->pint = (int*)malloc(sizeof(int));
       *(pint_listelem->pint) = list_size - i;
       evdsptc_list_push(&list, (evdsptc_listelem_t*)pint_listelem);
    }

    //bubble sort
    for(i = 0; i < list_size - 1; i++){
        iterator = evdsptc_list_iterator(&list);
        for(j = 0; j < list_size - i - 1; j++){
            iterator = evdsptc_listelem_next(iterator);
            next     = evdsptc_listelem_next(iterator);
            if(*(((int_listelem_t*)next    )->pint) <
               *(((int_listelem_t*)iterator)->pint)){
                evdsptc_listelem_insertnext(next, evdsptc_listelem_remove(iterator));
                iterator = next;
            }
        }
    }

    //check
    i = 1; 
    iterator = evdsptc_list_iterator(&list);
    while(evdsptc_listelem_hasnext(iterator)){
        iterator = evdsptc_listelem_next(iterator);
        pint_listelem = (int_listelem_t*)iterator;
        j = *(pint_listelem->pint);
        CHECK_EQUAL(i, j);
        i++; 
    }

    evdsptc_list_destroy(&list);
}
