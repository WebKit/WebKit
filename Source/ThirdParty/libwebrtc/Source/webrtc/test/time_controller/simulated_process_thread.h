/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_SIMULATED_PROCESS_THREAD_H_
#define TEST_TIME_CONTROLLER_SIMULATED_PROCESS_THREAD_H_

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

class SimulatedProcessThread : public ProcessThread,
                               public sim_time_impl::SimulatedSequenceRunner {
 public:
  SimulatedProcessThread(sim_time_impl::SimulatedTimeControllerImpl* handler,
                         absl::string_view name);
  virtual ~SimulatedProcessThread();
  void RunReady(Timestamp at_time) override;

  Timestamp GetNextRunTime() const override {
    rtc::CritScope lock(&lock_);
    return next_run_time_;
  }

  TaskQueueBase* GetAsTaskQueue() override { return this; }

  // ProcessThread interface
  void Start() override;
  void Stop() override;
  void WakeUp(Module* module) override;
  void RegisterModule(Module* module, const rtc::Location& from) override;
  void DeRegisterModule(Module* module) override;
  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

 private:
  void Delete() override {
    // ProcessThread shouldn't be deleted as a TaskQueue.
    RTC_NOTREACHED();
  }
  Timestamp GetNextTime(Module* module, Timestamp at_time);

  sim_time_impl::SimulatedTimeControllerImpl* const handler_;
  // Using char* to be debugger friendly.
  char* name_;
  rtc::CriticalSection lock_;
  Timestamp next_run_time_ RTC_GUARDED_BY(lock_) = Timestamp::PlusInfinity();

  std::deque<std::unique_ptr<QueuedTask>> queue_;
  std::map<Timestamp, std::vector<std::unique_ptr<QueuedTask>>> delayed_tasks_
      RTC_GUARDED_BY(lock_);

  bool process_thread_running_ RTC_GUARDED_BY(lock_) = false;
  std::vector<Module*> stopped_modules_ RTC_GUARDED_BY(lock_);
  std::map<Timestamp, std::list<Module*>> delayed_modules_
      RTC_GUARDED_BY(lock_);
};
}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_PROCESS_THREAD_H_
