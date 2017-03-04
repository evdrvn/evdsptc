evdsptc
=======

sync/async event dispatcher for C/C++

## Getting Started

* Building from source

    ```shell
    cd build
    cmake ..
    make
    ```

## Examples

* async event
    * See async_event_example, test/src/example.cpp 
        ```cpp
        static volatile int sum = 0;
        
        static bool add_int(evdsptc_event_t* event){   
            sum += (int)evdsptc_event_getparam(event); 
            return true; // set true if done
        }
        
        TEST(example_group, async_event_example){
        
            evdsptc_context_t ctx;
            evdsptc_event_t* ev;
            int i = 0;
            int sum_expected = 0;
        
            evdsptc_create(&ctx, NULL, NULL, NULL);
        
            for(i = 0; i <= 10; i++){
                ev = (evdsptc_event_t*)malloc(sizeof(evdsptc_event_t));
                evdsptc_event_init(ev, add_int, (void*)i, false, true, evdsptc_event_free);
                evdsptc_post(&ctx, ev);
                sum_expected += i;
            }
        
            //wait to have be handled async event
            i = 0;
            while(sum < sum_expected && i++ < USLEEP_PERIOD) usleep(NUM_OF_USLEEP);
            CHECK_EQUAL(sum_expected, sum);
        
            evdsptc_destory(&ctx, true);
        }
        
        ```

* sync event
    * See sync_event_example, test/src/example.cpp 
        ```cpp
        TEST(example_group, sync_event_example){
        
            evdsptc_context_t ctx;
            evdsptc_event_t ev[11];
            int i = 0;
            int sum_expected = 0;
        
            evdsptc_create(&ctx, NULL, NULL, NULL);
        
            for(i = 0; i <= 10; i++){
                evdsptc_event_init(&ev[i], add_int, (void*)i, true, false, NULL);
                evdsptc_post(&ctx, &ev[i]);
                sum_expected += i;
            }
        
            CHECK_EQUAL(sum_expected, sum);
        
            evdsptc_destory(&ctx, true);
        }
        
        ```

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
