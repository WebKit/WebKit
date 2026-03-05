/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/overuse_frame_detector.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "api/array_view.h"
#include "api/environment/environment.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/event.h"
#include "rtc_base/random.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/video_stream_encoder_observer.h"

namespace webrtc {
namespace {

using ::testing::InvokeWithoutArgs;

constexpr int kWidth = 640;
constexpr int kHeight = 480;
// Corresponds to load of 15%
constexpr TimeDelta kFrameInterval = TimeDelta::Millis(33);
constexpr TimeDelta kProcessTime = TimeDelta::Millis(5);

class MockCpuOveruseObserver : public OveruseFrameDetectorObserverInterface {
 public:
  MockCpuOveruseObserver() {}
  ~MockCpuOveruseObserver() override {}

  MOCK_METHOD(void, AdaptUp, (), (override));
  MOCK_METHOD(void, AdaptDown, (), (override));
};

class CpuOveruseObserverImpl : public OveruseFrameDetectorObserverInterface {
 public:
  CpuOveruseObserverImpl() : overuse_(0), normaluse_(0) {}
  ~CpuOveruseObserverImpl() override {}

  void AdaptDown() override { ++overuse_; }
  void AdaptUp() override { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorUnderTest : public OveruseFrameDetector {
 public:
  explicit OveruseFrameDetectorUnderTest(
      const Environment& env,
      CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(env, metrics_observer) {}
  ~OveruseFrameDetectorUnderTest() override {}

  using OveruseFrameDetector::CheckForOveruse;
  using OveruseFrameDetector::SetOptions;
};

class OveruseFrameDetectorTest : public ::testing::Test,
                                 public CpuOveruseMetricsObserver {
 protected:
  void SetUp() override {
    observer_ = &mock_observer_;
    options_.min_process_count = 0;
    overuse_detector_ = std::make_unique<OveruseFrameDetectorUnderTest>(
        CreateTestEnvironment({.time = &clock_}), this);
    // Unfortunately, we can't call SetOptions here, since that would break
    // single-threading requirements in the RunOnTqNormalUsage test.
  }

  void OnEncodedFrameTimeMeasured(int encode_time_ms,
                                  int encode_usage_percent) override {
    encode_usage_percent_ = encode_usage_percent;
  }

  int InitialUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) /
            2.0f) +
           0.5;
  }

  virtual void InsertAndSendFramesWithInterval(int num_frames,
                                               TimeDelta interval_us,
                                               int width,
                                               int height,
                                               TimeDelta delay_us) {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(width, height))
            .set_rotation(kVideoRotation_0)
            .set_timestamp_us(0)
            .build();
    uint32_t timestamp = 0;
    while (num_frames-- > 0) {
      frame.set_rtp_timestamp(timestamp);
      int64_t capture_time_us = clock_.TimeInMicroseconds();
      overuse_detector_->FrameCaptured(frame, capture_time_us);
      clock_.AdvanceTime(delay_us);
      overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                   capture_time_us, delay_us.us());
      clock_.AdvanceTime(interval_us - delay_us);
      timestamp += interval_us.us() * 90 / 1000;
    }
  }

