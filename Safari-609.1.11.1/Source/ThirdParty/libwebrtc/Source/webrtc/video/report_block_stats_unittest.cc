/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/report_block_stats.h"

#include "test/gtest.h"

namespace webrtc {

class ReportBlockStatsTest : public ::testing::Test {
 protected:
  ReportBlockStatsTest() {
    // kSsrc1: report 1-3.
    stats1_1_.packets_lost = 10;
    stats1_1_.extended_highest_sequence_number = 24000;
    stats1_2_.packets_lost = 15;
    stats1_2_.extended_highest_sequence_number = 24100;
    stats1_3_.packets_lost = 50;
    stats1_3_.extended_highest_sequence_number = 24200;
    // kSsrc2: report 1,2.
    stats2_1_.packets_lost = 111;
    stats2_1_.extended_highest_sequence_number = 8500;
    stats2_2_.packets_lost = 136;
    stats2_2_.extended_highest_sequence_number = 8800;
  }

  const uint32_t kSsrc1 = 123;
  const uint32_t kSsrc2 = 234;
  RtcpStatistics stats1_1_;
  RtcpStatistics stats1_2_;
  RtcpStatistics stats1_3_;
  RtcpStatistics stats2_1_;
  RtcpStatistics stats2_2_;
};

TEST_F(ReportBlockStatsTest, StoreAndGetFractionLost) {
  ReportBlockStats stats;
  EXPECT_EQ(-1, stats.FractionLostInPercent());

  // First report.
  stats.Store(kSsrc1, stats1_1_);
  EXPECT_EQ(-1, stats.FractionLostInPercent());
  // fl: 100 * (15-10) / (24100-24000) = 5%
  stats.Store(kSsrc1, stats1_2_);
  EXPECT_EQ(5, stats.FractionLostInPercent());
  // fl: 100 * (50-10) / (24200-24000) = 20%
  stats.Store(kSsrc1, stats1_3_);
  EXPECT_EQ(20, stats.FractionLostInPercent());
}

TEST_F(ReportBlockStatsTest, StoreAndGetFractionLost_TwoSsrcs) {
  ReportBlockStats stats;
  EXPECT_EQ(-1, stats.FractionLostInPercent());

  // First report.
  stats.Store(kSsrc1, stats1_1_);
  EXPECT_EQ(-1, stats.FractionLostInPercent());
  // fl: 100 * (15-10) / (24100-24000) = 5%
  stats.Store(kSsrc1, stats1_2_);
  EXPECT_EQ(5, stats.FractionLostInPercent());

  // First report, kSsrc2.
  stats.Store(kSsrc2, stats2_1_);
  EXPECT_EQ(5, stats.FractionLostInPercent());
  // fl: 100 * ((15-10) + (136-111)) / ((24100-24000) + (8800-8500)) = 7%
  stats.Store(kSsrc2, stats2_2_);
  EXPECT_EQ(7, stats.FractionLostInPercent());
}

}  // namespace webrtc
