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
#include <memory>
#include <string>
#include <utility>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/simulated_thread.h"

namespace webrtc {

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

std::unique_ptr<Thread> GlobalSimulatedTimeController::CreateThread(
    const std::string& name,
    std::unique_ptr<SocketServer> socket_server) {
  return impl_.CreateThread(name, std::move(socket_server));
}

Thread* GlobalSimulatedTimeController::GetMainThread() {
  return main_thread_.get();
}

void GlobalSimulatedTimeController::AdvanceTime(TimeDelta duration) {
  ScopedYieldPolicy yield_policy(&impl_);
  Timestamp current_time = impl_.CurrentTime();
  RTC_DCHECK_EQ(current_time, sim_clock_.CurrentTime());
  RTC_DCHECK_EQ(current_time.us(), TimeMicros());
  Timestamp target_time = current_time + duration;
  while (current_time < target_time) {
    impl_.RunReadyRunners();
    Timestamp next_time = std::min(impl_.NextRunTime(), target_time);
    impl_.AdvanceTime(next_time);
    auto delta = next_time - current_time;
    current_time = next_time;
    sim_clock_.AdvanceTimeMicroseconds(delta.us());
    global_clock_.AdvanceTime(delta);
  }
  // After time has been simulated up until `target_time` we also need to run
  // tasks meant to be executed at `target_time`.
  impl_.RunReadyRunners();
}

void GlobalSimulatedTimeController::SkipForwardBy(TimeDelta duration) {
  ScopedYieldPolicy yield_policy(&impl_);
  Timestamp current_time = impl_.CurrentTime();
  Timestamp target_time = current_time + duration;
  impl_.AdvanceTime(target_time);
  sim_clock_.AdvanceTimeMicroseconds(duration.us());
  global_clock_.AdvanceTime(duration);
}

}  // namespace webrtc
