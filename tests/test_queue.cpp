#define BOOST_TEST_DYN_LINK

#include <data_structures/thread_safe/lock_based/queue.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

using boost::timer::nanosecond_type;
using thread_pool::data_structures::thread_safe::lock_based::queue;
using thread_pool::data_structures::thread_safe::lock_based::std_queue;

template <typename Q> void test_read_write(Q& queue, size_t queue_size) {
	std::thread reader{[&queue, queue_size]() {
		for (int i = 0; i < queue_size; ++i)
			for (Q::value_type element; !queue.pop(element);)
				;
	}};
	std::thread writer{[&queue, queue_size]() {
		for (int i = 0; i < queue_size; ++i) {
			int i2 = i;
			queue.push(std::move(i2));
		}
	}};

	if (reader.joinable())
		reader.join();
	if (writer.joinable())
		writer.join();
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

BOOST_AUTO_TEST_SUITE_END()

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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Performance)

BOOST_AUTO_TEST_CASE(std_queue_vs_queue) {
	queue<int> q;
	std_queue<int> std_q;
	const size_t queue_size = 1'000'000;

	boost::timer::cpu_timer q_timer;
	test_read_write(q, queue_size);
	boost::timer::cpu_times q_times = q_timer.elapsed();

	boost::timer::cpu_timer std_q_timer;
	test_read_write(std_q, queue_size);
	boost::timer::cpu_times std_q_times = std_q_timer.elapsed();

	BOOST_TEST_MESSAGE("USER+SYSTEM | queue time: " << q_times.user + q_times.system
													<< ", std_queue time: " << std_q_times.user + std_q_times.system);
	BOOST_TEST_MESSAGE("WALL | queue time: " << q_times.wall << ", std_queue time: " << std_q_times.wall);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()