/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/test/simulated_time_test_fixture.h"

#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/create_test_environment.h"

namespace webrtc::video_timing_simulator {

SimulatedTimeTestFixture::SimulatedTimeTestFixture()
    : time_controller_(/*start_time=*/Timestamp::Seconds(10000)),
      env_(CreateTestEnvironment(
          CreateTestEnvironmentOptions{.time = &time_controller_})),
      queue_(env_.task_queue_factory().CreateTaskQueue(
          "test_queue",
          TaskQueueFactory::Priority::NORMAL)),
      queue_ptr_(queue_.get()) {}

SimulatedTimeTestFixture::~SimulatedTimeTestFixture() = default;

void SimulatedTimeTestFixture::SendTask(absl::AnyInvocable<void() &&> task) {
  queue_->PostTask(std::move(task));
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

}  // namespace webrtc::video_timing_simulator
