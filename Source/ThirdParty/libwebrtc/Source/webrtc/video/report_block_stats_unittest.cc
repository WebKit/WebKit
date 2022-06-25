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
namespace {

constexpr uint32_t kSsrc1 = 123;
constexpr uint32_t kSsrc2 = 234;

TEST(ReportBlockStatsTest, StoreAndGetFractionLost) {
  ReportBlockStats stats;
  EXPECT_EQ(-1, stats.FractionLostInPercent());

  // First report.
  stats.Store(kSsrc1, /*packets_lost=*/10,
              /*extended_highest_sequence_number=*/24'000);
  EXPECT_EQ(-1, stats.FractionLostInPercent());
  // fl: 100 * (15-10) / (24100-24000) = 5%
  stats.Store(kSsrc1, /*packets_lost=*/15,
              /*extended_highest_sequence_number=*/24'100);
  EXPECT_EQ(5, stats.FractionLostInPercent());
  // fl: 100 * (50-10) / (24200-24000) = 20%
  stats.Store(kSsrc1, /*packets_lost=*/50,
              /*extended_highest_sequence_number=*/24'200);
  EXPECT_EQ(20, stats.FractionLostInPercent());
}

TEST(ReportBlockStatsTest, StoreAndGetFractionLost_TwoSsrcs) {
  ReportBlockStats stats;
  EXPECT_EQ(-1, stats.FractionLostInPercent());

  // First report.
  stats.Store(kSsrc1, /*packets_lost=*/10,
              /*extended_highest_sequence_number=*/24'000);
  EXPECT_EQ(-1, stats.FractionLostInPercent());
  // fl: 100 * (15-10) / (24100-24000) = 5%
  stats.Store(kSsrc1, /*packets_lost=*/15,
              /*extended_highest_sequence_number=*/24'100);
  EXPECT_EQ(5, stats.FractionLostInPercent());

  // First report, kSsrc2.
  stats.Store(kSsrc2, /*packets_lost=*/111,
              /*extended_highest_sequence_number=*/8'500);
  EXPECT_EQ(5, stats.FractionLostInPercent());
  // fl: 100 * ((15-10) + (136-111)) / ((24100-24000) + (8800-8500)) = 7%
  stats.Store(kSsrc2, /*packets_lost=*/136,
              /*extended_highest_sequence_number=*/8'800);
  EXPECT_EQ(7, stats.FractionLostInPercent());
}

}  // namespace
}  // namespace webrtc
