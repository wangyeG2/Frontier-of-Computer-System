cmake_minimum_required(VERSION 3.10)
project(LevelDB_Lab)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")

find_path(LEVELDB_INCLUDE_DIR leveldb/db.h)
find_library(LEVELDB_LIBRARY leveldb)

include_directories(${LEVELDB_INCLUDE_DIR} include)

add_executable(task1_benchmark src/task1_benchmark.cpp)
target_link_libraries(task1_benchmark ${LEVELDB_LIBRARY} pthread)

add_executable(task2_latency src/task2_latency.cpp)
target_link_libraries(task2_latency ${LEVELDB_LIBRARY} pthread)

add_executable(task3_cache src/task3_cache.cpp)
target_link_libraries(task3_cache ${LEVELDB_LIBRARY} pthread)