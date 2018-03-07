/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/event.h"
#include "rtc_base/fakeclock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/overuse_frame_detector.h"

namespace webrtc {

using ::testing::InvokeWithoutArgs;

namespace {
  const int kWidth = 640;
  const int kHeight = 480;
  const int kFrameIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
  const int kProcessTimeUs = 5 * rtc::kNumMicrosecsPerMillisec;
}  // namespace

class MockCpuOveruseObserver : public AdaptationObserverInterface {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD1(AdaptUp, void(AdaptReason));
  MOCK_METHOD1(AdaptDown, void(AdaptReason));
};

class CpuOveruseObserverImpl : public AdaptationObserverInterface {
 public:
  CpuOveruseObserverImpl() :
    overuse_(0),
    normaluse_(0) {}
  virtual ~CpuOveruseObserverImpl() {}

  void AdaptDown(AdaptReason) { ++overuse_; }
  void AdaptUp(AdaptReason) { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorUnderTest : public OveruseFrameDetector {
 public:
  OveruseFrameDetectorUnderTest(const CpuOveruseOptions& options,
                                AdaptationObserverInterface* overuse_observer,
                                EncodedFrameObserver* encoder_timing,
                                CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(options,
                             overuse_observer,
                             encoder_timing,
                             metrics_observer) {}
  ~OveruseFrameDetectorUnderTest() {}

  using OveruseFrameDetector::CheckForOveruse;
};

class OveruseFrameDetectorTest : public ::testing::Test,
                                 public CpuOveruseMetricsObserver {
 protected:
  void SetUp() override {
    observer_.reset(new MockCpuOveruseObserver());
    options_.min_process_count = 0;
    ReinitializeOveruseDetector();
  }

  void ReinitializeOveruseDetector() {
    overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
        options_, observer_.get(), nullptr, this));
  }

  void OnEncodedFrameTimeMeasured(int encode_time_ms,
                                  const CpuOveruseMetrics& metrics) override {
    metrics_ = metrics;
  }

  int InitialUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) / 2.0f) + 0.5;
  }

  void InsertAndSendFramesWithInterval(int num_frames,
                                       int interval_us,
                                       int width,
                                       int height,
                                       int delay_us) {
    VideoFrame frame(I420Buffer::Create(width, height),
                     webrtc::kVideoRotation_0, 0);
    uint32_t timestamp = 0;
    while (num_frames-- > 0) {
      frame.set_timestamp(timestamp);
      overuse_detector_->FrameCaptured(frame, rtc::TimeMicros());
      clock_.AdvanceTimeMicros(delay_us);
      overuse_detector_->FrameSent(timestamp, rtc::TimeMicros());
      clock_.AdvanceTimeMicros(interval_us - delay_us);
      timestamp += interval_us * 90 / 1000;
    }
  }

  void ForceUpdate(int width, int height) {
    // Insert one frame, wait a second and then put in another to force update
    // the usage. From the tests where these are used, adding another sample
    // doesn't affect the expected outcome (this is mainly to check initial
    // values and whether the overuse detector has been reset or not).
    InsertAndSendFramesWithInterval(2, rtc::kNumMicrosecsPerSec,
                                    width, height, kFrameIntervalUs);
  }
  void TriggerOveruse(int num_times) {
    const int kDelayUs = 32 * rtc::kNumMicrosecsPerMillisec;
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(
          1000, kFrameIntervalUs, kWidth, kHeight, kDelayUs);
      overuse_detector_->CheckForOveruse();
    }
  }

  void TriggerUnderuse() {
    const int kDelayUs1 = 5000;
    const int kDelayUs2 = 6000;
    InsertAndSendFramesWithInterval(
        1300, kFrameIntervalUs, kWidth, kHeight, kDelayUs1);
    InsertAndSendFramesWithInterval(
        1, kFrameIntervalUs, kWidth, kHeight, kDelayUs2);
    overuse_detector_->CheckForOveruse();
  }

  int UsagePercent() { return metrics_.encode_usage_percent; }

  int64_t OveruseProcessingTimeLimitForFramerate(int fps) const {
    int64_t frame_interval = rtc::kNumMicrosecsPerSec / fps;
    int64_t max_processing_time_us =
        (frame_interval * options_.high_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  int64_t UnderuseProcessingTimeLimitForFramerate(int fps) const {
    int64_t frame_interval = rtc::kNumMicrosecsPerSec / fps;
    int64_t max_processing_time_us =
        (frame_interval * options_.low_encode_usage_threshold_percent) / 100;
    return max_processing_time_us;
  }

  CpuOveruseOptions options_;
  rtc::ScopedFakeClock clock_;
  std::unique_ptr<MockCpuOveruseObserver> observer_;
  std::unique_ptr<OveruseFrameDetectorUnderTest> overuse_detector_;
  CpuOveruseMetrics metrics_;

  static const auto reason_ = AdaptationObserverInterface::AdaptReason::kCpu;
};


// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithNoObserver) {
  overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
      options_, nullptr, nullptr, this));
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(0);
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, TriggerUnderuseWithMinProcessCount) {
  const int kProcessIntervalUs = 5 * rtc::kNumMicrosecsPerSec;
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
      options_, &overuse_observer, nullptr, this));
  InsertAndSendFramesWithInterval(
      1200, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  overuse_detector_->CheckForOveruse();
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_.AdvanceTimeMicros(kProcessIntervalUs);
  overuse_detector_->CheckForOveruse();
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(0);
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  InsertAndSendFramesWithInterval(
      1000, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_EQ(kProcessTimeUs * 100 / kFrameIntervalUs, UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterResolutionChange) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset (with new width/height).
  ForceUpdate(kWidth, kHeight + 1);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterFrameTimeout) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms *
      rtc::kNumMicrosecsPerMillisec, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2, (options_.frame_timeout_interval_ms + 1) *
      rtc::kNumMicrosecsPerMillisec, kWidth, kHeight, kProcessTimeUs);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MinFrameSamplesBeforeUpdating) {
  options_.min_frame_samples = 40;
  ReinitializeOveruseDetector();
  InsertAndSendFramesWithInterval(
      40, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  // Pass time far enough to digest all previous samples.
  clock_.AdvanceTimeMicros(rtc::kNumMicrosecsPerSec);
  InsertAndSendFramesWithInterval(1, kFrameIntervalUs, kWidth, kHeight,
                                  kProcessTimeUs);
  // The last sample has not been processed here.
  EXPECT_EQ(InitialUsage(), UsagePercent());

  // Pass time far enough to digest all previous samples, 41 in total.
  clock_.AdvanceTimeMicros(rtc::kNumMicrosecsPerSec);
  InsertAndSendFramesWithInterval(
      1, kFrameIntervalUs, kWidth, kHeight, kProcessTimeUs);
  EXPECT_NE(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MeasuresMultipleConcurrentSamples) {
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_))
      .Times(testing::AtLeast(1));
  static const int kIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
  static const size_t kNumFramesEncodingDelay = 3;
  VideoFrame frame(I420Buffer::Create(kWidth, kHeight),
                   webrtc::kVideoRotation_0, 0);
  for (size_t i = 0; i < 1000; ++i) {
    // Unique timestamps.
    frame.set_timestamp(static_cast<uint32_t>(i));
    overuse_detector_->FrameCaptured(frame, rtc::TimeMicros());
    clock_.AdvanceTimeMicros(kIntervalUs);
    if (i > kNumFramesEncodingDelay) {
      overuse_detector_->FrameSent(
          static_cast<uint32_t>(i - kNumFramesEncodingDelay),
          rtc::TimeMicros());
    }
    overuse_detector_->CheckForOveruse();
  }
}