  virtual void InsertAndSendSimulcastFramesWithInterval(
      int num_frames,
      TimeDelta interval,
      int width,
      int height,
      // One element per layer
      ArrayView<const TimeDelta> delays_us) {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(width, height))
            .set_rotation(kVideoRotation_0)
            .set_timestamp_us(0)
            .build();
    uint32_t timestamp = 0;
    while (num_frames-- > 0) {
      frame.set_rtp_timestamp(timestamp);
      int64_t capture_time_us = clock_.TimeInMicroseconds();
      overuse_detector_->FrameCaptured(frame, capture_time_us);
      TimeDelta max_delay = TimeDelta::Zero();
      for (TimeDelta delay : delays_us) {
        if (delay > max_delay) {
          clock_.AdvanceTime(delay - max_delay);
          max_delay = delay;
        }

        overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                     capture_time_us, delay.us());
      }
      overuse_detector_->CheckForOveruse(observer_);
      clock_.AdvanceTime(interval - max_delay);
      timestamp += interval.us() * 90 / 1000;
    }
  }

  virtual void InsertAndSendFramesWithRandomInterval(int num_frames,
                                                     TimeDelta min_interval_us,
                                                     TimeDelta max_interval_us,
                                                     int width,
                                                     int height,
                                                     TimeDelta delay_us) {
    Random random(17);

    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(width, height))
            .set_rotation(kVideoRotation_0)
            .set_timestamp_us(0)
            .build();
    uint32_t timestamp = 0;
    while (num_frames-- > 0) {
      frame.set_rtp_timestamp(timestamp);
      TimeDelta interval_us = TimeDelta::Micros(random.Rand(
          min_interval_us.us<uint32_t>(), max_interval_us.us<uint32_t>()));
      int64_t capture_time_us = clock_.TimeInMicroseconds();
      overuse_detector_->FrameCaptured(frame, capture_time_us);
      clock_.AdvanceTime(delay_us);
      overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                   capture_time_us, delay_us.us());

      overuse_detector_->CheckForOveruse(observer_);
      // Avoid turning clock backwards.
      if (interval_us > delay_us)
        clock_.AdvanceTime(interval_us - delay_us);
      timestamp += interval_us.us() * 90 / 1000;
    }
  }

  virtual void ForceUpdate(int width, int height) {
    // Insert one frame, wait a second and then put in another to force update
    // the usage. From the tests where these are used, adding another sample
    // doesn't affect the expected outcome (this is mainly to check initial
    // values and whether the overuse detector has been reset or not).
    InsertAndSendFramesWithInterval(2, TimeDelta::Seconds(1), width, height,
                                    kFrameInterval);
  }
  void TriggerOveruse(int num_times) {
    const TimeDelta kDelay = TimeDelta::Millis(32);
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                      kDelay);
      overuse_detector_->CheckForOveruse(observer_);
    }
  }

  void TriggerUnderuse() {
    const TimeDelta kDelayUs1 = TimeDelta::Micros(5000);
    const TimeDelta kDelayUs2 = TimeDelta::Micros(6000);
    InsertAndSendFramesWithInterval(1300, kFrameInterval, kWidth, kHeight,
                                    kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameInterval, kWidth, kHeight,
                                    kDelayUs2);
    overuse_detector_->CheckForOveruse(observer_);
  }

  int UsagePercent() { return encode_usage_percent_; }

  TimeDelta OveruseProcessingTimeLimitForFramerate(int fps) const {
    TimeDelta frame_interval = TimeDelta::Seconds(1) / fps;
    TimeDelta max_processing_time_us =
        (frame_interval * options_.high_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  TimeDelta UnderuseProcessingTimeLimitForFramerate(int fps) const {
    TimeDelta frame_interval = TimeDelta::Seconds(1) / fps;
    TimeDelta max_processing_time_us =
        (frame_interval * options_.low_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  CpuOveruseOptions options_;
  SimulatedClock clock_{Timestamp::Millis(12345)};
  MockCpuOveruseObserver mock_observer_;
  OveruseFrameDetectorObserverInterface* observer_;
  std::unique_ptr<OveruseFrameDetectorUnderTest> overuse_detector_;
  int encode_usage_percent_ = -1;
};

// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // usage > high => overuse
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // usage > high => overuse
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(::testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(::testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, TriggerUnderuseWithMinProcessCount) {
  const TimeDelta kProcessInterval = TimeDelta::Seconds(5);
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  observer_ = nullptr;
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1200, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  overuse_detector_->CheckForOveruse(&overuse_observer);
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_.AdvanceTime(kProcessInterval);
  overuse_detector_->CheckForOveruse(&overuse_observer);
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(0);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_EQ(static_cast<int>(kProcessTime * 100 / kFrameInterval),
            UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterResolutionChange) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset (with new width/height).
  ForceUpdate(kWidth, kHeight + 1);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterFrameTimeout) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, TimeDelta::Millis(options_.frame_timeout_interval_ms), kWidth, kHeight,
      kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2, TimeDelta::Millis(options_.frame_timeout_interval_ms + 1), kWidth,
      kHeight, kProcessTime);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MinFrameSamplesBeforeUpdating) {
  options_.min_frame_samples = 40;
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(40, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  // Pass time far enough to digest all previous samples.
  clock_.AdvanceTime(TimeDelta::Seconds(1));
  InsertAndSendFramesWithInterval(1, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  // The last sample has not been processed here.
  EXPECT_EQ(InitialUsage(), UsagePercent());

  // Pass time far enough to digest all previous samples, 41 in total.
  clock_.AdvanceTime(TimeDelta::Seconds(1));
  InsertAndSendFramesWithInterval(1, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MeasuresMultipleConcurrentSamples) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(::testing::AtLeast(1));
  static TimeDelta kInterval = TimeDelta::Millis(33);
  static const size_t kNumFramesEncodingDelay = 3;
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(I420Buffer::Create(kWidth, kHeight))
          .set_rotation(kVideoRotation_0)
          .set_timestamp_us(0)
          .build();
  for (size_t i = 0; i < 1000; ++i) {
    // Unique timestamps.
    frame.set_rtp_timestamp(static_cast<uint32_t>(i));
    int64_t capture_time_us = clock_.TimeInMicroseconds();
    overuse_detector_->FrameCaptured(frame, capture_time_us);
    clock_.AdvanceTime(kInterval);
    if (i > kNumFramesEncodingDelay) {
      overuse_detector_->FrameSent(
          static_cast<uint32_t>(i - kNumFramesEncodingDelay),
          clock_.TimeInMicroseconds(), capture_time_us, kInterval.us());
    }
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest, UpdatesExistingSamples) {
  // >85% encoding time should trigger overuse.
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(::testing::AtLeast(1));
  static const TimeDelta kInterval = TimeDelta::Millis(33);
  static const TimeDelta kDelay = TimeDelta::Millis(30);
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(I420Buffer::Create(kWidth, kHeight))
          .set_rotation(kVideoRotation_0)
          .set_timestamp_us(0)
          .build();
  uint32_t timestamp = 0;
  for (size_t i = 0; i < 1000; ++i) {
    frame.set_rtp_timestamp(timestamp);
    int64_t capture_time_us = clock_.TimeInMicroseconds();
    overuse_detector_->FrameCaptured(frame, capture_time_us);
    // Encode and send first parts almost instantly.
    clock_.AdvanceTime(TimeDelta::Millis(1));
    overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                 capture_time_us, TimeDelta::Millis(1).us());
    // Encode heavier part, resulting in >85% usage total.
    clock_.AdvanceTime(kDelay - TimeDelta::Millis(1));
    overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                 capture_time_us, kDelay.us());
    clock_.AdvanceTime(kInterval - kDelay);
    timestamp += kInterval.us() * 90 / 1000;
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest, RunOnTqNormalUsage) {
  TaskQueueForTest queue("OveruseFrameDetectorTestQueue");

  queue.SendTask([&] {
    overuse_detector_->StartCheckForOveruse(queue.Get(), options_, observer_);
  });

  Event event;
  // Expect NormalUsage(). When called, stop the `overuse_detector_` and then
  // set `event` to end the test.
  EXPECT_CALL(mock_observer_, AdaptUp())
      .WillOnce(InvokeWithoutArgs([this, &event] {
        overuse_detector_->StopCheckForOveruse();
        event.Set();
      }));

  queue.PostTask([this] {
    const TimeDelta kDelayUs1 = TimeDelta::Millis(5);
    const TimeDelta kDelayUs2 = TimeDelta::Millis(6);
    InsertAndSendFramesWithInterval(1300, kFrameInterval, kWidth, kHeight,
                                    kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameInterval, kWidth, kHeight,
                                    kDelayUs2);
  });

  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(10)));
}

// TODO(crbug.com/webrtc/12846): investigate why the test fails on MAC bots.
#if !defined(WEBRTC_MAC)
TEST_F(OveruseFrameDetectorTest, MaxIntervalScalesWithFramerate) {
  const int kCapturerMaxFrameRate = 30;
  const int kEncodeMaxFrameRate = 20;  // Maximum fps the encoder can sustain.

  overuse_detector_->SetOptions(options_);
  // Trigger overuse.
  TimeDelta frame_interval_us = TimeDelta::Seconds(1) / kCapturerMaxFrameRate;
  // Processing time just below over use limit given kEncodeMaxFrameRate.
  TimeDelta processing_time_us =
      (98 * OveruseProcessingTimeLimitForFramerate(kEncodeMaxFrameRate)) / 100;
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }

  // Simulate frame rate reduction and normal usage.
  frame_interval_us = TimeDelta::Seconds(1) / kEncodeMaxFrameRate;
  overuse_detector_->OnTargetFramerateUpdated(kEncodeMaxFrameRate);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }

  // Reduce processing time to trigger underuse.
  processing_time_us =
      (98 * UnderuseProcessingTimeLimitForFramerate(kEncodeMaxFrameRate)) / 100;
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(1);
  InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                  processing_time_us);
  overuse_detector_->CheckForOveruse(observer_);
}
#endif

