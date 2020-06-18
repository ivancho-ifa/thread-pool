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
using std::sort;
using std::vector;


BOOST_AUTO_TEST_SUITE(Basic)

BOOST_AUTO_TEST_CASE(CreateThreadPool) {
	thread_pool::thread_pool workers;

	vector<int> data;
	data.reserve(100'000);
	for (int i = 0; i < 100'000; ++i)
		data.push_back(99'999 - i);

	auto job0 = workers.execute([&data]() {
		cout << "thread " << this_thread::get_id() << '\n';

		sort(data.begin(), data.begin() + data.size() / 2);
	});
	auto job1 = workers.execute([&data]() {
		cout << "thread " << this_thread::get_id() << '\n';

		sort(data.begin() + data.size() / 2 + 1, data.end());
	});

	job0.wait();
	job1.wait();

	vector<int> sorted_data;
	auto job2 = workers.execute([&]() {
		merge(data.begin(), data.begin() + data.size() / 2, data.begin() + data.size() / 2 + 1, data.end(), back_inserter(sorted_data));
	});

	for (int i = 0; i < 100'000; ++i) {
		if (i == data[i])
			cout << 'OK' << '\n';
		else {
			cout << i << " == " << data[i] << '\n';
			cout << "NOT OK" << '\n';
			break;
		}
	}

	const auto workers_times = workers.workers_execution_times();

	for (int i = 0; i < workers_times.size(); ++i) {
		const auto& worker_times = workers_times[i];

		cout << "working - system time: " << worker_times.working_times().system << '\n';
		cout << "working - user time: " << worker_times.working_times().user << '\n';
		cout << "working - wall time: " << worker_times.working_times().wall << '\n';

		cout << "sleeping - system time: " << worker_times.sleeping_times().system << '\n';
		cout << "sleeping - user time: " << worker_times.sleeping_times().user << '\n';
		cout << "sleeping - wall time: " << worker_times.sleeping_times().wall << '\n';
	}
}

BOOST_AUTO_TEST_SUITE_END(Basic)