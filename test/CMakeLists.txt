cmake_minimum_required(VERSION 2.8)
add_executable(evdsptc_tests src/evdsptc_test.cpp src/example.cpp)

target_compile_options(evdsptc_tests PRIVATE "-Wall")
target_include_directories(evdsptc_tests PRIVATE ../src)
target_include_directories(evdsptc_tests PRIVATE ext/cpputest/include)

add_library(evdsptc STATIC ../src/evdsptc.c)
find_library(CppUTest NAMES libCppUTest.a PATHS ext/cpputest/lib)
find_library(CppUTestExt NAMES libCppUTestExt.a PATHS ext/cpputest/lib)

target_link_libraries(evdsptc_tests evdsptc)
target_link_libraries(evdsptc_tests ${CppUTest})
target_link_libraries(evdsptc_tests ${CppUTestExt})
target_link_libraries(evdsptc_tests gcov)
target_link_libraries(evdsptc_tests pthread)
