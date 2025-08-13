cmake_minimum_required(VERSION 3.14)
project(TaskSchedulerTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE GTest::gtest_main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
