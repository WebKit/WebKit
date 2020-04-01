/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/frame_rate_estimator.h"

#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr TimeDelta kDefaultWindow = TimeDelta::Millis(1000);
}

class FrameRateEstimatorTest : public ::testing::Test {
 public:
  FrameRateEstimatorTest() : clock_(123), estimator_(kDefaultWindow) {}

 protected:
  SimulatedClock clock_;
  FrameRateEstimator estimator_;
};

TEST_F(FrameRateEstimatorTest, NoEstimateWithLessThanTwoFrames) {
  EXPECT_FALSE(estimator_.GetAverageFps());
  estimator_.OnFrame(clock_.CurrentTime());
  EXPECT_FALSE(estimator_.GetAverageFps());
  clock_.AdvanceTime(TimeDelta::Millis(33));
  EXPECT_FALSE(estimator_.GetAverageFps());
}

TEST_F(FrameRateEstimatorTest, NoEstimateWithZeroSpan) {
  // Two frames, but they are spanning 0ms so can't estimate frame rate.
  estimator_.OnFrame(clock_.CurrentTime());
  estimator_.OnFrame(clock_.CurrentTime());
  EXPECT_FALSE(estimator_.GetAverageFps());
}

TEST_F(FrameRateEstimatorTest, SingleSpanFps) {
  const double kExpectedFps = 30.0;
  estimator_.OnFrame(clock_.CurrentTime());
  clock_.AdvanceTime(TimeDelta::Seconds(1) / kExpectedFps);
  estimator_.OnFrame(clock_.CurrentTime());
  EXPECT_NEAR(*estimator_.GetAverageFps(), kExpectedFps, 0.001);
}

TEST_F(FrameRateEstimatorTest, AverageFps) {
  // Insert frames a intervals corresponding to 10fps for half the window, then
  // 40fps half the window. The average should be 20fps.
  const double kLowFps = 10.0;
  const double kHighFps = 30.0;
  const double kExpectedFps = 20.0;

  const Timestamp start_time = clock_.CurrentTime();
  while (clock_.CurrentTime() - start_time < kDefaultWindow / 2) {
    estimator_.OnFrame(clock_.CurrentTime());
    clock_.AdvanceTime(TimeDelta::Seconds(1) / kLowFps);
  }
  while (clock_.CurrentTime() - start_time < kDefaultWindow) {
    estimator_.OnFrame(clock_.CurrentTime());
    clock_.AdvanceTime(TimeDelta::Seconds(1) / kHighFps);
  }

  EXPECT_NEAR(*estimator_.GetAverageFps(), kExpectedFps, 0.001);
}

TEST_F(FrameRateEstimatorTest, CullsOldFramesFromAveragingWindow) {
  // Two frames, just on the border of the 1s window => 1 fps.
  estimator_.OnFrame(clock_.CurrentTime());
  clock_.AdvanceTime(kDefaultWindow);
  estimator_.OnFrame(clock_.CurrentTime());
  EXPECT_TRUE(estimator_.GetAverageFps());
  EXPECT_NEAR(*estimator_.GetAverageFps(), 1.0, 0.001);

  // Oldest frame should just be pushed out the window, leaving a single frame
  // => no estimate possible.
  clock_.AdvanceTime(TimeDelta::Micros(1));
  EXPECT_FALSE(estimator_.GetAverageFps(clock_.CurrentTime()));
}

TEST_F(FrameRateEstimatorTest, Reset) {
  estimator_.OnFrame(clock_.CurrentTime());
  clock_.AdvanceTime(TimeDelta::Seconds(1) / 30);
  estimator_.OnFrame(clock_.CurrentTime());
  EXPECT_TRUE(estimator_.GetAverageFps());

  // Clear estimator, no estimate should be possible even after inserting one
  // new frame.
  estimator_.Reset();
  EXPECT_FALSE(estimator_.GetAverageFps());
  clock_.AdvanceTime(TimeDelta::Seconds(1) / 30);
  estimator_.OnFrame(clock_.CurrentTime());
  EXPECT_FALSE(estimator_.GetAverageFps());
}

}  // namespace webrtc
