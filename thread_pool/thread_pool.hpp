#pragma once

#include "data_structures/thread_safe/lock_based/queue.hpp"

#include <ThreadPool_Export.h>

#include <boost/timer/timer.hpp>

#include <atomic>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <unordered_map>

namespace thread_pool {

class THREADPOOL_EXPORT thread_pool final {
  public:
	struct worker_stats final {
		boost::timer::cpu_times working_time;
		boost::timer::cpu_times overall_time;
	};

	thread_pool();

	thread_pool(const thread_pool& other) = delete;

	thread_pool& operator=(const thread_pool& other) = delete;

	thread_pool(thread_pool&& other) noexcept;

	thread_pool& operator=(thread_pool&& other) noexcept;

	~thread_pool();

	template <typename Job>
	// TODO research how std::future works
	std::future<typename std::result_of<Job()>::type> add_job(Job job) {
		using JobResult = std::result_of<Job()>::type;

		// TODO research how std::packaged_task works
		std::packaged_task<JobResult()> task{std::move(job)};
		std::future<JobResult> result{task.get_future()};
		_jobs.push(job_wrapper{std::move(task)});
		return result;
	}

	void execute_pending_job();

	std::vector<thread_pool::worker_stats> workers_stats() const;

  private:
	class job_wrapper final {
	  public:
		job_wrapper() = default;

		~job_wrapper() = default;

		job_wrapper(const job_wrapper&) = delete;

		job_wrapper& operator=(const job_wrapper&) = delete;

		job_wrapper(job_wrapper&&) = default;

		job_wrapper& operator=(job_wrapper&&) = default;

		template <typename Job> job_wrapper(Job&& job) : _job{new job_wrapper::job<Job>{std::move(job)}} {
		}

		void execute();

	  private:
		class callable {
		  public:
			virtual void execute() = 0;

			virtual ~callable() = default;
		};

		template <typename F> class job final : public callable {
		  public:
			job(F&& f) : _f{std::move(f)} {
			}

			void execute() override {
				_f();
			}

		  private:
			F _f;
		};

		std::unique_ptr<callable> _job;
	};

	void execute_pending_jobs();

	void join_threads();

	// TODO research how std::atomic<bool> works
	std::atomic<bool> _execute;
	data_structures::thread_safe::lock_based::std_queue<job_wrapper> _jobs;
	std::vector<worker_stats> _workers_stats;
	std::vector<std::thread> _workers;
};

} // namespace thread_pool