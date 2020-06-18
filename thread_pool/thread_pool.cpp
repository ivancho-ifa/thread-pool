#include "thread_pool.hpp"

#include <iostream>
#include <system_error>


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

/* thread_pool::worker_execution_times definitions */

thread_pool::worker_execution_times::worker_execution_times(cpu_times&& working_times, cpu_times&& sleeping_times) :
   _working{working_times}, _sleeping{sleeping_times}
{}

/* thread_pool::worker_execution_times definitions end */


thread_pool::thread_pool() :
   _execute{true}
{
   const unsigned threads_count = thread::hardware_concurrency();
   _workers.reserve(threads_count);
   _worker_times.reserve(_workers.size());
   try {
      for (unsigned i = 0; i < threads_count; ++i) {
         _worker_times.emplace_back();
         _workers.emplace_back(thread{&thread_pool::execute_pending_job, this, ref(_worker_times[i])});
      }
   }
   catch (const system_error& e) {
      _execute = false;
      this->join_threads();
      throw;
   }
}

thread_pool::thread_pool(thread_pool&& other) noexcept :
   _workers{move(other._workers)}
{}

thread_pool& thread_pool::operator=(thread_pool&& other) noexcept {
   _workers = move(other._workers);

   return *this;
}

thread_pool::~thread_pool() {
   _execute = false;
   this->join_threads();
}

const vector<thread_pool::worker_execution_times>& thread_pool::workers_execution_times() const {
   return _worker_times;
}


/* thread_pool::job_wrapper definitions */

void thread_pool::job_wrapper::execute() {
   _timer.start();

   _job->execute();

   _timer.stop();
}

cpu_times thread_pool::job_wrapper::last_execution_time() const {
   return _timer.elapsed();
}

/* thread_pool::job_wrapper definitions end */


void thread_pool::execute_pending_job(worker_execution_times& execution_times) {
   cpu_timer thread_execution_timer;
   cpu_times thread_working_time;
   thread_working_time.clear();

   while (_execute) {
		job_wrapper job = _jobs.wait_pop();
		job.execute();
   }

   cpu_times thread_execution_time = thread_execution_timer.elapsed();
   
   cpu_times thread_sleep_time;
   thread_sleep_time.system = thread_execution_time.system - thread_working_time.system;
   thread_sleep_time.user = thread_execution_time.user - thread_working_time.user;
   thread_sleep_time.wall = thread_execution_time.wall - thread_working_time.wall;

   execution_times = worker_execution_times{move(thread_working_time), move(thread_sleep_time)};
}

void thread_pool::join_threads()
{
   std::cout << "Threads joined" << '\n';

   for (thread& worker : _workers)
      if (worker.joinable())
         worker.join();
}

} // namespace thread_pool