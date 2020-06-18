#define BOOST_TEST_DYN_LINK

#include <data_structures/thread_safe/lock_based/queue.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

using boost::timer::nanosecond_type;
using thread_pool::data_structures::thread_safe::lock_based::std_queue;
using thread_pool::data_structures::thread_safe::lock_based::queue;

template<typename T>
nanosecond_type measure_performance() {
	boost::timer::cpu_timer timer;
	timer.start();
	
	T queue;
	
	std::thread reader{[&queue]() {
		for (int i = 0; i < 1000; ++i)
			queue.wait_pop();
	}};
	std::thread writer{[&queue]() {
		for (int i = 0; i < 1000; ++i) {
			int i2 = i;
			queue.push(std::move(i2));
		}
	}};

	if (reader.joinable())
		reader.join();
	if (writer.joinable())
		writer.join();
	
	timer.stop();
	boost::timer::cpu_times elapsed_times = timer.elapsed();

	return elapsed_times.wall;
}

template<typename T>
nanosecond_type measure_performance(std::size_t count) {
	using namespace boost::accumulators;

	accumulator_set<nanosecond_type, features<tag::mean>> accumulator;
	for (int i = 0; i < count; ++i)
		accumulator(measure_performance<T>());
	return mean(accumulator);
}

BOOST_AUTO_TEST_SUITE(ThreadSafeQueue)

BOOST_AUTO_TEST_SUITE(SingleThread)

BOOST_AUTO_TEST_CASE(Pop) {
	thread_pool::data_structures::thread_safe::lock_based::queue<int> queue;

	queue.push(0);
	queue.push(1);
	BOOST_CHECK_EQUAL(queue.pop(), 0);
	queue.push(2);
	BOOST_CHECK_EQUAL(queue.pop(), 1);
	BOOST_CHECK_EQUAL(queue.pop(), 2);

	BOOST_CHECK(queue.empty());
}

BOOST_AUTO_TEST_SUITE_END(SingleThread)

BOOST_AUTO_TEST_SUITE(ReaderWriterThreads)

BOOST_AUTO_TEST_CASE(WaitPop) {
	thread_pool::data_structures::thread_safe::lock_based::queue<int> queue;

	std::thread reader{[&queue]() {
		for (int i = 0; i < 100; ++i)
			BOOST_CHECK_EQUAL(queue.wait_pop(), i);
	}};
	std::thread writer{[&queue]() {
		for (int i = 0; i < 100; ++i) {
			int i2 = i;
			queue.push(std::move(i2));
		}
	}};

	if (reader.joinable())
		reader.join();
	if (writer.joinable())
		writer.join();
}

BOOST_AUTO_TEST_SUITE_END(ReaderWriterThreads)

BOOST_AUTO_TEST_SUITE(Performance)

BOOST_AUTO_TEST_CASE(std_queue_vs_queue) {
	nanosecond_type queue_time = measure_performance<queue<int>>(100);
	nanosecond_type thread_safe_std_queue_time = measure_performance<std_queue<int>>(100);

	BOOST_TEST_MESSAGE("queue time: " << queue_time);
	BOOST_TEST_MESSAGE("std_queue time: " << thread_safe_std_queue_time);

	BOOST_CHECK_LE(queue_time, thread_safe_std_queue_time);
}

BOOST_AUTO_TEST_SUITE_END(Performance)

BOOST_AUTO_TEST_SUITE_END(ThreadSafeQueue)