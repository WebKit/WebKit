/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/time_controller/simulated_time_controller_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
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
    : thread_id_(CurrentThreadId()), current_time_(start_time) {}

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

std::unique_ptr<Thread> SimulatedTimeControllerImpl::CreateThread(
    const std::string& name,
    std::unique_ptr<SocketServer> socket_server) {
  auto thread =
      std::make_unique<SimulatedThread>(this, name, std::move(socket_server));
  Register(thread.get());
  return thread;
}

void SimulatedTimeControllerImpl::YieldExecution() {
  if (CurrentThreadId() == thread_id_) {
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
  RTC_DCHECK_EQ(CurrentThreadId(), thread_id_);
  Timestamp current_time = CurrentTime();
  // Clearing `ready_runners_` in case this is a recursive call:
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
      // Unregister() which will grab `lock_` again to remove items from
      // `ready_runners_`.
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

}  // namespace webrtc
