/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/framerate_controller.h"

#include <cstdint>

#include "rtc_base/time_utils.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kInputFps = 30;
constexpr int kNumFrames = 60;
}  // namespace

class FramerateControllerTest : public ::testing::Test {
 protected:
  int64_t GetNextTimestampNs() {
    int64_t interval_us = kNumMicrosecsPerSec / kInputFps;
    next_timestamp_us_ += interval_us;
    return next_timestamp_us_ * kNumNanosecsPerMicrosec;
  }

  int64_t next_timestamp_us_ = 6543210;
  FramerateController controller_;
};

TEST_F(FramerateControllerTest, NoFramesDroppedIfNothingRequested) {
  // Default max framerate is maxdouble.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, AllFramesDroppedIfZeroRequested) {
  controller_.SetMaxFramerate(0);

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_TRUE(controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, AllFramesDroppedIfNegativeRequested) {
  controller_.SetMaxFramerate(-1);

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_TRUE(controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, EverySecondFrameDroppedIfHalfRequested) {
  controller_.SetMaxFramerate(kInputFps / 2);

  // The first frame should not be dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_EQ(i % 2 == 0, controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, EveryThirdFrameDroppedIfTwoThirdsRequested) {
  controller_.SetMaxFramerate(kInputFps * 2 / 3);

  // The first frame should not be dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_EQ(i % 3 == 0, controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, NoFrameDroppedIfTwiceRequested) {
  controller_.SetMaxFramerate(kInputFps * 2);

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, TestAverageFramerate) {
  const double kMaxFps = 18.2;
  controller_.SetMaxFramerate(kMaxFps);

  const int kNumSec = 10;
  int frames_kept = 0;
  for (int i = 0; i < kInputFps * kNumSec; ++i) {
    if (!controller_.ShouldDropFrame(GetNextTimestampNs()))
      ++frames_kept;
  }
  double average_fps = static_cast<double>(frames_kept) / kNumSec;
  EXPECT_NEAR(kMaxFps, average_fps, 0.01);
}

TEST_F(FramerateControllerTest, NoFrameDroppedForLargeTimestampOffset) {
  controller_.SetMaxFramerate(kInputFps);
  EXPECT_FALSE(controller_.ShouldDropFrame(0));

  const int64_t kLargeOffsetNs = -987654321LL * 1000;
  EXPECT_FALSE(controller_.ShouldDropFrame(kLargeOffsetNs));

  int64_t input_interval_ns = kNumNanosecsPerSec / kInputFps;
  EXPECT_FALSE(controller_.ShouldDropFrame(kLargeOffsetNs + input_interval_ns));
}

TEST_F(FramerateControllerTest, NoFrameDroppedIfInputWithJitterRequested) {
  controller_.SetMaxFramerate(kInputFps);

  // Input fps with jitter.
  int64_t input_interval_ns = kNumNanosecsPerSec / kInputFps;
  EXPECT_FALSE(controller_.ShouldDropFrame(input_interval_ns * 0 / 10));
  EXPECT_FALSE(controller_.ShouldDropFrame(input_interval_ns * 10 / 10 - 1));
  EXPECT_FALSE(controller_.ShouldDropFrame(input_interval_ns * 25 / 10));
  EXPECT_FALSE(controller_.ShouldDropFrame(input_interval_ns * 30 / 10));
  EXPECT_FALSE(controller_.ShouldDropFrame(input_interval_ns * 35 / 10));
  EXPECT_FALSE(controller_.ShouldDropFrame(input_interval_ns * 50 / 10));
}

TEST_F(FramerateControllerTest, FrameDroppedWhenReductionRequested) {
  controller_.SetMaxFramerate(kInputFps);

  // Expect no frame drop.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.ShouldDropFrame(GetNextTimestampNs()));

  // Reduce max frame rate.
  controller_.SetMaxFramerate(kInputFps / 2);

  // Verify that every other frame is dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_EQ(i % 2 == 0, controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, NoFramesDroppedAfterReset) {
  controller_.SetMaxFramerate(0);

  // All frames dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_TRUE(controller_.ShouldDropFrame(GetNextTimestampNs()));

  controller_.Reset();

  // Expect no frame drop after reset.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.ShouldDropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, TestKeepFrame) {
  FramerateController controller(kInputFps / 2);

  EXPECT_FALSE(controller.ShouldDropFrame(GetNextTimestampNs()));
  EXPECT_TRUE(controller.ShouldDropFrame(GetNextTimestampNs()));
  EXPECT_FALSE(controller.ShouldDropFrame(GetNextTimestampNs()));
  EXPECT_TRUE(controller.ShouldDropFrame(GetNextTimestampNs()));
  EXPECT_FALSE(controller.ShouldDropFrame(GetNextTimestampNs()));

  // Next frame should be dropped.
  // Keep this frame (e.g. in case of a key frame).
  controller.KeepFrame(GetNextTimestampNs());
  // Expect next frame to be dropped instead.
  EXPECT_TRUE(controller.ShouldDropFrame(GetNextTimestampNs()));
}

}  // namespace webrtc
