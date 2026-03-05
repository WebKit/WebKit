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
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/adaptation/resource.h"
#include "api/field_trials.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_adaptation_reason.h"
#include "call/adaptation/test/fake_video_stream_input_state_provider.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "call/adaptation/video_stream_adapter.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using testing::_;

namespace webrtc {

namespace {

constexpr TimeDelta kInterval = TimeDelta::Seconds(1);

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
  }

 protected:
  // Posted tasks, including repeated tasks, are executed when simulated time is
  // advanced by time_controller_.AdvanceTime().
  GlobalSimulatedTimeController time_controller_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
  FakeVideoStreamInputStateProvider input_state_provider_;
};

TEST_F(PixelLimitResourceTest, ResourceNotCreatedIfFieldTrialMissing) {
  FieldTrials field_trials = CreateTestFieldTrials("");
  EXPECT_FALSE(PixelLimitResource::CreateIfFieldTrialEnabled(
      field_trials, task_queue_.get(), &input_state_provider_));
}

TEST_F(PixelLimitResourceTest,
       NothingIsReportedWhileCurrentPixelsIsMissingOrEqualToMaxPixels) {
  constexpr int kMaxPixels = 640 * 480;
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-PixelLimitResource/target_pixels:" + std::to_string(kMaxPixels) +
      ",interval:" + ToString(kInterval) + "/");
  // Because our mock is strick, the test would fail if
  // OnResourceUsageStateMeasured() is invoked.
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
        field_trials, task_queue_.get(), &input_state_provider_);
    ASSERT_TRUE(pixel_limit_resource);
    pixel_limit_resource->SetResourceListener(&resource_listener);
    time_controller_.AdvanceTime(kInterval * 10);
    SetCurrentPixels(kMaxPixels);
    time_controller_.AdvanceTime(kInterval * 10);
    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

TEST_F(PixelLimitResourceTest,
       OveruseIsReportedWhileCurrentPixelsIsGreaterThanMaxPixels) {
  constexpr int kMaxPixels = 640 * 480;
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-PixelLimitResource/target_pixels:" + std::to_string(kMaxPixels) +
      ",interval:" + ToString(kInterval) + "/");
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
        field_trials, task_queue_.get(), &input_state_provider_);
    ASSERT_TRUE(pixel_limit_resource);
    pixel_limit_resource->SetResourceListener(&resource_listener);

    SetCurrentPixels(kMaxPixels + 1);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kOveruse))
        .Times(1);
    time_controller_.AdvanceTime(kInterval);

    // As long as the current pixels has not updated, the overuse signal is
    // repeated at a fixed interval.
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kOveruse))
        .Times(3);
    time_controller_.AdvanceTime(kInterval * 3);

    // When the overuse signal has resulted in a lower resolution, the overuse
    // signals stops.
    SetCurrentPixels(kMaxPixels);
    EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_, _)).Times(0);
    time_controller_.AdvanceTime(kInterval * 3);

    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

TEST_F(PixelLimitResourceTest,
       UnderuseIsReportedWhileCurrentPixelsIsLessThanMinPixels) {
  constexpr int kMaxPixels = 640 * 480;
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-PixelLimitResource/target_pixels:" + std::to_string(kMaxPixels) +
      ",interval:" + ToString(kInterval) + "/");
  const int kMinPixels = GetLowerResolutionThan(kMaxPixels);
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
        field_trials, task_queue_.get(), &input_state_provider_);
    ASSERT_TRUE(pixel_limit_resource);
    pixel_limit_resource->SetResourceListener(&resource_listener);

    SetCurrentPixels(kMinPixels - 1);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kUnderuse))
        .Times(1);
    time_controller_.AdvanceTime(kInterval);

    // As long as the current pixels has not updated, the underuse signal is
    // repeated at a fixed interval.
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kUnderuse))
        .Times(3);
    time_controller_.AdvanceTime(kInterval * 3);

    // When the underuse signal has resulted in a higher resolution, the
    // underuse signals stops.
    SetCurrentPixels(kMinPixels);
    EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_, _)).Times(0);
    time_controller_.AdvanceTime(kInterval * 3);

    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

TEST_F(PixelLimitResourceTest, PeriodicallyAdaptsUpWhenToggling) {
  constexpr int kMaxPixels = 640 * 360;
  constexpr TimeDelta kToggleInterval = kInterval * 2;
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-PixelLimitResource/target_pixels:" + std::to_string(kMaxPixels) +
      ",interval:" + ToString(kInterval) +
      ",toggle:" + ToString(kToggleInterval) + "/");
  testing::StrictMock<MockResourceListener> resource_listener;
  RunTaskOnTaskQueue([&]() {
    auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
        field_trials, task_queue_.get(), &input_state_provider_);
    ASSERT_TRUE(pixel_limit_resource);
    pixel_limit_resource->SetResourceListener(&resource_listener);
    SetCurrentPixels(1280 * 720);

    // Since kToggleInterval is kInterval * 2, we should see two signals per
    // toggle.
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kOveruse))
        .Times(2);
    time_controller_.AdvanceTime(kToggleInterval);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kUnderuse))
        .Times(2);
    time_controller_.AdvanceTime(kToggleInterval);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kOveruse))
        .Times(2);
    time_controller_.AdvanceTime(kToggleInterval);
    EXPECT_CALL(resource_listener,
                OnResourceUsageStateMeasured(_, ResourceUsageState::kUnderuse))
        .Times(2);
    time_controller_.AdvanceTime(kToggleInterval);
    // And so on...

    pixel_limit_resource->SetResourceListener(nullptr);
  });
}

TEST_F(PixelLimitResourceTest, AdaptationReasonIsCpuByDefault) {
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-PixelLimitResource/target_pixels:123/");
  auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
      field_trials, task_queue_.get(), &input_state_provider_);
  EXPECT_EQ(pixel_limit_resource->adaptation_reason(),
            VideoAdaptationReason::kCpu);
}

TEST_F(PixelLimitResourceTest, CanSpecifyAdaptationReason) {
  {
    FieldTrials field_trials =
        CreateTestFieldTrials("WebRTC-PixelLimitResource/reason:quality/");
    auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
        field_trials, task_queue_.get(), &input_state_provider_);
    EXPECT_EQ(pixel_limit_resource->adaptation_reason(),
              VideoAdaptationReason::kQuality);
  }
  {
    FieldTrials field_trials =
        CreateTestFieldTrials("WebRTC-PixelLimitResource/reason:cpu/");
    auto pixel_limit_resource = PixelLimitResource::CreateIfFieldTrialEnabled(
        field_trials, task_queue_.get(), &input_state_provider_);
    EXPECT_EQ(pixel_limit_resource->adaptation_reason(),
              VideoAdaptationReason::kCpu);
  }
}

}  // namespace webrtc
