/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/stats.h"

#include "webrtc/test/gtest.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

TEST(StatsTest, TestEmptyObject) {
  Stats stats;
  EXPECT_EQ(0u, stats.stats_.size());
  stats.PrintSummary();  // should not crash
}

TEST(StatsTest, AddSingleFrame) {
  const int kFrameNumber = 0;
  Stats stats;
  stats.NewFrame(kFrameNumber);
  EXPECT_EQ(1u, stats.stats_.size());
  FrameStatistic* frame_stat = &stats.stats_[0];
  EXPECT_EQ(kFrameNumber, frame_stat->frame_number);
}

TEST(StatsTest, AddMultipleFrames) {
  Stats stats;
  const int kNumFrames = 1000;
  for (int i = 0; i < kNumFrames; ++i) {
    FrameStatistic& frame_stat = stats.NewFrame(i);
    EXPECT_EQ(i, frame_stat.frame_number);
  }
  EXPECT_EQ(kNumFrames, static_cast<int>(stats.stats_.size()));

  stats.PrintSummary();  // should not crash
}

}  // namespace test
}  // namespace webrtc
