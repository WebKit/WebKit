/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/time_controller/simulated_time_task_queue_controller.h"

#include <algorithm>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

SimulatedTimeTaskQueueController::SimulatedTimeTaskQueueController(
    Timestamp start_time)
    : sim_clock_(start_time.us()), impl_(start_time), yield_policy_(&impl_) {}

SimulatedTimeTaskQueueController::~SimulatedTimeTaskQueueController() = default;

Clock* SimulatedTimeTaskQueueController::GetClock() {
  return &sim_clock_;
}

TaskQueueFactory* SimulatedTimeTaskQueueController::GetTaskQueueFactory() {
  return &impl_;
}

// This implementation is almost identical to
// `GlobalSimulatdTimeController::AdvanceTime`, with the main difference that
// the process-global clock is not overwritten.
void SimulatedTimeTaskQueueController::AdvanceTime(TimeDelta duration) {
  ScopedYieldPolicy yield_policy(&impl_);
  Timestamp current_time = impl_.CurrentTime();
  RTC_DCHECK_EQ(current_time, sim_clock_.CurrentTime());
  Timestamp target_time = current_time + duration;
  while (current_time < target_time) {
    impl_.RunReadyRunners();
    Timestamp next_time = std::min(impl_.NextRunTime(), target_time);
    impl_.AdvanceTime(next_time);
    auto delta = next_time - current_time;
    current_time = next_time;
    sim_clock_.AdvanceTimeMicroseconds(delta.us());
  }
  // After time has been simulated up until `target_time` we also need to run
  // tasks meant to be executed at `target_time`.
  impl_.RunReadyRunners();
}

}  // namespace webrtc
