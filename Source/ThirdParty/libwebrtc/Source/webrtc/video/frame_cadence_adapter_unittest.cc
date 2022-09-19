/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_cadence_adapter.h"

#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/event.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/ntp_time.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Pair;
using ::testing::Values;

VideoFrame CreateFrame() {
  return VideoFrame::Builder()
      .set_video_frame_buffer(
          rtc::make_ref_counted<NV12Buffer>(/*width=*/16, /*height=*/16))
      .build();
}

VideoFrame CreateFrameWithTimestamps(
    GlobalSimulatedTimeController* time_controller) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(
          rtc::make_ref_counted<NV12Buffer>(/*width=*/16, /*height=*/16))
      .set_ntp_time_ms(time_controller->GetClock()->CurrentNtpInMilliseconds())
      .set_timestamp_us(time_controller->GetClock()->CurrentTime().us())
      .build();
}

std::unique_ptr<FrameCadenceAdapterInterface> CreateAdapter(
    const FieldTrialsView& field_trials,
    Clock* clock) {
  return FrameCadenceAdapterInterface::Create(clock, TaskQueueBase::Current(),
                                              field_trials);
}

class MockCallback : public FrameCadenceAdapterInterface::Callback {
 public:
  MOCK_METHOD(void, OnFrame, (Timestamp, int, const VideoFrame&), (override));
  MOCK_METHOD(void, OnDiscardedFrame, (), (override));
  MOCK_METHOD(void, RequestRefreshFrame, (), (override));
};

class ZeroHertzFieldTrialDisabler : public test::ScopedKeyValueConfig {
 public:
  ZeroHertzFieldTrialDisabler()
      : test::ScopedKeyValueConfig("WebRTC-ZeroHertzScreenshare/Disabled/") {}
};

class ZeroHertzFieldTrialEnabler : public test::ScopedKeyValueConfig {
 public:
  ZeroHertzFieldTrialEnabler()
      : test::ScopedKeyValueConfig("WebRTC-ZeroHertzScreenshare/Enabled/") {}
};

TEST(FrameCadenceAdapterTest,
     ForwardsFramesOnConstructionAndUnderDisabledFieldTrial) {
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(1));
  ZeroHertzFieldTrialDisabler disabled_field_trials;
  test::ScopedKeyValueConfig no_field_trials;
  for (int i = 0; i != 2; i++) {
    MockCallback callback;
    auto adapter =
        CreateAdapter(i == 0 ? disabled_field_trials : no_field_trials,
                      time_controller.GetClock());
    adapter->Initialize(&callback);
    VideoFrame frame = CreateFrame();
    EXPECT_CALL(callback, OnFrame).Times(1);
    adapter->OnFrame(frame);
    time_controller.AdvanceTime(TimeDelta::Zero());
    Mock::VerifyAndClearExpectations(&callback);
    EXPECT_CALL(callback, OnDiscardedFrame).Times(1);
    adapter->OnDiscardedFrame();
    Mock::VerifyAndClearExpectations(&callback);
  }
}

TEST(FrameCadenceAdapterTest, CountsOutstandingFramesToProcess) {
  test::ScopedKeyValueConfig no_field_trials;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(1));
  MockCallback callback;
  auto adapter = CreateAdapter(no_field_trials, time_controller.GetClock());
  adapter->Initialize(&callback);
  EXPECT_CALL(callback, OnFrame(_, 2, _)).Times(1);
  EXPECT_CALL(callback, OnFrame(_, 1, _)).Times(1);
  auto frame = CreateFrame();
  adapter->OnFrame(frame);
  adapter->OnFrame(frame);
  time_controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_CALL(callback, OnFrame(_, 1, _)).Times(1);
  adapter->OnFrame(frame);
  time_controller.AdvanceTime(TimeDelta::Zero());
}