TEST_F(OveruseFrameDetectorTest, RespectsMinFramerate) {
  const int kMinFrameRate = 7;  // Minimum fps allowed by current detector impl.
  overuse_detector_->SetOptions(options_);
  overuse_detector_->OnTargetFramerateUpdated(kMinFrameRate);

  // Normal usage just at the limit.
  TimeDelta frame_interval_us = TimeDelta::Seconds(1) / kMinFrameRate;
  // Processing time just below over use limit given kEncodeMaxFrameRate.
  TimeDelta processing_time_us =
      (98 * OveruseProcessingTimeLimitForFramerate(kMinFrameRate)) / 100;
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }

  // Over the limit to overuse.
  processing_time_us =
      (102 * OveruseProcessingTimeLimitForFramerate(kMinFrameRate)) / 100;
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }

  // Reduce input frame rate. Should still trigger overuse.
  overuse_detector_->OnTargetFramerateUpdated(kMinFrameRate - 1);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest, LimitsMaxFrameInterval) {
  const int kMaxFrameRate = 20;
  overuse_detector_->SetOptions(options_);
  overuse_detector_->OnTargetFramerateUpdated(kMaxFrameRate);
  TimeDelta frame_interval_us = TimeDelta::Seconds(1) / kMaxFrameRate;
  // Maximum frame interval allowed is 35% above ideal.
  TimeDelta max_frame_interval_us = (135 * frame_interval_us) / 100;
  // Maximum processing time, without triggering overuse, allowed with the above
  // frame interval.
  TimeDelta max_processing_time_us =
      (max_frame_interval_us * options_.high_encode_usage_threshold_percent) /
      100;

  // Processing time just below overuse limit given kMaxFrameRate.
  TimeDelta processing_time_us = (98 * max_processing_time_us) / 100;
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, max_frame_interval_us, kWidth,
                                    kHeight, processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }

  // Go above limit, trigger overuse.
  processing_time_us = (102 * max_processing_time_us) / 100;
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, max_frame_interval_us, kWidth,
                                    kHeight, processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }

  // Increase frame interval, should still trigger overuse.
  max_frame_interval_us = max_frame_interval_us * 2;
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, max_frame_interval_us, kWidth,
                                    kHeight, processing_time_us);
    overuse_detector_->CheckForOveruse(observer_);
  }
}

