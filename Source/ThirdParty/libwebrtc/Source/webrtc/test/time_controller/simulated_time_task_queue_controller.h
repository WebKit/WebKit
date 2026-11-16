/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TIME_CONTROLLER_SIMULATED_TIME_TASK_QUEUE_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_SIMULATED_TIME_TASK_QUEUE_CONTROLLER_H_

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/simulated_time_controller_impl.h"

namespace webrtc {

// This is a lightweight version of the `GlobalSimulatedTimeController`. It
// supports simulated time `TaskQueue`s, but not simulated time `Thread`s. The
// benefit is that it does not have a dependence on setting the process-global
// clock, and it can thus be `testonly=false`, for consumers that require that.
class SimulatedTimeTaskQueueController {
 public:
  explicit SimulatedTimeTaskQueueController(Timestamp start_time);
  ~SimulatedTimeTaskQueueController();

  // Clock for the simulated time.
  Clock* GetClock();

  // Task queues run on simulated time.
  TaskQueueFactory* GetTaskQueueFactory();

  // Advance simulated time by `duration` and run all relevant tasks.
  void AdvanceTime(TimeDelta duration);

 private:
  SimulatedClock sim_clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
  ScopedYieldPolicy yield_policy_;
};

}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_TASK_QUEUE_CONTROLLER_H_