TEST(FrameCadenceAdapterTest, FrameRateFollowsRateStatisticsByDefault) {
  test::ScopedKeyValueConfig no_field_trials;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(no_field_trials, time_controller.GetClock());
  adapter->Initialize(nullptr);

  // Create an "oracle" rate statistics which should be followed on a sequence
  // of frames.
  RateStatistics rate(
      FrameCadenceAdapterInterface::kFrameRateAveragingWindowSizeMs, 1000);

  for (int frame = 0; frame != 10; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
    adapter->UpdateFrameRate();
    EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
              adapter->GetInputFrameRateFps())
        << " failed for frame " << frame;
  }
}

TEST(FrameCadenceAdapterTest,
     FrameRateFollowsRateStatisticsWhenFeatureDisabled) {
  ZeroHertzFieldTrialDisabler feature_disabler;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(feature_disabler, time_controller.GetClock());
  adapter->Initialize(nullptr);

  // Create an "oracle" rate statistics which should be followed on a sequence
  // of frames.
  RateStatistics rate(
      FrameCadenceAdapterInterface::kFrameRateAveragingWindowSizeMs, 1000);

  for (int frame = 0; frame != 10; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
    adapter->UpdateFrameRate();
    EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
              adapter->GetInputFrameRateFps())
        << " failed for frame " << frame;
  }
}

TEST(FrameCadenceAdapterTest, FrameRateFollowsMaxFpsWhenZeroHertzActivated) {
  ZeroHertzFieldTrialEnabler enabler;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(nullptr);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  for (int frame = 0; frame != 10; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    adapter->UpdateFrameRate();
    EXPECT_EQ(adapter->GetInputFrameRateFps(), 1u);
  }
}

TEST(FrameCadenceAdapterTest,
     FrameRateFollowsRateStatisticsAfterZeroHertzDeactivated) {
  ZeroHertzFieldTrialEnabler enabler;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(nullptr);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  RateStatistics rate(
      FrameCadenceAdapterInterface::kFrameRateAveragingWindowSizeMs, 1000);
  constexpr int MAX = 10;
  for (int frame = 0; frame != MAX; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
    adapter->UpdateFrameRate();
  }
  // Turn off zero hertz on the next-last frame; after the last frame we
  // should see a value that tracks the rate oracle.
  adapter->SetZeroHertzModeEnabled(absl::nullopt);
  // Last frame.
  time_controller.AdvanceTime(TimeDelta::Millis(10));
  rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
  adapter->UpdateFrameRate();

  EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
            adapter->GetInputFrameRateFps());
}

TEST(FrameCadenceAdapterTest, ForwardsFramesDelayed) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  constexpr int kNumFrames = 3;
  NtpTime original_ntp_time = time_controller.GetClock()->CurrentNtpTime();
  auto frame = CreateFrameWithTimestamps(&time_controller);
  int64_t original_timestamp_us = frame.timestamp_us();
  for (int index = 0; index != kNumFrames; ++index) {
    EXPECT_CALL(callback, OnFrame).Times(0);
    adapter->OnFrame(frame);
    EXPECT_CALL(callback, OnFrame)
        .WillOnce(Invoke([&](Timestamp post_time, int,
                             const VideoFrame& frame) {
          EXPECT_EQ(post_time, time_controller.GetClock()->CurrentTime());
          EXPECT_EQ(frame.timestamp_us(),
                    original_timestamp_us + index * rtc::kNumMicrosecsPerSec);
          EXPECT_EQ(frame.ntp_time_ms(), original_ntp_time.ToMs() +
                                             index * rtc::kNumMillisecsPerSec);
        }));
    time_controller.AdvanceTime(TimeDelta::Seconds(1));
    frame = CreateFrameWithTimestamps(&time_controller);
  }
}