// Models screencast, with irregular arrival of frames which are heavy
// to encode.
TEST_F(OveruseFrameDetectorTest, NoOveruseForLargeRandomFrameInterval) {
  // TODO(bugs.webrtc.org/8504): When new estimator is relanded,
  // behavior is improved in this scenario, with only AdaptUp events,
  // and estimated load closer to the true average.

  // EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  // EXPECT_CALL(mock_observer_, AdaptUp())
  //     .Times(::testing::AtLeast(1));
  overuse_detector_->SetOptions(options_);

  const int kNumFrames = 500;
  const TimeDelta kEncodeTime = TimeDelta::Micros(100);
  const TimeDelta kMinInterval = TimeDelta::Micros(30);
  const TimeDelta kMaxInterval = TimeDelta::Micros(1000);

  const int kTargetFramerate = 5;

  overuse_detector_->OnTargetFramerateUpdated(kTargetFramerate);

  InsertAndSendFramesWithRandomInterval(kNumFrames, kMinInterval, kMaxInterval,
                                        kWidth, kHeight, kEncodeTime);
  // Average usage 19%. Check that estimate is in the right ball park.
  // EXPECT_NEAR(UsagePercent(), 20, 10);
  EXPECT_NEAR(UsagePercent(), 20, 35);
}

