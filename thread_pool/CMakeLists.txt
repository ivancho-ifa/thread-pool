cmake_minimum_required(VERSION 3.8)


add_library(ThreadPool SHARED
        thread_pool.hpp thread_pool.cpp "data_structures/thread_safe/lock_based/queue.hpp" "util/boost.hpp")

set(Boost_ADDITIONAL_VERSIONS "1.73.0" "1.73.0")
find_package(Boost 1.73 REQUIRED COMPONENTS timer)
target_include_directories(ThreadPool PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_definitions(ThreadPool PRIVATE "BOOST_TEST_DYN_LINK=1")
target_link_libraries(ThreadPool ${Boost_TIMER_LIBRARY})

include(GenerateExportHeader)
GENERATE_EXPORT_HEADER(ThreadPool)
target_include_directories(ThreadPool PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(ThreadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})