TEST(FrameCadenceAdapterTest, RepeatsFramesDelayed) {
  // Logic in the frame cadence adapter avoids modifying frame NTP and render
  // timestamps if these timestamps looks unset, which is the case when the
  // clock is initialized running from 0. For this reason we choose the
  // `time_controller` initialization constant to something arbitrary which is
  // not 0.
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(47892223));
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  NtpTime original_ntp_time = time_controller.GetClock()->CurrentNtpTime();

  // Send one frame, expect 2 subsequent repeats.
  auto frame = CreateFrameWithTimestamps(&time_controller);
  int64_t original_timestamp_us = frame.timestamp_us();
  adapter->OnFrame(frame);

  EXPECT_CALL(callback, OnFrame)
      .WillOnce(Invoke([&](Timestamp post_time, int, const VideoFrame& frame) {
        EXPECT_EQ(post_time, time_controller.GetClock()->CurrentTime());
        EXPECT_EQ(frame.timestamp_us(), original_timestamp_us);
        EXPECT_EQ(frame.ntp_time_ms(), original_ntp_time.ToMs());
      }));
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, OnFrame)
      .WillOnce(Invoke([&](Timestamp post_time, int, const VideoFrame& frame) {
        EXPECT_EQ(post_time, time_controller.GetClock()->CurrentTime());
        EXPECT_EQ(frame.timestamp_us(),
                  original_timestamp_us + rtc::kNumMicrosecsPerSec);
        EXPECT_EQ(frame.ntp_time_ms(),
                  original_ntp_time.ToMs() + rtc::kNumMillisecsPerSec);
      }));
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, OnFrame)
      .WillOnce(Invoke([&](Timestamp post_time, int, const VideoFrame& frame) {
        EXPECT_EQ(post_time, time_controller.GetClock()->CurrentTime());
        EXPECT_EQ(frame.timestamp_us(),
                  original_timestamp_us + 2 * rtc::kNumMicrosecsPerSec);
        EXPECT_EQ(frame.ntp_time_ms(),
                  original_ntp_time.ToMs() + 2 * rtc::kNumMillisecsPerSec);
      }));
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameCadenceAdapterTest,
     RepeatsFramesWithoutTimestampsWithUnsetTimestamps) {
  // Logic in the frame cadence adapter avoids modifying frame NTP and render
  // timestamps if these timestamps looks unset, which is the case when the
  // clock is initialized running from 0. In this test we deliberately don't set
  // it to zero, but select unset timestamps in the frames (via CreateFrame())
  // and verify that the timestamp modifying logic doesn't depend on the current
  // time.
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(4711));
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});

  // Send one frame, expect a repeat.
  adapter->OnFrame(CreateFrame());
  EXPECT_CALL(callback, OnFrame)
      .WillOnce(Invoke([&](Timestamp post_time, int, const VideoFrame& frame) {
        EXPECT_EQ(post_time, time_controller.GetClock()->CurrentTime());
        EXPECT_EQ(frame.timestamp_us(), 0);
        EXPECT_EQ(frame.ntp_time_ms(), 0);
      }));
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);
  EXPECT_CALL(callback, OnFrame)
      .WillOnce(Invoke([&](Timestamp post_time, int, const VideoFrame& frame) {
        EXPECT_EQ(post_time, time_controller.GetClock()->CurrentTime());
        EXPECT_EQ(frame.timestamp_us(), 0);
        EXPECT_EQ(frame.ntp_time_ms(), 0);
      }));
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameCadenceAdapterTest, StopsRepeatingFramesDelayed) {
  // At 1s, the initially scheduled frame appears.
  // At 2s, the repeated initial frame appears.
  // At 2.5s, we schedule another new frame.
  // At 3.5s, we receive this frame.
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  NtpTime original_ntp_time = time_controller.GetClock()->CurrentNtpTime();

  // Send one frame, expect 1 subsequent repeat.
  adapter->OnFrame(CreateFrameWithTimestamps(&time_controller));
  EXPECT_CALL(callback, OnFrame).Times(2);
  time_controller.AdvanceTime(TimeDelta::Seconds(2.5));
  Mock::VerifyAndClearExpectations(&callback);

  // Send the new frame at 2.5s, which should appear after 3.5s.
  adapter->OnFrame(CreateFrameWithTimestamps(&time_controller));
  EXPECT_CALL(callback, OnFrame)
      .WillOnce(Invoke([&](Timestamp, int, const VideoFrame& frame) {
        EXPECT_EQ(frame.timestamp_us(), 5 * rtc::kNumMicrosecsPerSec / 2);
        EXPECT_EQ(frame.ntp_time_ms(),
                  original_ntp_time.ToMs() + 5u * rtc::kNumMillisecsPerSec / 2);
      }));
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameCadenceAdapterTest, RequestsRefreshFrameOnKeyFrameRequestWhenNew) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  constexpr int kMaxFps = 10;
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, kMaxFps});
  EXPECT_CALL(callback, RequestRefreshFrame);
  time_controller.AdvanceTime(
      TimeDelta::Seconds(1) *
      FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod /
      kMaxFps);
  adapter->ProcessKeyFrameRequest();
}

