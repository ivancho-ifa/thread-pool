#pragma once

#include "data_structures/thread_safe/lock_based/queue.hpp"

#include <ThreadPool_Export.h>

#include <boost/timer/timer.hpp>

#include <atomic>
#include <functional>
#include <future>
#include <queue>
#include <thread>

namespace thread_pool {

class THREADPOOL_EXPORT thread_pool final {
public:
   class worker_execution_times final {
   public:
      worker_execution_times() = default;

      worker_execution_times(boost::timer::cpu_times&& working_times, boost::timer::cpu_times&& sleeping_times);


      /* TODO Research why linking fails when
         working_times and sleeping_times are in .cpp */

      const boost::timer::cpu_times& working_times() const {
         return _working;
      }

      const boost::timer::cpu_times& sleeping_times() const {
         return _sleeping;
      }

   private:
      boost::timer::cpu_times _working;
      boost::timer::cpu_times _sleeping;
   };


   thread_pool();

   thread_pool(const thread_pool& other) = delete;
   thread_pool& operator=(const thread_pool& other) = delete;

   thread_pool(thread_pool&& other) noexcept;
   thread_pool& operator=(thread_pool&& other) noexcept;

   ~thread_pool();


   template<typename Job>
   // TODO research how std::future works
   std::future<typename std::result_of<Job()>::type> execute(Job job) {
      using JobResult = std::result_of<Job()>::type;

      // TODO research how std::packaged_task works
      std::packaged_task<JobResult()> task{std::move(job)};
      std::future<JobResult> result{task.get_future()};
      _jobs.push(job_wrapper{std::move(task)});
      return result;
   }

   const std::vector<worker_execution_times>& workers_execution_times() const;


private:
   class job_wrapper final {
   public:
      job_wrapper() = default;
      ~job_wrapper() = default;

      job_wrapper(const job_wrapper&) = delete;
      job_wrapper& operator=(const job_wrapper&) = delete;

      job_wrapper(job_wrapper&&) = default;
      job_wrapper& operator=(job_wrapper&&) = default;


      template<typename Job>
      job_wrapper(Job&& job) :
         _job{new job_wrapper::job<Job>{std::move(job)}}
      {}


      void execute();
      boost::timer::cpu_times last_execution_time() const;

   private:
      class callable {
      public:
         virtual void execute() = 0;
         virtual ~callable() = default;
      };

      template<typename F>
      class job final : public callable {
      public:
         job(F&& f) :
            _f{std::move(f)}
         {}

         void execute() override {
            _f();
         }

      private:
         F _f;
      };


      std::unique_ptr<callable> _job;
      boost::timer::cpu_timer _timer;
   };


   void execute_pending_job(worker_execution_times& execution_times);
   void join_threads();

   // TODO research how std::atomic<bool> works
   std::atomic<bool> _execute;
   std::vector<std::thread> _workers;
   std::vector<worker_execution_times> _worker_times;
   data_structures::thread_safe::lock_based::queue<job_wrapper> _jobs;
};

} // namespace thread_pool