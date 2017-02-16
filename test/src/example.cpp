#include "evdsptc.h"

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>

#define USLEEP_PERIOD (10000)
#define NUM_OF_USLEEP (10)

static volatile int sum = 0;

static bool add_int(void * param){
    sum += (int)param;
    return true; // set true if done
}

TEST_GROUP(example_group){
    void setup(){
        sum = 0;
    }
    void teardown(){
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(example_group, async_event_example){

    evdsptc_error_t ret;
    evdsptc_context_t ctx;
    evdsptc_event_t* ev;

    ret = evdsptc_create(&ctx, NULL, NULL, NULL);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);

    ev = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
    evdsptc_event_init(ev, add_int, (void*)100, false, true, evdsptc_event_free);
    
    ret = evdsptc_post(&ctx, ev);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);

    //wait to handle async event
    int i = 0;
    while(sum < 100 && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
    CHECK_EQUAL(100, sum);
    
    evdsptc_destory(&ctx, true); 
}

TEST(example_group, sync_event_example){

    evdsptc_error_t ret;
    evdsptc_context_t ctx;
    evdsptc_event_t ev;

    ret = evdsptc_create(&ctx, NULL, NULL, NULL);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);

    evdsptc_event_init(&ev, add_int, (void*)100, true, false, NULL);
    
    ret = evdsptc_post(&ctx, &ev);
    CHECK_EQUAL(EVDSPTC_ERROR_NONE, ret);
    CHECK_EQUAL(100, sum);
    
    evdsptc_destory(&ctx, true); 
}