TEST(FrameCadenceAdapterTest, IgnoresKeyFrameRequestShortlyAfterFrame) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 10});
  adapter->OnFrame(CreateFrame());
  time_controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);
  adapter->ProcessKeyFrameRequest();
}

TEST(FrameCadenceAdapterTest, RequestsRefreshFramesUntilArrival) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  constexpr int kMaxFps = 10;
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, kMaxFps});

  // We should see max_fps + 1 -
  // FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod refresh
  // frame requests during the one second we wait until we send a single frame,
  // after which refresh frame requests should cease (we should see no such
  // requests during a second).
  EXPECT_CALL(callback, RequestRefreshFrame)
      .Times(kMaxFps + 1 -
             FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod);
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);
  adapter->OnFrame(CreateFrame());
  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameCadenceAdapterTest, RequestsRefreshAfterFrameDrop) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  constexpr int kMaxFps = 10;
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, kMaxFps});

  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);

  // Send a frame through to cancel the initial delayed timer waiting for first
  // frame entry.
  adapter->OnFrame(CreateFrame());
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);

  // Send a dropped frame indication without any following frames received.
  // After FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod
  // frame periods, we should receive a first refresh request.
  adapter->OnDiscardedFrame();
  EXPECT_CALL(callback, RequestRefreshFrame);
  time_controller.AdvanceTime(
      TimeDelta::Seconds(1) *
      FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod /
      kMaxFps);
  Mock::VerifyAndClearExpectations(&callback);

  // We will now receive a refresh frame request for every frame period.
  EXPECT_CALL(callback, RequestRefreshFrame).Times(kMaxFps);
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);

  // After a frame is passed the requests will cease.
  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);
  adapter->OnFrame(CreateFrame());
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameCadenceAdapterTest, OmitsRefreshAfterFrameDropWithTimelyFrameEntry) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Zero());
  auto adapter = CreateAdapter(enabler, time_controller.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  constexpr int kMaxFps = 10;
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, kMaxFps});

  // Send a frame through to cancel the initial delayed timer waiting for first
  // frame entry.
  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);
  adapter->OnFrame(CreateFrame());
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);

  // Send a frame drop indication. No refresh frames should be requested
  // until FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod
  // intervals pass. Stop short of this.
  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);
  adapter->OnDiscardedFrame();
  time_controller.AdvanceTime(
      TimeDelta::Seconds(1) *
          FrameCadenceAdapterInterface::kOnDiscardedFrameRefreshFramePeriod /
          kMaxFps -
      TimeDelta::Micros(1));
  Mock::VerifyAndClearExpectations(&callback);

  // Send a frame. The timer to request the refresh frame should be cancelled by
  // the reception, so no refreshes should be requested.
  EXPECT_CALL(callback, RequestRefreshFrame).Times(0);
  adapter->OnFrame(CreateFrame());
  time_controller.AdvanceTime(TimeDelta::Seconds(1));
  Mock::VerifyAndClearExpectations(&callback);
}

