include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.11.0
)
FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED
        ../benchmark/benchmark_queue.cpp
)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

add_executable(test_spmc test_spmc.cpp
)

target_link_libraries(test_spmc
        PRIVATE
        GTest::GTest
        spmc)

add_test(spmc_queue_test test_spmc)