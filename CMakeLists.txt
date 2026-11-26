cmake_minimum_required(VERSION 3.14)
project(ConcurrentMemoryPool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

find_package(Threads REQUIRED)

set(POOL_SOURCES
    src/common.h
    src/ObjectPool.h
    src/PageMap.h
    src/ThreadCache.h
    src/Centralcache.h
    src/Pagecache.h
    src/ConcurrentAlloc.h
    src/ThreadCache.cpp
    src/Centralcache.cpp
    src/Pagecache.cpp
)

add_library(mempool STATIC ${POOL_SOURCES})
target_include_directories(mempool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mempool PUBLIC Threads::Threads)

# Usage: ./benchmark <ops-per-round> <thread-count> <rounds>
add_executable(benchmark src/BenchMark.cpp)
target_link_libraries(benchmark PRIVATE mempool)

# Functional + assertion-based unit tests
add_executable(unittest src/Unittest.cpp)
target_link_libraries(unittest PRIVATE mempool)

# Smoke-test driver
add_executable(smoke src/Test.cpp)
target_link_libraries(smoke PRIVATE mempool)

enable_testing()
add_test(NAME unit_tests COMMAND unittest)

message(STATUS "ConcurrentMemoryPool configured (${CMAKE_BUILD_TYPE})")