class FrameCadenceAdapterSimulcastLayersParamTest
    : public ::testing::TestWithParam<int> {
 public:
  static constexpr int kMaxFpsHz = 8;
  static constexpr TimeDelta kMinFrameDelay =
      TimeDelta::Millis(1000 / kMaxFpsHz);
  static constexpr TimeDelta kIdleFrameDelay =
      FrameCadenceAdapterInterface::kZeroHertzIdleRepeatRatePeriod;

  FrameCadenceAdapterSimulcastLayersParamTest() {
    adapter_->Initialize(&callback_);
    adapter_->OnConstraintsChanged(VideoTrackSourceConstraints{0, kMaxFpsHz});
    time_controller_.AdvanceTime(TimeDelta::Zero());
    adapter_->SetZeroHertzModeEnabled(
        FrameCadenceAdapterInterface::ZeroHertzModeParams{});
    const int num_spatial_layers = GetParam();
    adapter_->SetZeroHertzModeEnabled(
        FrameCadenceAdapterInterface::ZeroHertzModeParams{num_spatial_layers});
  }

  int NumSpatialLayers() const { return GetParam(); }

 protected:
  ZeroHertzFieldTrialEnabler enabler_;
  MockCallback callback_;
  GlobalSimulatedTimeController time_controller_{Timestamp::Zero()};
  const std::unique_ptr<FrameCadenceAdapterInterface> adapter_{
      CreateAdapter(enabler_, time_controller_.GetClock())};
};

TEST_P(FrameCadenceAdapterSimulcastLayersParamTest,
       LayerReconfigurationResetsConvergenceInfo) {
  // Assumes layer reconfiguration has just happened.
  // Verify the state is unconverged.
  adapter_->OnFrame(CreateFrame());
  EXPECT_CALL(callback_, OnFrame).Times(kMaxFpsHz);
  time_controller_.AdvanceTime(kMaxFpsHz * kMinFrameDelay);
}

TEST_P(FrameCadenceAdapterSimulcastLayersParamTest,
       IgnoresKeyFrameRequestWhileShortRepeating) {
  // Plot:
  // 1. 0 * kMinFrameDelay: Start unconverged. Frame -> adapter.
  // 2. 1 * kMinFrameDelay: Frame -> callback.
  // 3. 2 * kMinFrameDelay: 1st short repeat.
  // Since we're unconverged we assume the process continues.
  adapter_->OnFrame(CreateFrame());
  time_controller_.AdvanceTime(2 * kMinFrameDelay);
  EXPECT_CALL(callback_, RequestRefreshFrame).Times(0);
  adapter_->ProcessKeyFrameRequest();

  // Expect short repeating as ususal.
  EXPECT_CALL(callback_, OnFrame).Times(8);
  time_controller_.AdvanceTime(8 * kMinFrameDelay);
}

TEST_P(FrameCadenceAdapterSimulcastLayersParamTest,
       IgnoresKeyFrameRequestJustBeforeIdleRepeating) {
  // (Only for > 0 spatial layers as we assume not converged with 0 layers)
  if (NumSpatialLayers() == 0)
    return;

  // Plot:
  // 1. 0 * kMinFrameDelay: Start converged. Frame -> adapter.
  // 2. 1 * kMinFrameDelay: Frame -> callback. New repeat scheduled at
  //    (kMaxFpsHz + 1) * kMinFrameDelay.
  // 3. kMaxFpsHz * kMinFrameDelay: Process keyframe.
  // 4. (kMaxFpsHz + N) * kMinFrameDelay (1 <= N <= kMaxFpsHz): Short repeats
  //    due to not converged.
  for (int i = 0; i != NumSpatialLayers(); i++) {
    adapter_->UpdateLayerStatus(i, /*enabled=*/true);
    adapter_->UpdateLayerQualityConvergence(i, /*converged=*/true);
  }
  adapter_->OnFrame(CreateFrame());
  time_controller_.AdvanceTime(kIdleFrameDelay);

  // We process the key frame request kMinFrameDelay before the first idle
  // repeat should happen. The resulting repeats should happen spaced by
  // kMinFrameDelay before we get new convergence info.
  EXPECT_CALL(callback_, RequestRefreshFrame).Times(0);
  adapter_->ProcessKeyFrameRequest();
  EXPECT_CALL(callback_, OnFrame).Times(kMaxFpsHz);
  time_controller_.AdvanceTime(kMaxFpsHz * kMinFrameDelay);
}