TEST_F(OveruseFrameDetectorTest, UpdatesExistingSamples) {
  // >85% encoding time should trigger overuse.
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_))
      .Times(testing::AtLeast(1));
  static const int kIntervalUs = 33 * rtc::kNumMicrosecsPerMillisec;
  static const int kDelayUs = 30 * rtc::kNumMicrosecsPerMillisec;
  VideoFrame frame(I420Buffer::Create(kWidth, kHeight),
                   webrtc::kVideoRotation_0, 0);
  uint32_t timestamp = 0;
  for (size_t i = 0; i < 1000; ++i) {
    frame.set_timestamp(timestamp);
    overuse_detector_->FrameCaptured(frame, rtc::TimeMicros());
    // Encode and send first parts almost instantly.
    clock_.AdvanceTimeMicros(rtc::kNumMicrosecsPerMillisec);
    overuse_detector_->FrameSent(timestamp, rtc::TimeMicros());
    // Encode heavier part, resulting in >85% usage total.
    clock_.AdvanceTimeMicros(kDelayUs - rtc::kNumMicrosecsPerMillisec);
    overuse_detector_->FrameSent(timestamp, rtc::TimeMicros());
    clock_.AdvanceTimeMicros(kIntervalUs - kDelayUs);
    timestamp += kIntervalUs * 90 / 1000;
    overuse_detector_->CheckForOveruse();
  }
}

