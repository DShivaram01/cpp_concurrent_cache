cmake_minimum_required(VERSION 3.20)
project(cpp_concurrent_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

add_library(cache src/lru_cache.cpp)
target_include_directories(cache PUBLIC include)
target_link_libraries(cache PUBLIC Threads::Threads)
target_compile_options(cache PRIVATE -Wall -Wextra -Wpedantic)

add_executable(cache_benchmark benchmarks/benchmark_cache.cpp)
target_link_libraries(cache_benchmark PRIVATE cache)
target_compile_options(cache_benchmark PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(cache_benchmark PRIVATE CACHE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(cache_tests tests/test_lru_cache.cpp)
target_link_libraries(cache_tests PRIVATE cache GTest::gtest_main)
target_compile_options(cache_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(concurrent_cache_tests tests/test_concurrent_cache.cpp)
target_link_libraries(concurrent_cache_tests PRIVATE cache GTest::gtest_main)
target_compile_options(concurrent_cache_tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(cache_tests)
gtest_discover_tests(concurrent_cache_tests)