TEST_P(FrameCadenceAdapterSimulcastLayersParamTest,
       IgnoresKeyFrameRequestShortRepeatsBeforeIdleRepeat) {
  // (Only for > 0 spatial layers as we assume not converged with 0 layers)
  if (NumSpatialLayers() == 0)
    return;
  // Plot:
  // 1. 0 * kMinFrameDelay: Start converged. Frame -> adapter.
  // 2. 1 * kMinFrameDelay: Frame -> callback. New repeat scheduled at
  //    (kMaxFpsHz + 1) * kMinFrameDelay.
  // 3. 2 * kMinFrameDelay: Process keyframe.
  // 4. (2 + N) * kMinFrameDelay (1 <= N <= kMaxFpsHz): Short repeats due to not
  //    converged.
  for (int i = 0; i != NumSpatialLayers(); i++) {
    adapter_->UpdateLayerStatus(i, /*enabled=*/true);
    adapter_->UpdateLayerQualityConvergence(i, /*converged=*/true);
  }
  adapter_->OnFrame(CreateFrame());
  time_controller_.AdvanceTime(2 * kMinFrameDelay);

  // We process the key frame request (kMaxFpsHz - 1) * kMinFrameDelay before
  // the first idle repeat should happen. The resulting repeats should happen
  // spaced kMinFrameDelay before we get new convergence info.
  EXPECT_CALL(callback_, RequestRefreshFrame).Times(0);
  adapter_->ProcessKeyFrameRequest();
  EXPECT_CALL(callback_, OnFrame).Times(kMaxFpsHz);
  time_controller_.AdvanceTime(kMaxFpsHz * kMinFrameDelay);
}

INSTANTIATE_TEST_SUITE_P(,
                         FrameCadenceAdapterSimulcastLayersParamTest,
                         Values(0, 1, 2));

class ZeroHertzLayerQualityConvergenceTest : public ::testing::Test {
 public:
  static constexpr TimeDelta kMinFrameDelay = TimeDelta::Millis(100);
  static constexpr TimeDelta kIdleFrameDelay =
      FrameCadenceAdapterInterface::kZeroHertzIdleRepeatRatePeriod;