// Models screencast, with irregular arrival of frames, often
// exceeding the timeout interval.
TEST_F(OveruseFrameDetectorTest, NoOveruseForRandomFrameIntervalWithReset) {
  // TODO(bugs.webrtc.org/8504): When new estimator is relanded,
  // behavior is improved in this scenario, and we get AdaptUp events.
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  // EXPECT_CALL(mock_observer_, AdaptUp())
  //     .Times(::testing::AtLeast(1));

  const int kNumFrames = 500;
  const TimeDelta kEncodeTime = TimeDelta::Millis(100);
  const TimeDelta kMinInterval = TimeDelta::Millis(30);
  const TimeDelta kMaxInterval = TimeDelta::Millis(3000);

  const int kTargetFramerate = 5;

  overuse_detector_->OnTargetFramerateUpdated(kTargetFramerate);

  InsertAndSendFramesWithRandomInterval(kNumFrames, kMinInterval, kMaxInterval,
                                        kWidth, kHeight, kEncodeTime);

  // Average usage 6.6%, but since the frame_timeout_interval_ms is
  // only 1500 ms, we often reset the estimate to the initial value.
  // Check that estimate is in the right ball park.
  EXPECT_GE(UsagePercent(), 1);
  EXPECT_LE(UsagePercent(), InitialUsage() + 5);
}

// Models simulcast, with multiple encoded frames for each input frame.
// Load estimate should be based on the maximum encode time per input frame.
TEST_F(OveruseFrameDetectorTest, NoOveruseForSimulcast) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);

  constexpr int kNumFrames = 500;
  constexpr TimeDelta kEncodeTimes[] = {
      TimeDelta::Millis(10),
      TimeDelta::Millis(8),
      TimeDelta::Millis(12),
  };
  constexpr TimeDelta kInterval = TimeDelta::Millis(30);

  InsertAndSendSimulcastFramesWithInterval(kNumFrames, kInterval, kWidth,
                                           kHeight, kEncodeTimes);

  // Average usage 40%. 12 ms / 30 ms.
  EXPECT_GE(UsagePercent(), 35);
  EXPECT_LE(UsagePercent(), 45);
}

// Tests using new cpu load estimator
class OveruseFrameDetectorTest2 : public OveruseFrameDetectorTest {
 protected:
  void SetUp() override {
    options_.filter_time_ms = TimeDelta::Seconds(5).ms();
    OveruseFrameDetectorTest::SetUp();
  }

  void InsertAndSendFramesWithInterval(int num_frames,
                                       TimeDelta interval_us,
                                       int width,
                                       int height,
                                       TimeDelta delay_us) override {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(width, height))
            .set_rotation(kVideoRotation_0)
            .set_timestamp_us(0)
            .build();
    while (num_frames-- > 0) {
      int64_t capture_time_us = clock_.TimeInMicroseconds();
      overuse_detector_->FrameCaptured(frame, capture_time_us /* ignored */);
      overuse_detector_->FrameSent(0 /* ignored timestamp */,
                                   0 /* ignored send_time_us */,
                                   capture_time_us, delay_us.us());
      clock_.AdvanceTime(interval_us);
    }
  }

  void InsertAndSendFramesWithRandomInterval(int num_frames,
                                             TimeDelta min_interval_us,
                                             TimeDelta max_interval_us,
                                             int width,
                                             int height,
                                             TimeDelta delay_us) override {
    Random random(17);

    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(width, height))
            .set_rotation(kVideoRotation_0)
            .set_timestamp_us(0)
            .build();
    for (int i = 0; i < num_frames; i++) {
      TimeDelta interval_us = TimeDelta::Micros(random.Rand(
          min_interval_us.us<uint32_t>(), max_interval_us.us<uint32_t>()));
      int64_t capture_time_us = clock_.TimeInMicroseconds();
      overuse_detector_->FrameCaptured(frame, capture_time_us);
      overuse_detector_->FrameSent(0 /* ignored timestamp */,
                                   0 /* ignored send_time_us */,
                                   capture_time_us, delay_us.us());

      overuse_detector_->CheckForOveruse(observer_);
      clock_.AdvanceTime(interval_us);
    }
  }

  void ForceUpdate(int width, int height) override {
    // This is mainly to check initial values and whether the overuse
    // detector has been reset or not.
    InsertAndSendFramesWithInterval(1, TimeDelta::Seconds(1), width, height,
                                    kFrameInterval);
  }
};

// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest2, TriggerOveruse) {
  // usage > high => overuse
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest2, OveruseAndRecover) {
  // usage > high => overuse
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(::testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest2, DoubleOveruseAndRecover) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(::testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest2, TriggerUnderuseWithMinProcessCount) {
  const TimeDelta kProcessInterval = TimeDelta::Seconds(5);
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  observer_ = nullptr;
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1200, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  overuse_detector_->CheckForOveruse(&overuse_observer);
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_.AdvanceTime(kProcessInterval);
  overuse_detector_->CheckForOveruse(&overuse_observer);
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest2, ConstantOveruseGivesNoNormalUsage) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(0);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest2, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(1);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest2, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest2, ProcessingUsage) {
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_EQ(static_cast<int>(kProcessTime * 100 / kFrameInterval),
            UsagePercent());
}

