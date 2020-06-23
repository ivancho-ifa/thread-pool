#include "thread_pool.hpp"
#include "util/boost.hpp"

#include <iostream>
#include <system_error>

using util::boost::operator-;
using util::boost::operator+=;

using boost::timer::cpu_timer;
using boost::timer::cpu_times;

using std::cout;
using std::exception;
using std::logic_error;
using std::move;
using std::ref;
using std::system_error;
using std::thread;
using std::vector;

namespace thread_pool {

thread_pool::thread_pool() : thread_pool{std::thread::hardware_concurrency()} {
}

thread_pool::thread_pool(size_t threads_count) : _execute{true} {
	_workers.reserve(threads_count);
	_workers_stats.max_load_factor(.75f);
	_workers_stats.reserve(_workers.size());
	try {
		for (unsigned i = 0; i < threads_count; ++i) {
			_workers.emplace_back(thread{&thread_pool::execute_pending_jobs, this});
			_workers_stats[_workers.back().get_id()] = worker_stats{};
		}
	}
	catch (const system_error& e) {
		_execute = false;
		this->join_threads();
		throw;
	}
}

thread_pool::thread_pool(thread_pool&& other) noexcept : _workers{move(other._workers)} {
}

thread_pool& thread_pool::operator=(thread_pool&& other) noexcept {
	_workers = move(other._workers);

	return *this;
}

thread_pool::~thread_pool() {
	_execute = false;
	this->join_threads();
}

void thread_pool::execute_pending_job() {
	// TODO use wait_pop when you can interrupt it
	// job_wrapper job = _jobs.wait_pop();
	// job.execute();

	boost::timer::cpu_timer thread_execution_timer;

	if (job_wrapper job; _jobs.pop(job)) {
		boost::timer::cpu_timer job_execution_timer;
		job.execute();
		job_execution_timer.stop();

		// Only this thread can change its entry. The map is not reallocated.
		auto worker_stats_entry = _workers_stats.find(std::this_thread::get_id());
		if (worker_stats_entry != _workers_stats.cend()) {
			auto& worker_stats = worker_stats_entry->second;

			std::lock_guard<std::shared_mutex> lock{_workers_stats_guard};
			worker_stats.overall_time += thread_execution_timer.elapsed();
			worker_stats.working_time += job_execution_timer.elapsed();
		}
	}
	else {
		auto worker_stats_entry = _workers_stats.find(std::this_thread::get_id());
		if (worker_stats_entry != _workers_stats.cend()) {
			auto& worker_stats = worker_stats_entry->second;

			std::lock_guard<std::shared_mutex> lock{_workers_stats_guard};
			worker_stats.overall_time += thread_execution_timer.elapsed();
		}

		std::this_thread::yield();
	}
}

/* thread_pool::job_wrapper definitions */

void thread_pool::job_wrapper::execute() {
	_job->execute();
}

/* thread_pool::job_wrapper definitions end */

void thread_pool::execute_pending_jobs() {
	while (_execute)
		this->execute_pending_job();
}

std::unordered_map<std::thread::id, thread_pool::worker_stats> thread_pool::workers_stats() const {
	std::shared_lock<std::shared_mutex> lock{_workers_stats_guard};

	return _workers_stats;
}

void thread_pool::join_threads() {
	for (thread& worker : _workers)
		if (worker.joinable())
			worker.join();
}

} // namespace thread_pool