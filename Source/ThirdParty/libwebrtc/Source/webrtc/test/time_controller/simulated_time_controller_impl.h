/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_IMPL_H_
#define TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/location.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace sim_time_impl {
class SimulatedSequenceRunner {
 public:
  virtual ~SimulatedSequenceRunner() = default;
  // Provides next run time.
  virtual Timestamp GetNextRunTime() const = 0;
  // Runs all ready tasks and modules and updates next run time.
  virtual void RunReady(Timestamp at_time) = 0;

  // All implementations also implements TaskQueueBase in some form, but if we'd
  // inherit from it in this interface we'd run into issues with double
  // inheritance. Therefore we simply allow the implementations to provide a
  // casted pointer to themself.
  virtual TaskQueueBase* GetAsTaskQueue() = 0;
};

class SimulatedTimeControllerImpl : public TaskQueueFactory,
                                    public YieldInterface {
 public:
  explicit SimulatedTimeControllerImpl(Timestamp start_time);
  ~SimulatedTimeControllerImpl() override;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const RTC_LOCKS_EXCLUDED(time_lock_) override;

  // Implements the YieldInterface by running ready tasks on all task queues,
  // except that if this method is called from a task, the task queue running
  // that task is skipped.
  void YieldExecution() RTC_LOCKS_EXCLUDED(time_lock_, lock_) override;

  // Create thread using provided `socket_server`.
  std::unique_ptr<Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<SocketServer> socket_server)
      RTC_LOCKS_EXCLUDED(time_lock_, lock_);

  // Runs all runners in `runners_` that has tasks or modules ready for
  // execution.
  void RunReadyRunners() RTC_LOCKS_EXCLUDED(time_lock_, lock_);
  // Return `current_time_`.
  Timestamp CurrentTime() const RTC_LOCKS_EXCLUDED(time_lock_);
  // Return min of runner->GetNextRunTime() for runner in `runners_`.
  Timestamp NextRunTime() const RTC_LOCKS_EXCLUDED(lock_);
  // Set `current_time_` to `target_time`.
  void AdvanceTime(Timestamp target_time) RTC_LOCKS_EXCLUDED(time_lock_);
  // Adds `runner` to `runners_`.
  void Register(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);
  // Removes `runner` from `runners_`.
  void Unregister(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);

  // Indicates that `yielding_from` is not ready to run.
  void StartYield(TaskQueueBase* yielding_from);
  // Indicates that processing can be continued on `yielding_from`.
  void StopYield(TaskQueueBase* yielding_from);

 private:
  const PlatformThreadId thread_id_;
  const std::unique_ptr<Thread> dummy_thread_ = Thread::Create();
  mutable Mutex time_lock_;
  Timestamp current_time_ RTC_GUARDED_BY(time_lock_);
  mutable Mutex lock_;
  std::vector<SimulatedSequenceRunner*> runners_ RTC_GUARDED_BY(lock_);
  // Used in RunReadyRunners() to keep track of ready runners that are to be
  // processed in a round robin fashion. the reason it's a member is so that
  // runners can removed from here by Unregister().
  std::list<SimulatedSequenceRunner*> ready_runners_ RTC_GUARDED_BY(lock_);

  // Runners on which YieldExecution has been called.
  std::unordered_set<TaskQueueBase*> yielded_;
};

}  // namespace sim_time_impl

// Used to satisfy sequence checkers for non task queue sequences.
class TokenTaskQueue : public TaskQueueBase {
 public:
  // Promoted to public
  using CurrentTaskQueueSetter = TaskQueueBase::CurrentTaskQueueSetter;

  void Delete() override { RTC_DCHECK_NOTREACHED(); }
  void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                    const PostTaskTraits& traits,
                    const Location& location) override {
    RTC_DCHECK_NOTREACHED();
  }
  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                           TimeDelta delay,
                           const PostDelayedTaskTraits& traits,
                           const Location& location) override {
    RTC_DCHECK_NOTREACHED();
  }
};

}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_IMPL_H_