TEST_F(OveruseFrameDetectorTest2, ResetAfterResolutionChange) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset (with new width/height).
  ForceUpdate(kWidth, kHeight + 1);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest2, ResetAfterFrameTimeout) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(1000, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, TimeDelta::Millis(options_.frame_timeout_interval_ms), kWidth, kHeight,
      kProcessTime);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2, TimeDelta::Millis(options_.frame_timeout_interval_ms + 1), kWidth,
      kHeight, kProcessTime);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest2, ConvergesSlowly) {
  overuse_detector_->SetOptions(options_);
  InsertAndSendFramesWithInterval(1, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  // No update for the first sample.
  EXPECT_EQ(InitialUsage(), UsagePercent());

  // Total time approximately 40 * 33ms = 1.3s, significantly less
  // than the 5s time constant.
  InsertAndSendFramesWithInterval(40, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);

  // Should have started to approach correct load of 15%, but not very far.
  EXPECT_LT(UsagePercent(), InitialUsage());
  EXPECT_GT(UsagePercent(), (InitialUsage() * 3 + 8) / 4);

  // Run for roughly 10s more, should now be closer.
  InsertAndSendFramesWithInterval(300, kFrameInterval, kWidth, kHeight,
                                  kProcessTime);
  EXPECT_NEAR(UsagePercent(), 20, 5);
}

TEST_F(OveruseFrameDetectorTest2, InitialProcessingUsage) {
  overuse_detector_->SetOptions(options_);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest2, MeasuresMultipleConcurrentSamples) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(::testing::AtLeast(1));
  static const TimeDelta kInterval = TimeDelta::Millis(33);
  static const size_t kNumFramesEncodingDelay = 3;
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(I420Buffer::Create(kWidth, kHeight))
          .set_rotation(kVideoRotation_0)
          .set_timestamp_us(0)
          .build();
  for (size_t i = 0; i < 1000; ++i) {
    // Unique timestamps.
    frame.set_rtp_timestamp(static_cast<uint32_t>(i));
    int64_t capture_time_us = clock_.TimeInMicroseconds();
    overuse_detector_->FrameCaptured(frame, capture_time_us);
    clock_.AdvanceTime(kInterval);
    if (i > kNumFramesEncodingDelay) {
      overuse_detector_->FrameSent(
          static_cast<uint32_t>(i - kNumFramesEncodingDelay),
          clock_.TimeInMicroseconds(), capture_time_us, kInterval.us());
    }
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest2, UpdatesExistingSamples) {
  // >85% encoding time should trigger overuse.
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(::testing::AtLeast(1));
  static const TimeDelta kInterval = TimeDelta::Millis(33);
  static const TimeDelta kDelay = TimeDelta::Millis(30);
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(I420Buffer::Create(kWidth, kHeight))
          .set_rotation(kVideoRotation_0)
          .set_timestamp_us(0)
          .build();
  uint32_t timestamp = 0;
  for (size_t i = 0; i < 1000; ++i) {
    frame.set_rtp_timestamp(timestamp);
    int64_t capture_time_us = clock_.TimeInMicroseconds();
    overuse_detector_->FrameCaptured(frame, capture_time_us);
    // Encode and send first parts almost instantly.
    clock_.AdvanceTime(TimeDelta::Millis(1));
    overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                 capture_time_us, TimeDelta::Millis(1).us());
    // Encode heavier part, resulting in >85% usage total.
    clock_.AdvanceTime(kDelay - TimeDelta::Millis(1));
    overuse_detector_->FrameSent(timestamp, clock_.TimeInMicroseconds(),
                                 capture_time_us, kDelay.us());
    clock_.AdvanceTime(kInterval - kDelay);
    timestamp += kInterval.us() * 90 / 1000;
    overuse_detector_->CheckForOveruse(observer_);
  }
}

