/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/pixel_limit_resource.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/units/timestamp.h"
#include "call/adaptation/test/fake_video_stream_input_state_provider.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "call/adaptation/video_stream_adapter.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using testing::_;

namespace webrtc {

namespace {

constexpr TimeDelta kResourceUsageCheckIntervalMs = TimeDelta::Seconds(5);

}  // namespace

class PixelLimitResourceTest : public ::testing::Test {
 public:
  PixelLimitResourceTest()
      : time_controller_(Timestamp::Micros(1234)),
        task_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "TestQueue",
            TaskQueueFactory::Priority::NORMAL)),
        input_state_provider_() {}

  void SetCurrentPixels(int current_pixels) {
    input_state_provider_.SetInputState(current_pixels, 30, current_pixels);
  }

  void RunTaskOnTaskQueue(absl::AnyInvocable<void() &&> task) {
    task_queue_->PostTask(std::move(task));
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

 protected:
  // Posted tasks, including repeated tasks, are executed when simulated time is
  // advanced by time_controller_.AdvanceTime().
  GlobalSimulatedTimeController time_controller_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
  FakeVideoStreamInputStateProvider input_state_provider_;
};

TEST_F(PixelLimitResourceTest, ResourceIsSilentByDefault) {
  // Because our mock is strick, the test would fail if
  // OnResourceUsageStateMeasured() is invoked.
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    rtc::scoped_refptr<PixelLimitResource> pixel_limit_resource =
        PixelLimitResource::Create(task_queue_.get(), &input_state_provider_);
    pixel_limit_resource->SetResourceListener(&resource_listener);
    // Set a current pixel count.
    SetCurrentPixels(1280 * 720);
    // Advance a significant amount of time.
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs * 10);
    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

TEST_F(PixelLimitResourceTest,
       OveruseIsReportedWhileCurrentPixelsIsGreaterThanMaxPixels) {
  constexpr int kMaxPixels = 640 * 480;
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    rtc::scoped_refptr<PixelLimitResource> pixel_limit_resource =
        PixelLimitResource::Create(task_queue_.get(), &input_state_provider_);
    pixel_limit_resource->SetResourceListener(&resource_listener);
    time_controller_.AdvanceTime(TimeDelta::Zero());

    pixel_limit_resource->SetMaxPixels(kMaxPixels);
    SetCurrentPixels(kMaxPixels + 1);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kOveruse))
        .Times(1);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs);

    // As long as the current pixels has not updated, the overuse signal is
    // repeated at a fixed interval.
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kOveruse))
        .Times(3);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs * 3);

    // When the overuse signal has resulted in a lower resolution, the overuse
    // signals stops.
    SetCurrentPixels(kMaxPixels);
    EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_, _)).Times(0);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs * 3);

    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

TEST_F(PixelLimitResourceTest,
       UnderuseIsReportedWhileCurrentPixelsIsLessThanMinPixels) {
  constexpr int kMaxPixels = 640 * 480;
  const int kMinPixels = GetLowerResolutionThan(kMaxPixels);
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    rtc::scoped_refptr<PixelLimitResource> pixel_limit_resource =
        PixelLimitResource::Create(task_queue_.get(), &input_state_provider_);
    pixel_limit_resource->SetResourceListener(&resource_listener);
    time_controller_.AdvanceTime(TimeDelta::Zero());

    pixel_limit_resource->SetMaxPixels(kMaxPixels);
    SetCurrentPixels(kMinPixels - 1);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kUnderuse))
        .Times(1);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs);

    // As long as the current pixels has not updated, the underuse signal is
    // repeated at a fixed interval.
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kUnderuse))
        .Times(3);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs * 3);

    // When the underuse signal has resulted in a higher resolution, the
    // underuse signals stops.
    SetCurrentPixels(kMinPixels);
    EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_, _)).Times(0);
    time_controller_.AdvanceTime(kResourceUsageCheckIntervalMs * 3);

    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

}  // namespace webrtc
