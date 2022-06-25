/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_

#include <list>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/sequence_checker.h"
#include "api/test/time_controller.h"
#include "api/units/timestamp.h"
#include "modules/include/module.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/synchronization/yield_policy.h"

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
                                    public rtc::YieldInterface {
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
  // Create process thread with the name |thread_name|.
  std::unique_ptr<ProcessThread> CreateProcessThread(const char* thread_name)
      RTC_LOCKS_EXCLUDED(time_lock_, lock_);
  // Create thread using provided |socket_server|.
  std::unique_ptr<rtc::Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<rtc::SocketServer> socket_server)
      RTC_LOCKS_EXCLUDED(time_lock_, lock_);

  // Runs all runners in |runners_| that has tasks or modules ready for
  // execution.
  void RunReadyRunners() RTC_LOCKS_EXCLUDED(time_lock_, lock_);
  // Return |current_time_|.
  Timestamp CurrentTime() const RTC_LOCKS_EXCLUDED(time_lock_);
  // Return min of runner->GetNextRunTime() for runner in |runners_|.
  Timestamp NextRunTime() const RTC_LOCKS_EXCLUDED(lock_);
  // Set |current_time_| to |target_time|.
  void AdvanceTime(Timestamp target_time) RTC_LOCKS_EXCLUDED(time_lock_);
  // Adds |runner| to |runners_|.
  void Register(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);
  // Removes |runner| from |runners_|.
  void Unregister(SimulatedSequenceRunner* runner) RTC_LOCKS_EXCLUDED(lock_);

  // Indicates that |yielding_from| is not ready to run.
  void StartYield(TaskQueueBase* yielding_from);
  // Indicates that processing can be continued on |yielding_from|.
  void StopYield(TaskQueueBase* yielding_from);

 private:
  const rtc::PlatformThreadId thread_id_;
  const std::unique_ptr<rtc::Thread> dummy_thread_ = rtc::Thread::Create();
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

  void Delete() override { RTC_NOTREACHED(); }
  void PostTask(std::unique_ptr<QueuedTask> /*task*/) override {
    RTC_NOTREACHED();
  }
  void PostDelayedTask(std::unique_ptr<QueuedTask> /*task*/,
                       uint32_t /*milliseconds*/) override {
    RTC_NOTREACHED();
  }
};

// TimeController implementation using completely simulated time. Task queues
// and process threads created by this controller will run delayed activities
// when AdvanceTime() is called. Overrides the global clock backing
// rtc::TimeMillis() and rtc::TimeMicros(). Note that this is not thread safe
// since it modifies global state.
class GlobalSimulatedTimeController : public TimeController {
 public:
  explicit GlobalSimulatedTimeController(Timestamp start_time);
  ~GlobalSimulatedTimeController() override;

  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  std::unique_ptr<rtc::Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<rtc::SocketServer> socket_server) override;
  rtc::Thread* GetMainThread() override;

  void AdvanceTime(TimeDelta duration) override;

 private:
  rtc::ScopedBaseFakeClock global_clock_;
  // Provides simulated CurrentNtpInMilliseconds()
  SimulatedClock sim_clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
  rtc::ScopedYieldPolicy yield_policy_;
  std::unique_ptr<rtc::Thread> main_thread_;
};
}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