  ZeroHertzLayerQualityConvergenceTest() {
    adapter_->Initialize(&callback_);
    adapter_->SetZeroHertzModeEnabled(
        FrameCadenceAdapterInterface::ZeroHertzModeParams{
            /*num_simulcast_layers=*/2});
    adapter_->OnConstraintsChanged(VideoTrackSourceConstraints{
        /*min_fps=*/0, /*max_fps=*/TimeDelta::Seconds(1) / kMinFrameDelay});
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  void PassFrame() { adapter_->OnFrame(CreateFrame()); }

  void ExpectFrameEntriesAtDelaysFromNow(
      std::initializer_list<TimeDelta> list) {
    Timestamp origin = time_controller_.GetClock()->CurrentTime();
    for (auto delay : list) {
      EXPECT_CALL(callback_, OnFrame(origin + delay, _, _));
      time_controller_.AdvanceTime(origin + delay -
                                   time_controller_.GetClock()->CurrentTime());
    }
  }

  void ScheduleDelayed(TimeDelta delay, absl::AnyInvocable<void() &&> task) {
    TaskQueueBase::Current()->PostDelayedTask(std::move(task), delay);
  }

 protected:
  ZeroHertzFieldTrialEnabler field_trial_enabler_;
  MockCallback callback_;
  GlobalSimulatedTimeController time_controller_{Timestamp::Zero()};
  std::unique_ptr<FrameCadenceAdapterInterface> adapter_{
      CreateAdapter(field_trial_enabler_, time_controller_.GetClock())};
};

TEST_F(ZeroHertzLayerQualityConvergenceTest, InitialStateUnconverged) {
  // As the layer count is just configured, assume we start out as unconverged.
  PassFrame();
  ExpectFrameEntriesAtDelaysFromNow({
      1 * kMinFrameDelay,  // Original frame emitted
      2 * kMinFrameDelay,  // Short repeats.
      3 * kMinFrameDelay,  // ...
  });
}

TEST_F(ZeroHertzLayerQualityConvergenceTest, UnconvergedAfterLayersEnabled) {
  // With newly enabled layers we assume quality is unconverged.
  adapter_->UpdateLayerStatus(0, /*enabled=*/true);
  adapter_->UpdateLayerStatus(1, /*enabled=*/true);
  PassFrame();
  ExpectFrameEntriesAtDelaysFromNow({
      kMinFrameDelay,      // Original frame emitted
      2 * kMinFrameDelay,  // Unconverged repeats.
      3 * kMinFrameDelay,  // ...
  });
}

TEST_F(ZeroHertzLayerQualityConvergenceTest,
       RepeatsPassedFramesUntilConvergence) {
  ScheduleDelayed(TimeDelta::Zero(), [&] {
    adapter_->UpdateLayerStatus(0, /*enabled=*/true);
    adapter_->UpdateLayerStatus(1, /*enabled=*/true);
    PassFrame();
  });
  ScheduleDelayed(2.5 * kMinFrameDelay, [&] {
    adapter_->UpdateLayerQualityConvergence(/*spatial_index=*/1, true);
  });
  ScheduleDelayed(3.5 * kMinFrameDelay, [&] {
    adapter_->UpdateLayerQualityConvergence(/*spatial_index=*/0, true);
  });
  ScheduleDelayed(8 * kMinFrameDelay, [&] { PassFrame(); });
  ScheduleDelayed(9.5 * kMinFrameDelay, [&] {
    adapter_->UpdateLayerQualityConvergence(/*spatial_index=*/0, true);
  });
  ScheduleDelayed(10.5 * kMinFrameDelay, [&] {
    adapter_->UpdateLayerQualityConvergence(/*spatial_index=*/1, true);
  });
  ExpectFrameEntriesAtDelaysFromNow({
      kMinFrameDelay,      // Original frame emitted
      2 * kMinFrameDelay,  // Repeat from kMinFrameDelay.

      // 2.5 * kMinFrameDelay: Converged in layer 1, layer 0 still unconverged.
      3 * kMinFrameDelay,  // Repeat from 2 * kMinFrameDelay.

      // 3.5 * kMinFrameDelay: Converged in layer 0 as well.
      4 * kMinFrameDelay,  // Repeat from 3 * kMinFrameDelay. An idle repeat is
                           // scheduled for kIdleFrameDelay + 3 *
                           // kMinFrameDelay.

      // A new frame is passed at 8 * kMinFrameDelay.
      9 * kMinFrameDelay,  // Original frame emitted

      // 9.5 * kMinFrameDelay: Converged in layer 0, layer 1 still unconverged.
      10 * kMinFrameDelay,  // Repeat from 9 * kMinFrameDelay.
      // 10.5 * kMinFrameDelay: Converged in layer 0 as well.
      11 * kMinFrameDelay,                        // Idle repeats from 1000.
      11 * kMinFrameDelay + kIdleFrameDelay,      // ...
      11 * kMinFrameDelay + 2 * kIdleFrameDelay,  // ...
                                                  // ...
  });
}

class FrameCadenceAdapterMetricsTest : public ::testing::Test {
 public:
  FrameCadenceAdapterMetricsTest() : time_controller_(Timestamp::Millis(1)) {
    metrics::Reset();
  }
  void DepleteTaskQueues() { time_controller_.AdvanceTime(TimeDelta::Zero()); }