TEST_F(OveruseFrameDetectorTest, RunOnTqNormalUsage) {
  rtc::TaskQueue queue("OveruseFrameDetectorTestQueue");

  rtc::Event event(false, false);
  queue.PostTask([this, &event] {
    overuse_detector_->StartCheckForOveruse();
    event.Set();
  });
  event.Wait(rtc::Event::kForever);

  // Expect NormalUsage(). When called, stop the |overuse_detector_| and then
  // set |event| to end the test.
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_))
      .WillOnce(InvokeWithoutArgs([this, &event] {
        overuse_detector_->StopCheckForOveruse();
        event.Set();
      }));

  queue.PostTask([this] {
    const int kDelayUs1 = 5 * rtc::kNumMicrosecsPerMillisec;
    const int kDelayUs2 = 6 * rtc::kNumMicrosecsPerMillisec;
    InsertAndSendFramesWithInterval(1300, kFrameIntervalUs, kWidth, kHeight,
                                    kDelayUs1);
    InsertAndSendFramesWithInterval(1, kFrameIntervalUs, kWidth, kHeight,
                                    kDelayUs2);
  });

  EXPECT_TRUE(event.Wait(10000));
}

TEST_F(OveruseFrameDetectorTest, MaxIntervalScalesWithFramerate) {
  const int kCapturerMaxFrameRate = 30;
  const int kEncodeMaxFrameRate = 20;  // Maximum fps the encoder can sustain.

  // Trigger overuse.
  int64_t frame_interval_us = rtc::kNumMicrosecsPerSec / kCapturerMaxFrameRate;
  // Processing time just below over use limit given kEncodeMaxFrameRate.
  int64_t processing_time_us =
      (98 * OveruseProcessingTimeLimitForFramerate(kEncodeMaxFrameRate)) / 100;
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse();
  }

  // Simulate frame rate reduction and normal usage.
  frame_interval_us = rtc::kNumMicrosecsPerSec / kEncodeMaxFrameRate;
  overuse_detector_->OnTargetFramerateUpdated(kEncodeMaxFrameRate);
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse();
  }

  // Reduce processing time to trigger underuse.
  processing_time_us =
      (98 * UnderuseProcessingTimeLimitForFramerate(kEncodeMaxFrameRate)) / 100;
  EXPECT_CALL(*(observer_.get()), AdaptUp(reason_)).Times(1);
  InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                  processing_time_us);
  overuse_detector_->CheckForOveruse();
}

TEST_F(OveruseFrameDetectorTest, RespectsMinFramerate) {
  const int kMinFrameRate = 7;  // Minimum fps allowed by current detector impl.
  overuse_detector_->OnTargetFramerateUpdated(kMinFrameRate);

  // Normal usage just at the limit.
  int64_t frame_interval_us = rtc::kNumMicrosecsPerSec / kMinFrameRate;
  // Processing time just below over use limit given kEncodeMaxFrameRate.
  int64_t processing_time_us =
      (98 * OveruseProcessingTimeLimitForFramerate(kMinFrameRate)) / 100;
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse();
  }

  // Over the limit to overuse.
  processing_time_us =
      (102 * OveruseProcessingTimeLimitForFramerate(kMinFrameRate)) / 100;
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse();
  }

  // Reduce input frame rate. Should still trigger overuse.
  overuse_detector_->OnTargetFramerateUpdated(kMinFrameRate - 1);
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, frame_interval_us, kWidth, kHeight,
                                    processing_time_us);
    overuse_detector_->CheckForOveruse();
  }
}

TEST_F(OveruseFrameDetectorTest, LimitsMaxFrameInterval) {
  const int kMaxFrameRate = 20;
  overuse_detector_->OnTargetFramerateUpdated(kMaxFrameRate);
  int64_t frame_interval_us = rtc::kNumMicrosecsPerSec / kMaxFrameRate;
  // Maximum frame interval allowed is 35% above ideal.
  int64_t max_frame_interval_us = (135 * frame_interval_us) / 100;
  // Maximum processing time, without triggering overuse, allowed with the above
  // frame interval.
  int64_t max_processing_time_us =
      (max_frame_interval_us * options_.high_encode_usage_threshold_percent) /
      100;

  // Processing time just below overuse limit given kMaxFrameRate.
  int64_t processing_time_us = (98 * max_processing_time_us) / 100;
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(0);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, max_frame_interval_us, kWidth,
                                    kHeight, processing_time_us);
    overuse_detector_->CheckForOveruse();
  }

  // Go above limit, trigger overuse.
  processing_time_us = (102 * max_processing_time_us) / 100;
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, max_frame_interval_us, kWidth,
                                    kHeight, processing_time_us);
    overuse_detector_->CheckForOveruse();
  }

  // Increase frame interval, should still trigger overuse.
  max_frame_interval_us *= 2;
  EXPECT_CALL(*(observer_.get()), AdaptDown(reason_)).Times(1);
  for (int i = 0; i < options_.high_threshold_consecutive_count; ++i) {
    InsertAndSendFramesWithInterval(1200, max_frame_interval_us, kWidth,
                                    kHeight, processing_time_us);
    overuse_detector_->CheckForOveruse();
  }
}

}  // namespace webrtc
