#define BOOST_TEST_DYN_LINK

#include <thread_pool.hpp>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <iostream>
#include <numeric>
#include <thread>


namespace this_thread = std::this_thread;

using std::back_inserter;
using std::cout;
using std::merge;
using std::random_shuffle;
using std::sort;
using std::vector;


BOOST_AUTO_TEST_SUITE(Basic)

BOOST_AUTO_TEST_CASE(CtorDtor) {
	thread_pool::thread_pool workers;
}

BOOST_AUTO_TEST_CASE(CreateThreadPool) {
	thread_pool::thread_pool workers;

	vector<int> data;
	data.reserve(100'000);
	for (int i = 0; i < 100'000; ++i)
		data.emplace_back(i);
	random_shuffle(data.begin(), data.end());


	// sort the two halves of the array

	auto job0 = workers.execute([&]() {
		cout << "thread " << this_thread::get_id() << '\n';

		sort(data.begin(), data.begin() + data.size() / 2);
	});
	auto job1 = workers.execute([&]() {
		cout << "thread " << this_thread::get_id() << '\n';

		sort(data.begin() + data.size() / 2, data.end());
	});

	job0.wait();
	job1.wait();


	// merge the two sorted halves of the array

	vector<int> sorted_data;
	sorted_data.reserve(data.size());

	auto job2 = workers.execute([&]() {
		merge(data.begin(), data.begin() + data.size() / 2, data.begin() + data.size() / 2, data.end(), back_inserter(sorted_data));
	});

	job2.wait();


	for (int i = 0; i < 100'000; ++i) {
		BOOST_CHECK_EQUAL(sorted_data[i], i);

		if (sorted_data[i] != i)
			break;
	}

	cout << "Finished work" << '\n';
}

BOOST_AUTO_TEST_SUITE_END(Basic)