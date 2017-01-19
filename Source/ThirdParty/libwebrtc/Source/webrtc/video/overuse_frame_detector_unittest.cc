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

#include "webrtc/base/event.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video_frame.h"

namespace webrtc {

using ::testing::Invoke;

namespace {
  const int kWidth = 640;
  const int kHeight = 480;
  const int kFrameInterval33ms = 33;
  const int kProcessIntervalMs = 5000;
  const int kProcessTime5ms = 5;
}  // namespace

class MockCpuOveruseObserver : public CpuOveruseObserver {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD0(OveruseDetected, void());
  MOCK_METHOD0(NormalUsage, void());
};

class CpuOveruseObserverImpl : public CpuOveruseObserver {
 public:
  CpuOveruseObserverImpl() :
    overuse_(0),
    normaluse_(0) {}
  virtual ~CpuOveruseObserverImpl() {}

  void OveruseDetected() { ++overuse_; }
  void NormalUsage() { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorUnderTest : public OveruseFrameDetector {
 public:
  OveruseFrameDetectorUnderTest(Clock* clock,
                                const CpuOveruseOptions& options,
                                CpuOveruseObserver* overuse_observer,
                                EncodedFrameObserver* encoder_timing,
                                CpuOveruseMetricsObserver* metrics_observer)
      : OveruseFrameDetector(clock,
                             options,
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
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockCpuOveruseObserver());
    options_.min_process_count = 0;
    ReinitializeOveruseDetector();
  }

  void ReinitializeOveruseDetector() {
    overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
        clock_.get(), options_, observer_.get(), nullptr, this));
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
                                       int interval_ms,
                                       int width,
                                       int height,
                                       int delay_ms) {
    VideoFrame frame(I420Buffer::Create(width, height),
                     webrtc::kVideoRotation_0, 0);
    uint32_t timestamp = 0;
    while (num_frames-- > 0) {
      frame.set_timestamp(timestamp);
      overuse_detector_->FrameCaptured(frame, clock_->TimeInMilliseconds());
      clock_->AdvanceTimeMilliseconds(delay_ms);
      overuse_detector_->FrameSent(timestamp, clock_->TimeInMilliseconds());
      clock_->AdvanceTimeMilliseconds(interval_ms - delay_ms);
      timestamp += interval_ms * 90;
    }
  }

  void ForceUpdate(int width, int height) {
    // Insert one frame, wait a second and then put in another to force update
    // the usage. From the tests where these are used, adding another sample
    // doesn't affect the expected outcome (this is mainly to check initial
    // values and whether the overuse detector has been reset or not).
    InsertAndSendFramesWithInterval(2, 1000, width, height, kFrameInterval33ms);
  }
  void TriggerOveruse(int num_times) {
    const int kDelayMs = 32;
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(
          1000, kFrameInterval33ms, kWidth, kHeight, kDelayMs);
      overuse_detector_->CheckForOveruse();
    }
  }

  void TriggerUnderuse() {
    const int kDelayMs1 = 5;
    const int kDelayMs2 = 6;
    InsertAndSendFramesWithInterval(
        1300, kFrameInterval33ms, kWidth, kHeight, kDelayMs1);
    InsertAndSendFramesWithInterval(
        1, kFrameInterval33ms, kWidth, kHeight, kDelayMs2);
    overuse_detector_->CheckForOveruse();
  }

  int UsagePercent() { return metrics_.encode_usage_percent; }

  CpuOveruseOptions options_;
  std::unique_ptr<SimulatedClock> clock_;
  std::unique_ptr<MockCpuOveruseObserver> observer_;
  std::unique_ptr<OveruseFrameDetectorUnderTest> overuse_detector_;
  CpuOveruseMetrics metrics_;
};


// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithNoObserver) {
  overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
      clock_.get(), options_, nullptr, nullptr, this));
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, TriggerUnderuseWithMinProcessCount) {
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  overuse_detector_.reset(new OveruseFrameDetectorUnderTest(
      clock_.get(), options_, &overuse_observer, nullptr, this));
  InsertAndSendFramesWithInterval(
      1200, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  overuse_detector_->CheckForOveruse();
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_->AdvanceTimeMilliseconds(kProcessIntervalMs);
  overuse_detector_->CheckForOveruse();
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_EQ(kProcessTime5ms * 100 / kFrameInterval33ms, UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterResolutionChange) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset (with new width/height).
  ForceUpdate(kWidth, kHeight + 1);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterFrameTimeout) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms + 1, kWidth, kHeight,
      kProcessTime5ms);
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MinFrameSamplesBeforeUpdating) {
  options_.min_frame_samples = 40;
  ReinitializeOveruseDetector();
  InsertAndSendFramesWithInterval(
      40, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  // Pass time far enough to digest all previous samples.
  clock_->AdvanceTimeMilliseconds(1000);
  InsertAndSendFramesWithInterval(1, kFrameInterval33ms, kWidth, kHeight,
                                  kProcessTime5ms);
  // The last sample has not been processed here.
  EXPECT_EQ(InitialUsage(), UsagePercent());

  // Pass time far enough to digest all previous samples, 41 in total.
  clock_->AdvanceTimeMilliseconds(1000);
  InsertAndSendFramesWithInterval(
      1, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  ForceUpdate(kWidth, kHeight);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MeasuresMultipleConcurrentSamples) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(testing::AtLeast(1));
  static const int kIntervalMs = 33;
  static const size_t kNumFramesEncodingDelay = 3;
  VideoFrame frame(I420Buffer::Create(kWidth, kHeight),
                   webrtc::kVideoRotation_0, 0);
  for (size_t i = 0; i < 1000; ++i) {
    // Unique timestamps.
    frame.set_timestamp(static_cast<uint32_t>(i));
    overuse_detector_->FrameCaptured(frame, clock_->TimeInMilliseconds());
    clock_->AdvanceTimeMilliseconds(kIntervalMs);
    if (i > kNumFramesEncodingDelay) {
      overuse_detector_->FrameSent(
          static_cast<uint32_t>(i - kNumFramesEncodingDelay),
          clock_->TimeInMilliseconds());
    }
    overuse_detector_->CheckForOveruse();
  }
}

TEST_F(OveruseFrameDetectorTest, UpdatesExistingSamples) {
  // >85% encoding time should trigger overuse.
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(testing::AtLeast(1));
  static const int kIntervalMs = 33;
  static const int kDelayMs = 30;
  VideoFrame frame(I420Buffer::Create(kWidth, kHeight),
                   webrtc::kVideoRotation_0, 0);
  uint32_t timestamp = 0;
  for (size_t i = 0; i < 1000; ++i) {
    frame.set_timestamp(timestamp);
    overuse_detector_->FrameCaptured(frame, clock_->TimeInMilliseconds());
    // Encode and send first parts almost instantly.
    clock_->AdvanceTimeMilliseconds(1);
    overuse_detector_->FrameSent(timestamp, clock_->TimeInMilliseconds());
    // Encode heavier part, resulting in >85% usage total.
    clock_->AdvanceTimeMilliseconds(kDelayMs - 1);
    overuse_detector_->FrameSent(timestamp, clock_->TimeInMilliseconds());
    clock_->AdvanceTimeMilliseconds(kIntervalMs - kDelayMs);
    timestamp += kIntervalMs * 90;
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
  EXPECT_CALL(*(observer_.get()), NormalUsage())
      .WillOnce(Invoke([this, &event] {
        overuse_detector_->StopCheckForOveruse();
        event.Set();
      }));

  queue.PostTask([this, &event] {
    const int kDelayMs1 = 5;
    const int kDelayMs2 = 6;
    InsertAndSendFramesWithInterval(1300, kFrameInterval33ms, kWidth, kHeight,
                                    kDelayMs1);
    InsertAndSendFramesWithInterval(1, kFrameInterval33ms, kWidth, kHeight,
                                    kDelayMs2);
  });

  EXPECT_TRUE(event.Wait(10000));
}

}  // namespace webrtc
