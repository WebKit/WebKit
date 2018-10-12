/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videocodec_test_stats_impl.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {
using FrameStatistics = VideoCodecTestStatsImpl::FrameStatistics;
namespace {
const size_t kTimestamp = 12345;
}  // namespace

TEST(StatsTest, AddAndGetFrame) {
  VideoCodecTestStatsImpl stats;
  stats.AddFrame(FrameStatistics(0, kTimestamp, 0));
  FrameStatistics* frame_stat = stats.GetFrame(0u, 0);
  EXPECT_EQ(0u, frame_stat->frame_number);
  EXPECT_EQ(kTimestamp, frame_stat->rtp_timestamp);
}

TEST(StatsTest, AddAndGetFrames) {
  VideoCodecTestStatsImpl stats;
  const size_t kNumFrames = 1000;
  for (size_t i = 0; i < kNumFrames; ++i) {
    stats.AddFrame(FrameStatistics(i, kTimestamp + i, 0));
    FrameStatistics* frame_stat = stats.GetFrame(i, 0);
    EXPECT_EQ(i, frame_stat->frame_number);
    EXPECT_EQ(kTimestamp + i, frame_stat->rtp_timestamp);
  }
  EXPECT_EQ(kNumFrames, stats.Size(0));
  // Get frame.
  size_t i = 22;
  FrameStatistics* frame_stat = stats.GetFrameWithTimestamp(kTimestamp + i, 0);
  EXPECT_EQ(i, frame_stat->frame_number);
  EXPECT_EQ(kTimestamp + i, frame_stat->rtp_timestamp);
}

TEST(StatsTest, AddFrameLayering) {
  VideoCodecTestStatsImpl stats;
  for (size_t i = 0; i < 3; ++i) {
    stats.AddFrame(FrameStatistics(0, kTimestamp + i, i));
    FrameStatistics* frame_stat = stats.GetFrame(0u, i);
    EXPECT_EQ(0u, frame_stat->frame_number);
    EXPECT_EQ(kTimestamp, frame_stat->rtp_timestamp - i);
    EXPECT_EQ(1u, stats.Size(i));
  }
}

}  // namespace test
}  // namespace webrtc
