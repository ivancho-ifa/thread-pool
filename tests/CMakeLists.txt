cmake_minimum_required(VERSION 2.8.11)

add_executable(ThreadPoolTests main.cpp "test_thread_pool.cpp" "test_queue.cpp")

# Link Boost libraries

set(Boost_ADDITIONAL_VERSIONS "1.73.0" "1.73.0")

find_package(Boost 1.73 REQUIRED COMPONENTS timer unit_test_framework)

target_include_directories(ThreadPoolTests PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_definitions(ThreadPoolTests PRIVATE "BOOST_TEST_DYN_LINK=1")
target_link_libraries(ThreadPoolTests ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
target_link_libraries(ThreadPoolTests ${Boost_TIMER_LIBRARY})

# Link ThreadPool

target_include_directories(ThreadPoolTests PRIVATE ThreadPool)
target_link_libraries(ThreadPoolTests ThreadPool)


enable_testing()

add_test(NAME "ThreadPoolTest" COMMAND ${RUNTIME_OUTPUT_DIRECTORY}/ThreadPoolTests.exe)