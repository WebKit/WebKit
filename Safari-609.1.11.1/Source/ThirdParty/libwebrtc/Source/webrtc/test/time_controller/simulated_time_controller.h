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

#include "api/units/timestamp.h"
#include "modules/include/module.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "rtc_base/thread_checker.h"
#include "test/time_controller/time_controller.h"

namespace webrtc {

namespace sim_time_impl {
class SimulatedSequenceRunner;

class SimulatedTimeControllerImpl : public TaskQueueFactory,
                                    public rtc::YieldInterface {
 public:
  explicit SimulatedTimeControllerImpl(Timestamp start_time);
  ~SimulatedTimeControllerImpl() override;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override;

  // Implements the YieldInterface by running ready tasks on all task queues,
  // except that if this method is called from a task, the task queue running
  // that task is skipped.
  void YieldExecution() override;
  // Create process thread with the name |thread_name|.
  std::unique_ptr<ProcessThread> CreateProcessThread(const char* thread_name);
  // Runs all runners in |runners_| that has tasks or modules ready for
  // execution.
  void RunReadyRunners();
  // Return |current_time_|.
  Timestamp CurrentTime() const;
  // Return min of runner->GetNextRunTime() for runner in |runners_|.
  Timestamp NextRunTime() const;
  // Set |current_time_| to |target_time|.
  void AdvanceTime(Timestamp target_time);
  // Removes |runner| from |runners_|.
  void Unregister(SimulatedSequenceRunner* runner);

 private:
  const rtc::PlatformThreadId thread_id_;
  rtc::ThreadChecker thread_checker_;
  rtc::CriticalSection time_lock_;
  Timestamp current_time_ RTC_GUARDED_BY(time_lock_);
  rtc::CriticalSection lock_;
  std::vector<SimulatedSequenceRunner*> runners_ RTC_GUARDED_BY(lock_);
  // Used in RunReadyRunners() to keep track of ready runners that are to be
  // processed in a round robin fashion. the reason it's a member is so that
  // runners can removed from here by Unregister().
  std::list<SimulatedSequenceRunner*> ready_runners_ RTC_GUARDED_BY(lock_);

  // Task queues on which YieldExecution has been called.
  std::unordered_set<TaskQueueBase*> yielded_ RTC_GUARDED_BY(thread_checker_);
};
}  // namespace sim_time_impl

// TimeController implementation using completely simulated time. Task queues
// and process threads created by this controller will run delayed activities
// when Sleep() is called. Overrides the global clock backing rtc::TimeMillis()
// and rtc::TimeMicros(). Note that this is not thread safe since it modifies
// global state.
class GlobalSimulatedTimeController : public TimeController {
 public:
  explicit GlobalSimulatedTimeController(Timestamp start_time);
  ~GlobalSimulatedTimeController() override;

  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void Sleep(TimeDelta duration) override;
  void InvokeWithControlledYield(std::function<void()> closure) override;

 private:
  rtc::ScopedBaseFakeClock global_clock_;
  // Provides simulated CurrentNtpInMilliseconds()
  SimulatedClock sim_clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
};
}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
