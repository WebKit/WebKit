/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/frame_dropper.h"

#include "webrtc/base/logging.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

const float kTargetBitRateKbps = 300;
const float kIncomingFrameRate = 30;
const size_t kFrameSizeBytes = 1250;

const size_t kLargeFrameSizeBytes = 25000;

const bool kIncludeKeyFrame = true;
const bool kDoNotIncludeKeyFrame = false;

}  // namespace

class FrameDropperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    frame_dropper_.SetRates(kTargetBitRateKbps, kIncomingFrameRate);
  }

  void OverflowLeakyBucket() {
    // Overflow bucket in frame dropper.
    for (int i = 0; i < kIncomingFrameRate; ++i) {
      frame_dropper_.Fill(kFrameSizeBytes, true);
    }
    frame_dropper_.Leak(kIncomingFrameRate);
  }

  void ValidateNoDropsAtTargetBitrate(int large_frame_size_bytes,
                                      int large_frame_rate,
                                      bool is_large_frame_delta) {
    // Smaller frame size is computed to meet |kTargetBitRateKbps|.
    int small_frame_size_bytes =
        kFrameSizeBytes -
        (large_frame_size_bytes * large_frame_rate) / kIncomingFrameRate;

    for (int i = 1; i <= 5 * large_frame_rate; ++i) {
      // Large frame. First frame is always a key frame.
      frame_dropper_.Fill(large_frame_size_bytes,
                          (i == 1) ? false : is_large_frame_delta);
      frame_dropper_.Leak(kIncomingFrameRate);
      EXPECT_FALSE(frame_dropper_.DropFrame());

      // Smaller frames.
      for (int j = 1; j < kIncomingFrameRate / large_frame_rate; ++j) {
        frame_dropper_.Fill(small_frame_size_bytes, true);
        frame_dropper_.Leak(kIncomingFrameRate);
        EXPECT_FALSE(frame_dropper_.DropFrame());
      }
    }
  }

  void ValidateThroughputMatchesTargetBitrate(int bitrate_kbps,
                                              bool include_keyframe) {
    int delta_frame_size;
    int total_bytes = 0;

    if (include_keyframe) {
      delta_frame_size = ((1000.0 / 8 * bitrate_kbps) - kLargeFrameSizeBytes) /
                         (kIncomingFrameRate - 1);
    } else {
      delta_frame_size = bitrate_kbps * 1000.0 / (8 * kIncomingFrameRate);
    }
    const int kNumIterations = 1000;
    for (int i = 1; i <= kNumIterations; ++i) {
      int j = 0;
      if (include_keyframe) {
        if (!frame_dropper_.DropFrame()) {
          frame_dropper_.Fill(kLargeFrameSizeBytes, false);
          total_bytes += kLargeFrameSizeBytes;
        }
        frame_dropper_.Leak(kIncomingFrameRate);
        j++;
      }
      for (; j < kIncomingFrameRate; ++j) {
        if (!frame_dropper_.DropFrame()) {
          frame_dropper_.Fill(delta_frame_size, true);
          total_bytes += delta_frame_size;
        }
        frame_dropper_.Leak(kIncomingFrameRate);
      }
    }
    float throughput_kbps = total_bytes * 8.0 / (1000 * kNumIterations);
    float deviation_from_target =
        (throughput_kbps - kTargetBitRateKbps) * 100.0 / kTargetBitRateKbps;
    if (deviation_from_target < 0) {
      deviation_from_target = -deviation_from_target;
    }

    // Variation is < 0.1%
    EXPECT_LE(deviation_from_target, 0.1);
  }

  FrameDropper frame_dropper_;
};

TEST_F(FrameDropperTest, NoDropsWhenDisabled) {
  frame_dropper_.Enable(false);
  OverflowLeakyBucket();
  EXPECT_FALSE(frame_dropper_.DropFrame());
}

TEST_F(FrameDropperTest, DropsByDefaultWhenBucketOverflows) {
  OverflowLeakyBucket();
  EXPECT_TRUE(frame_dropper_.DropFrame());
}

TEST_F(FrameDropperTest, NoDropsWhenFillRateMatchesLeakRate) {
  for (int i = 0; i < 5 * kIncomingFrameRate; ++i) {
    frame_dropper_.Fill(kFrameSizeBytes, true);
    frame_dropper_.Leak(kIncomingFrameRate);
    EXPECT_FALSE(frame_dropper_.DropFrame());
  }
}

TEST_F(FrameDropperTest, LargeKeyFrames) {
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes, 1, false);
  frame_dropper_.Reset();
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes / 2, 2, false);
  frame_dropper_.Reset();
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes / 4, 4, false);
  frame_dropper_.Reset();
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes / 8, 8, false);
}

TEST_F(FrameDropperTest, LargeDeltaFrames) {
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes, 1, true);
  frame_dropper_.Reset();
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes / 2, 2, true);
  frame_dropper_.Reset();
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes / 4, 4, true);
  frame_dropper_.Reset();
  ValidateNoDropsAtTargetBitrate(kLargeFrameSizeBytes / 8, 8, true);
}

TEST_F(FrameDropperTest, TrafficVolumeAboveAvailableBandwidth) {
  ValidateThroughputMatchesTargetBitrate(700, kIncludeKeyFrame);
  ValidateThroughputMatchesTargetBitrate(700, kDoNotIncludeKeyFrame);
  ValidateThroughputMatchesTargetBitrate(600, kIncludeKeyFrame);
  ValidateThroughputMatchesTargetBitrate(600, kDoNotIncludeKeyFrame);
  ValidateThroughputMatchesTargetBitrate(500, kIncludeKeyFrame);
  ValidateThroughputMatchesTargetBitrate(500, kDoNotIncludeKeyFrame);
}

}  // namespace webrtc