TEST_F(OveruseFrameDetectorTest2, RunOnTqNormalUsage) {
  TaskQueueForTest queue("OveruseFrameDetectorTestQueue");

  queue.SendTask([&] {
    overuse_detector_->StartCheckForOveruse(queue.Get(), options_, observer_);
  });

  Event event;
  // Expect NormalUsage(). When called, stop the `overuse_detector_` and then
  // set `event` to end the test.
  EXPECT_CALL(mock_observer_, AdaptUp())
      .WillOnce(InvokeWithoutArgs([this, &event] {
        overuse_detector_->StopCheckForOveruse();
        event.Set();
      }));

  queue.PostTask([this] {
    const TimeDelta kDelayUs1 = TimeDelta::Millis(5);
    const TimeDelta kDelayUs2 = TimeDelta::Millis(6);
    InsertAndSendFramesWithInterval(1300, kFrameInterval, kWidth, kHeight,
                                    kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameInterval, kWidth, kHeight,
                                    kDelayUs2);
  });

  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(10)));
}

// Models screencast, with irregular arrival of frames which are heavy
// to encode.
TEST_F(OveruseFrameDetectorTest2, NoOveruseForLargeRandomFrameInterval) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(::testing::AtLeast(1));

  const int kNumFrames = 500;
  const TimeDelta kEncodeTime = TimeDelta::Millis(100);
  const TimeDelta kMinInterval = TimeDelta::Millis(30);
  const TimeDelta kMaxInterval = TimeDelta::Millis(1000);

  InsertAndSendFramesWithRandomInterval(kNumFrames, kMinInterval, kMaxInterval,
                                        kWidth, kHeight, kEncodeTime);
  // Average usage 19%. Check that estimate is in the right ball park.
  EXPECT_NEAR(UsagePercent(), 20, 10);
}

// Models screencast, with irregular arrival of frames, often
// exceeding the timeout interval.
TEST_F(OveruseFrameDetectorTest2, NoOveruseForRandomFrameIntervalWithReset) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);
  EXPECT_CALL(mock_observer_, AdaptUp()).Times(::testing::AtLeast(1));

  const int kNumFrames = 500;
  const TimeDelta kEncodeTime = TimeDelta::Millis(100);
  const TimeDelta kMinInterval = TimeDelta::Millis(30);
  const TimeDelta kMaxInterval = TimeDelta::Millis(3000);

  InsertAndSendFramesWithRandomInterval(kNumFrames, kMinInterval, kMaxInterval,
                                        kWidth, kHeight, kEncodeTime);

  // Average usage 6.6%, but since the frame_timeout_interval_ms is
  // only 1500 ms, we often reset the estimate to the initial value.
  // Check that estimate is in the right ball park.
  EXPECT_GE(UsagePercent(), 1);
  EXPECT_LE(UsagePercent(), InitialUsage() + 5);
}

TEST_F(OveruseFrameDetectorTest2, ToleratesOutOfOrderFrames) {
  overuse_detector_->SetOptions(options_);
  // Represents a cpu utilization close to 100%. First input frame results in
  // three encoded frames, and the last of those isn't finished until after the
  // first encoded frame corresponding to the next input frame.
  const TimeDelta kEncodeTime = TimeDelta::Millis(30);
  const Timestamp kCaptureTimes[] = {
      Timestamp::Millis(33), Timestamp::Millis(33), Timestamp::Millis(66),
      Timestamp::Millis(33)};

  for (Timestamp capture_time : kCaptureTimes) {
    overuse_detector_->FrameSent(0, 0, capture_time.ms(), kEncodeTime.us());
  }
  EXPECT_GE(UsagePercent(), InitialUsage());
}

// Models simulcast, with multiple encoded frames for each input frame.
// Load estimate should be based on the maximum encode time per input frame.
TEST_F(OveruseFrameDetectorTest2, NoOveruseForSimulcast) {
  overuse_detector_->SetOptions(options_);
  EXPECT_CALL(mock_observer_, AdaptDown()).Times(0);

  constexpr int kNumFrames = 500;
  constexpr TimeDelta kEncodeTimes[] = {
      TimeDelta::Millis(10),
      TimeDelta::Millis(8),
      TimeDelta::Millis(12),
  };
  constexpr TimeDelta kInterval = TimeDelta::Millis(30);

  InsertAndSendSimulcastFramesWithInterval(kNumFrames, kInterval, kWidth,
                                           kHeight, kEncodeTimes);

  // Average usage 40%. 12 ms / 30 ms.
  EXPECT_GE(UsagePercent(), 35);
  EXPECT_LE(UsagePercent(), 45);
}

}  // namespace
}  // namespace webrtc
