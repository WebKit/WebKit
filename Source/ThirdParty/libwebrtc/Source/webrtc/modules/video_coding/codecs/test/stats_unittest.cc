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

class StatsTest : public testing::Test {
 protected:
  StatsTest() {}

  virtual ~StatsTest() {}

  void SetUp() { stats_ = new Stats(); }

  void TearDown() { delete stats_; }

  Stats* stats_;
};

// Test empty object
TEST_F(StatsTest, Uninitialized) {
  EXPECT_EQ(0u, stats_->stats_.size());
  stats_->PrintSummary();  // should not crash
}

// Add single frame stats and verify
TEST_F(StatsTest, AddOne) {
  stats_->NewFrame(0u);
  FrameStatistic* frameStat = &stats_->stats_[0];
  EXPECT_EQ(0, frameStat->frame_number);
}

// Add multiple frame stats and verify
TEST_F(StatsTest, AddMany) {
  int nbr_of_frames = 1000;
  for (int i = 0; i < nbr_of_frames; ++i) {
    FrameStatistic& frameStat = stats_->NewFrame(i);
    EXPECT_EQ(i, frameStat.frame_number);
  }
  EXPECT_EQ(nbr_of_frames, static_cast<int>(stats_->stats_.size()));

  stats_->PrintSummary();  // should not crash
}

}  // namespace test
}  // namespace webrtc
