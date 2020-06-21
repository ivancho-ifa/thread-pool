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

/* thread_pool::worker_execution_times definitions */

thread_pool::worker_execution_times::worker_execution_times(cpu_times&& working_times, cpu_times&& sleeping_times) :
   working{working_times}, sleeping{sleeping_times}
{}

/* thread_pool::worker_execution_times definitions end */


thread_pool::thread_pool() :
   _execute{true}
{
   const unsigned threads_count = thread::hardware_concurrency();
   _workers.reserve(threads_count);
   try {
      for (unsigned i = 0; i < threads_count; ++i) {
         _workers.emplace_back(thread{&thread_pool::execute_pending_jobs, this});
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

void thread_pool::execute_pending_job() {
   // TODO use wait_pop when you can interrupt it
   //job_wrapper job = _jobs.wait_pop();
   //job.execute();
   
   try {
      job_wrapper job = _jobs.pop();
      job.execute();
   }
   catch (const logic_error& error) {
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

void thread_pool::join_threads()
{
   std::cout << "Joining threads..." << '\n';

   for (thread& worker : _workers)
      if (worker.joinable())
         worker.join();
}

} // namespace thread_pool