 protected:
  GlobalSimulatedTimeController time_controller_;
};

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoUmasWithNoFrameTransfer) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, nullptr);
  adapter->Initialize(&callback);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{absl::nullopt, 1});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{2, 3});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4, 4});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5, absl::nullopt});
  DepleteTaskQueues();
  EXPECT_TRUE(metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoUmasWithoutEnabledContentType) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->OnFrame(CreateFrame());
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{absl::nullopt, 1});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{2, 3});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4, 4});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5, absl::nullopt});
  DepleteTaskQueues();
  EXPECT_TRUE(metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoConstraintsIfUnsetOnFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists"),
      ElementsAre(Pair(false, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsEmptyConstraintsIfSetOnFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, 2.0});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value"),
      ElementsAre(Pair(2.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max"),
      ElementsAre(Pair(2.0, 1)));
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinConstraintIfSetOnFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{3.0, absl::nullopt});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value"),
      ElementsAre(Pair(3.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinGtMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5.0, 4.0});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value"),
      ElementsAre(Pair(5.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value"),
      ElementsAre(Pair(4.0, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne"),
      ElementsAre(Pair(60 * 5.0 + 4.0 - 1, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinLtMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4.0, 5.0});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min"),
              ElementsAre(Pair(4.0, 1)));
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max"),
              ElementsAre(Pair(5.0, 1)));
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne"),
      ElementsAre(Pair(60 * 4.0 + 5.0 - 1, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsTimeUntilFirstFrame) {
  MockCallback callback;
  test::ScopedKeyValueConfig no_field_trials;
  auto adapter = CreateAdapter(no_field_trials, time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(
      FrameCadenceAdapterInterface::ZeroHertzModeParams{});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 5.0});
  time_controller_.AdvanceTime(TimeDelta::Millis(666));
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.ZeroHz.TimeUntilFirstFrameMs"),
      ElementsAre(Pair(666, 1)));
}

TEST(FrameCadenceAdapterRealTimeTest, TimestampsDoNotDrift) {
  // This regression test must be performed in realtime because of limitations
  // in GlobalSimulatedTimeController.
  //
  // We sleep for a long while in OnFrame when a repeat was scheduled which
  // should reflect in accordingly increased ntp_time_ms() and timestamp_us() in
  // the repeated frames.
  auto factory = CreateDefaultTaskQueueFactory();
  auto queue =
      factory->CreateTaskQueue("test", TaskQueueFactory::Priority::NORMAL);
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  Clock* clock = Clock::GetRealTimeClock();
  std::unique_ptr<FrameCadenceAdapterInterface> adapter;
  int frame_counter = 0;
  int64_t original_ntp_time_ms;
  int64_t original_timestamp_us;
  rtc::Event event;
  queue->PostTask([&] {
    adapter = CreateAdapter(enabler, clock);
    adapter->Initialize(&callback);
    adapter->SetZeroHertzModeEnabled(
        FrameCadenceAdapterInterface::ZeroHertzModeParams{});
    adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 30});
    auto frame = CreateFrame();
    original_ntp_time_ms = clock->CurrentNtpInMilliseconds();
    frame.set_ntp_time_ms(original_ntp_time_ms);
    original_timestamp_us = clock->CurrentTime().us();
    frame.set_timestamp_us(original_timestamp_us);
    constexpr int kSleepMs = rtc::kNumMillisecsPerSec / 2;
    EXPECT_CALL(callback, OnFrame)
        .WillRepeatedly(
            Invoke([&](Timestamp, int, const VideoFrame& incoming_frame) {
              ++frame_counter;
              // Avoid the first OnFrame and sleep on the second.
              if (frame_counter == 2) {
                SleepMs(kSleepMs);
              } else if (frame_counter == 3) {
                EXPECT_GE(incoming_frame.ntp_time_ms(),
                          original_ntp_time_ms + kSleepMs);
                EXPECT_GE(incoming_frame.timestamp_us(),
                          original_timestamp_us + kSleepMs);
                event.Set();
              }
            }));
    adapter->OnFrame(frame);
  });
  event.Wait(rtc::Event::kForever);
  rtc::Event finalized;
  queue->PostTask([&] {
    adapter = nullptr;
    finalized.Set();
  });
  finalized.Wait(rtc::Event::kForever);
}

}  // namespace
}  // namespace webrtc
