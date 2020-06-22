#define BOOST_TEST_DYN_LINK

#include <thread_pool.hpp>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>

namespace this_thread = std::this_thread;

using namespace std::chrono_literals;

using std::back_inserter;
using std::cout;
using std::merge;
using std::shuffle;
using std::sort;
using std::vector;

std::vector<std::vector<int>::iterator> separate_to_chunks(std::vector<int>& data, size_t chunks_count) {
	const size_t chunks_size = (data.size() / chunks_count) + 1;

	std::vector<std::vector<int>::iterator> chunk_separators;
	chunk_separators.reserve(chunks_size);

	for (int i = 0; i < chunks_count; ++i) {
		chunk_separators.push_back(data.begin() + i * chunks_size);
	}
	chunk_separators.push_back(data.end());

	return chunk_separators;
}

void merge_sorted_chunks_serial(std::vector<int>& data, std::vector<std::vector<int>::iterator>& chunk_separators) {
	const int end = chunk_separators.size() - 2;
	if (end == 0)
		return;

	for (int i = 0; i < end; i += 2) {
		std::inplace_merge(chunk_separators[i], chunk_separators[i + 1], chunk_separators[i + 2]);
	}
	for (int i = 1; i < chunk_separators.size() - 1; i += 2) {
		chunk_separators.erase(chunk_separators.begin() + i);
		--i;
	}

	merge_sorted_chunks_serial(data, chunk_separators);
}

std::future<std::pair<int, int>> parallel_sort(thread_pool::thread_pool& workers, std::vector<int>& data,
											   std::vector<std::vector<int>::iterator>& chunk_separators, int begin,
											   int end) {
	if (end == begin + 1) {
		return workers.add_job([=, chunk_begin = chunk_separators[begin], chunk_end = chunk_separators[end]]() {
			std::sort(chunk_begin, chunk_end);

			return std::make_pair(begin, end);
		});
	}

	const int middle = (begin + end) / 2;

	return workers.add_job([begin, middle, end, &workers, &data, &chunk_separators]() {
		const auto end_2 = middle + (middle - begin);

		auto merge_1 = parallel_sort(workers, data, chunk_separators, begin, middle);
		auto merge_2 = parallel_sort(workers, data, chunk_separators, middle, end_2);

		// Avoid deadlocking the workers
		while (merge_1.wait_for(0s) == std::future_status::timeout ||
			   merge_2.wait_for(0s) == std::future_status::timeout) {
			workers.execute_pending_job();
		}

		const auto merged_range_1 = merge_1.get();
		const auto merged_range_2 = merge_2.get();

		assert(std::is_sorted(chunk_separators[merged_range_1.first], chunk_separators[merged_range_1.second]));
		assert(std::is_sorted(chunk_separators[merged_range_2.first], chunk_separators[merged_range_2.second]));

		std::inplace_merge(chunk_separators[merged_range_1.first], chunk_separators[merged_range_1.second],
						   chunk_separators[merged_range_2.second]);

		assert(std::is_sorted(chunk_separators[merged_range_1.first], chunk_separators[merged_range_2.second]));

		if ((end - begin) % 2 != 0) {
			auto merge_3 = parallel_sort(workers, data, chunk_separators, merged_range_2.second, end);
			while (merge_3.wait_for(0s) == std::future_status::timeout) {
				workers.execute_pending_job();
			}
			merge_3.wait();

			std::inplace_merge(chunk_separators[merged_range_1.first], chunk_separators[merged_range_2.second],
							   chunk_separators[end]);

			assert(std::is_sorted(chunk_separators[merged_range_1.first], chunk_separators[end]));

			return std::make_pair(merged_range_1.first, end);
		}

		return std::make_pair(merged_range_1.first, merged_range_2.second);
	});
}


BOOST_AUTO_TEST_SUITE(ThreadPoolTests)

BOOST_AUTO_TEST_CASE(CreateAndDestroy) {
	BOOST_CHECK_NO_THROW({ thread_pool::thread_pool workers; });
}

BOOST_AUTO_TEST_CASE(DataParallelism) {
	std::vector<int> data(1'000'000);
	std::iota(data.begin(), data.end(), 0);
	std::shuffle(data.begin(), data.end(), std::default_random_engine{});

	std::list<int> sorted_data(data.size());
	std::iota(sorted_data.begin(), sorted_data.end(), 0);

	std::vector<int> data_2 = data;

	/* Parallel sort */

	boost::timer::cpu_timer parallel_sort_timer;

	thread_pool::thread_pool workers;

	const size_t chunks_count = 100;

	std::vector<std::future<void>> job_results;
	job_results.reserve(chunks_count);

	auto chunk_separators = separate_to_chunks(data, chunks_count);

	for (int i = 0; i < chunk_separators.size() - 1; ++i) {
		job_results.emplace_back(
			workers.add_job([chunk_begin = chunk_separators[i], chunk_end = chunk_separators[i + 1]]() {
				std::sort(chunk_begin, chunk_end);
			}));
	}

	for (auto& job_result : job_results)
		job_result.wait();

	merge_sorted_chunks_serial(data, chunk_separators);

	const auto parallel_sort_times = parallel_sort_timer.elapsed();

	BOOST_CHECK_EQUAL_COLLECTIONS(data.cbegin(), data.cend(), sorted_data.cbegin(), sorted_data.cend());


	/* std::sort */

	boost::timer::cpu_timer std_sort_timer;
	std::sort(data_2.begin(), data_2.end());
	const auto std_sort_times = std_sort_timer.elapsed();


	/* Compare times */

	const auto parallel_sort_time = parallel_sort_times.user + parallel_sort_times.system;
	const auto std_sort_time = std_sort_times.user + std_sort_times.system;

	BOOST_TEST_MESSAGE("parallel_sort_time: " << parallel_sort_time << ", std_sort_time: " << std_sort_time);
	BOOST_CHECK_LE(parallel_sort_time, std_sort_time);
}

BOOST_AUTO_TEST_CASE(TaskParallelism) {
	std::vector<int> data(1'000'000);
	std::iota(data.begin(), data.end(), 0);
	std::shuffle(data.begin(), data.end(), std::default_random_engine{});

	std::vector<int> data_2 = data;

	std::list<int> sorted_data(data.size());
	std::iota(sorted_data.begin(), sorted_data.end(), 0);


	/* Parallel sort */

	boost::timer::cpu_timer parallel_sort_timer;

	thread_pool::thread_pool workers;

	const size_t chunks_count = 100;
	auto chunk_separators = separate_to_chunks(data, chunks_count);

	auto sort_result = parallel_sort(workers, data, chunk_separators, 0, chunk_separators.size() - 1);
	sort_result.wait();

	const auto parallel_sort_times = parallel_sort_timer.elapsed();

	BOOST_CHECK_EQUAL_COLLECTIONS(data.cbegin(), data.cend(), sorted_data.cbegin(), sorted_data.cend());


	/* std::sort */

	boost::timer::cpu_timer std_sort_timer;
	std::sort(data_2.begin(), data_2.end());
	const auto std_sort_times = std_sort_timer.elapsed();


	/* Compare times */

	const auto parallel_sort_time = parallel_sort_times.user + parallel_sort_times.system;
	const auto std_sort_time = std_sort_times.user + std_sort_times.system;

	BOOST_TEST_MESSAGE("parallel_sort_time: " << parallel_sort_time << ", std_sort_time: " << std_sort_time);
	BOOST_CHECK_LE(parallel_sort_time, std_sort_time);
}

BOOST_AUTO_TEST_SUITE_END()