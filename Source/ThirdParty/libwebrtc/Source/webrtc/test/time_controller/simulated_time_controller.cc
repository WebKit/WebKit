/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/simulated_time_controller.h"

#include <algorithm>
#include <deque>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/string_view.h"
#include "test/time_controller/simulated_task_queue.h"
#include "test/time_controller/simulated_thread.h"

namespace webrtc {
namespace {
// Helper function to remove from a std container by value.
template <class C>
bool RemoveByValue(C* vec, typename C::value_type val) {
  auto it = std::find(vec->begin(), vec->end(), val);
  if (it == vec->end())
    return false;
  vec->erase(it);
  return true;
}
}  // namespace

namespace sim_time_impl {

SimulatedTimeControllerImpl::SimulatedTimeControllerImpl(Timestamp start_time)
    : thread_id_(rtc::CurrentThreadId()), current_time_(start_time) {}

SimulatedTimeControllerImpl::~SimulatedTimeControllerImpl() = default;

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
SimulatedTimeControllerImpl::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  // TODO(srte): Remove the const cast when the interface is made mutable.
  auto mutable_this = const_cast<SimulatedTimeControllerImpl*>(this);
  auto task_queue = std::unique_ptr<SimulatedTaskQueue, TaskQueueDeleter>(
      new SimulatedTaskQueue(mutable_this, name));
  mutable_this->Register(task_queue.get());
  return task_queue;
}

std::unique_ptr<rtc::Thread> SimulatedTimeControllerImpl::CreateThread(
    const std::string& name,
    std::unique_ptr<rtc::SocketServer> socket_server) {
  auto thread =
      std::make_unique<SimulatedThread>(this, name, std::move(socket_server));
  Register(thread.get());
  return thread;
}

void SimulatedTimeControllerImpl::YieldExecution() {
  if (rtc::CurrentThreadId() == thread_id_) {
    TaskQueueBase* yielding_from = TaskQueueBase::Current();
    // Since we might continue execution on a process thread, we should reset
    // the thread local task queue reference. This ensures that thread checkers
    // won't think we are executing on the yielding task queue. It also ensure
    // that TaskQueueBase::Current() won't return the yielding task queue.
    TokenTaskQueue::CurrentTaskQueueSetter reset_queue(nullptr);
    // When we yield, we don't want to risk executing further tasks on the
    // currently executing task queue. If there's a ready task that also yields,
    // it's added to this set as well and only tasks on the remaining task
    // queues are executed.
    auto inserted = yielded_.insert(yielding_from);
    RTC_DCHECK(inserted.second);
    RunReadyRunners();
    yielded_.erase(inserted.first);
  }
}

void SimulatedTimeControllerImpl::RunReadyRunners() {
  // Using a dummy thread rather than nullptr to avoid implicit thread creation
  // by Thread::Current().
  SimulatedThread::CurrentThreadSetter set_current(dummy_thread_.get());
  MutexLock lock(&lock_);
  RTC_DCHECK_EQ(rtc::CurrentThreadId(), thread_id_);
  Timestamp current_time = CurrentTime();
  // Clearing |ready_runners_| in case this is a recursive call:
  // RunReadyRunners -> Run -> Event::Wait -> Yield ->RunReadyRunners
  ready_runners_.clear();

  // We repeat until we have no ready left to handle tasks posted by ready
  // runners.
  while (true) {
    for (auto* runner : runners_) {
      if (yielded_.find(runner->GetAsTaskQueue()) == yielded_.end() &&
          runner->GetNextRunTime() <= current_time) {
        ready_runners_.push_back(runner);
      }
    }
    if (ready_runners_.empty())
      break;
    while (!ready_runners_.empty()) {
      auto* runner = ready_runners_.front();
      ready_runners_.pop_front();
      lock_.Unlock();
      // Note that the RunReady function might indirectly cause a call to
      // Unregister() which will grab |lock_| again to remove items from
      // |ready_runners_|.
      runner->RunReady(current_time);
      lock_.Lock();
    }
  }
}

Timestamp SimulatedTimeControllerImpl::CurrentTime() const {
  MutexLock lock(&time_lock_);
  return current_time_;
}

Timestamp SimulatedTimeControllerImpl::NextRunTime() const {
  Timestamp current_time = CurrentTime();
  Timestamp next_time = Timestamp::PlusInfinity();
  MutexLock lock(&lock_);
  for (auto* runner : runners_) {
    Timestamp next_run_time = runner->GetNextRunTime();
    if (next_run_time <= current_time)
      return current_time;
    next_time = std::min(next_time, next_run_time);
  }
  return next_time;
}

void SimulatedTimeControllerImpl::AdvanceTime(Timestamp target_time) {
  MutexLock time_lock(&time_lock_);
  RTC_DCHECK_GE(target_time, current_time_);
  current_time_ = target_time;
}

void SimulatedTimeControllerImpl::Register(SimulatedSequenceRunner* runner) {
  MutexLock lock(&lock_);
  runners_.push_back(runner);
}

void SimulatedTimeControllerImpl::Unregister(SimulatedSequenceRunner* runner) {
  MutexLock lock(&lock_);
  bool removed = RemoveByValue(&runners_, runner);
  RTC_CHECK(removed);
  RemoveByValue(&ready_runners_, runner);
}

void SimulatedTimeControllerImpl::StartYield(TaskQueueBase* yielding_from) {
  auto inserted = yielded_.insert(yielding_from);
  RTC_DCHECK(inserted.second);
}

void SimulatedTimeControllerImpl::StopYield(TaskQueueBase* yielding_from) {
  yielded_.erase(yielding_from);
}

}  // namespace sim_time_impl

GlobalSimulatedTimeController::GlobalSimulatedTimeController(
    Timestamp start_time)
    : sim_clock_(start_time.us()), impl_(start_time), yield_policy_(&impl_) {
  global_clock_.SetTime(start_time);
  auto main_thread = std::make_unique<SimulatedMainThread>(&impl_);
  impl_.Register(main_thread.get());
  main_thread_ = std::move(main_thread);
}

GlobalSimulatedTimeController::~GlobalSimulatedTimeController() = default;

Clock* GlobalSimulatedTimeController::GetClock() {
  return &sim_clock_;
}

TaskQueueFactory* GlobalSimulatedTimeController::GetTaskQueueFactory() {
  return &impl_;
}

std::unique_ptr<rtc::Thread> GlobalSimulatedTimeController::CreateThread(
    const std::string& name,
    std::unique_ptr<rtc::SocketServer> socket_server) {
  return impl_.CreateThread(name, std::move(socket_server));
}

rtc::Thread* GlobalSimulatedTimeController::GetMainThread() {
  return main_thread_.get();
}

void GlobalSimulatedTimeController::AdvanceTime(TimeDelta duration) {
  rtc::ScopedYieldPolicy yield_policy(&impl_);
  Timestamp current_time = impl_.CurrentTime();
  Timestamp target_time = current_time + duration;
  RTC_DCHECK_EQ(current_time.us(), rtc::TimeMicros());
  while (current_time < target_time) {
    impl_.RunReadyRunners();
    Timestamp next_time = std::min(impl_.NextRunTime(), target_time);
    impl_.AdvanceTime(next_time);
    auto delta = next_time - current_time;
    current_time = next_time;
    sim_clock_.AdvanceTimeMicroseconds(delta.us());
    global_clock_.AdvanceTime(delta);
  }
  // After time has been simulated up until |target_time| we also need to run
  // tasks meant to be executed at |target_time|.
  impl_.RunReadyRunners();
}

}  // namespace webrtc
