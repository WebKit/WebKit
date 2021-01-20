/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_EXTERNAL_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_EXTERNAL_TIME_CONTROLLER_H_

#include <functional>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

// TimeController implementation built on an external controlled alarm.
// This implementation is used to delegate scheduling and execution to an
// external run loop.
class ExternalTimeController : public TimeController, public TaskQueueFactory {
 public:
  explicit ExternalTimeController(ControlledAlarmClock* alarm);

  // Implementation of TimeController.
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void AdvanceTime(TimeDelta duration) override;
  std::unique_ptr<rtc::Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<rtc::SocketServer> socket_server) override;
  rtc::Thread* GetMainThread() override;

  // Implementation of TaskQueueFactory.
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      TaskQueueFactory::Priority priority) const override;

 private:
  class ProcessThreadWrapper;
  class TaskQueueWrapper;

  // Executes any tasks scheduled at or before the current time.  May call
  // |ScheduleNext| to schedule the next call to |Run|.
  void Run();

  void UpdateTime();
  void ScheduleNext();

  ControlledAlarmClock* alarm_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
  rtc::ScopedYieldPolicy yield_policy_;

  // Overrides the global rtc::Clock to ensure that it reports the same times as
  // the time controller.
  rtc::ScopedBaseFakeClock global_clock_;
};

}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_EXTERNAL_TIME_CONTROLLER_H_
