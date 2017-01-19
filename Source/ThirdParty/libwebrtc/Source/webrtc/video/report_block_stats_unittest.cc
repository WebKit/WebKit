/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/gtest.h"
#include "webrtc/video/report_block_stats.h"

namespace webrtc {

class ReportBlockStatsTest : public ::testing::Test {
 protected:
  ReportBlockStatsTest() : kSsrc1(0x12345), kSsrc2(0x23456) {}

  void SetUp() override {
    // kSsrc1: block 1-3.
    block1_1_.cumulativeLost = 10;
    block1_1_.fractionLost = 123;
    block1_1_.extendedHighSeqNum = 24000;
    block1_1_.jitter = 777;
    block1_1_.sourceSSRC = kSsrc1;
    block1_2_.cumulativeLost = 15;
    block1_2_.fractionLost = 0;
    block1_2_.extendedHighSeqNum = 24100;
    block1_2_.jitter = 222;
    block1_2_.sourceSSRC = kSsrc1;
    block1_3_.cumulativeLost = 50;
    block1_3_.fractionLost = 0;
    block1_3_.extendedHighSeqNum = 24200;
    block1_3_.jitter = 333;
    block1_3_.sourceSSRC = kSsrc1;
    // kSsrc2: block 1,2.
    block2_1_.cumulativeLost = 111;
    block2_1_.fractionLost = 222;
    block2_1_.extendedHighSeqNum = 8500;
    block2_1_.jitter = 555;
    block2_1_.sourceSSRC = kSsrc2;
    block2_2_.cumulativeLost = 136;
    block2_2_.fractionLost = 0;
    block2_2_.extendedHighSeqNum = 8800;
    block2_2_.jitter = 888;
    block2_2_.sourceSSRC = kSsrc2;

    ssrc1block1_.push_back(block1_1_);
    ssrc1block2_.push_back(block1_2_);
    ssrc12block1_.push_back(block1_1_);
    ssrc12block1_.push_back(block2_1_);
    ssrc12block2_.push_back(block1_2_);
    ssrc12block2_.push_back(block2_2_);
  }

  RtcpStatistics RtcpReportBlockToRtcpStatistics(
      const RTCPReportBlock& stats) {
    RtcpStatistics block;
    block.cumulative_lost = stats.cumulativeLost;
    block.fraction_lost = stats.fractionLost;
    block.extended_max_sequence_number = stats.extendedHighSeqNum;
    block.jitter = stats.jitter;
    return block;
  }

  const uint32_t kSsrc1;
  const uint32_t kSsrc2;
  RTCPReportBlock block1_1_;
  RTCPReportBlock block1_2_;
  RTCPReportBlock block1_3_;
  RTCPReportBlock block2_1_;
  RTCPReportBlock block2_2_;
  std::vector<RTCPReportBlock> ssrc1block1_;
  std::vector<RTCPReportBlock> ssrc1block2_;
  std::vector<RTCPReportBlock> ssrc12block1_;
  std::vector<RTCPReportBlock> ssrc12block2_;
};

TEST_F(ReportBlockStatsTest, AggregateAndStore_NoSsrc) {
  ReportBlockStats stats;
  std::vector<RTCPReportBlock> empty;
  RTCPReportBlock aggregated = stats.AggregateAndStore(empty);
  EXPECT_EQ(0U, aggregated.fractionLost);
  EXPECT_EQ(0U, aggregated.cumulativeLost);
  EXPECT_EQ(0U, aggregated.jitter);
  EXPECT_EQ(0U, aggregated.extendedHighSeqNum);
}

TEST_F(ReportBlockStatsTest, AggregateAndStore_OneSsrc) {
  ReportBlockStats stats;
  RTCPReportBlock aggregated = stats.AggregateAndStore(ssrc1block1_);
  // One ssrc, no aggregation done.
  EXPECT_EQ(123U, aggregated.fractionLost);
  EXPECT_EQ(10U, aggregated.cumulativeLost);
  EXPECT_EQ(777U, aggregated.jitter);
  EXPECT_EQ(24000U, aggregated.extendedHighSeqNum);

  aggregated = stats.AggregateAndStore(ssrc1block2_);
  EXPECT_EQ(0U, aggregated.fractionLost);
  EXPECT_EQ(15U, aggregated.cumulativeLost);
  EXPECT_EQ(222U, aggregated.jitter);
  EXPECT_EQ(24100U, aggregated.extendedHighSeqNum);

  // fl: 100 * (15-10) / (24100-24000) = 5%
  EXPECT_EQ(5, stats.FractionLostInPercent());
}

TEST_F(ReportBlockStatsTest, AggregateAndStore_TwoSsrcs) {
  ReportBlockStats stats;
  RTCPReportBlock aggregated = stats.AggregateAndStore(ssrc12block1_);
  EXPECT_EQ(0U, aggregated.fractionLost);
  EXPECT_EQ(10U + 111U, aggregated.cumulativeLost);
  EXPECT_EQ((777U + 555U) / 2, aggregated.jitter);
  EXPECT_EQ(0U, aggregated.extendedHighSeqNum);

  aggregated = stats.AggregateAndStore(ssrc12block2_);
  // fl: 255 * ((15-10) + (136-111)) / ((24100-24000) + (8800-8500)) = 19
  EXPECT_EQ(19U, aggregated.fractionLost);
  EXPECT_EQ(15U + 136U, aggregated.cumulativeLost);
  EXPECT_EQ((222U + 888U) / 2, aggregated.jitter);
  EXPECT_EQ(0U, aggregated.extendedHighSeqNum);

  // fl: 100 * ((15-10) + (136-111)) / ((24100-24000) + (8800-8500)) = 7%
  EXPECT_EQ(7, stats.FractionLostInPercent());
}

TEST_F(ReportBlockStatsTest, StoreAndGetFractionLost) {
  const uint32_t kRemoteSsrc = 1;
  ReportBlockStats stats;
  EXPECT_EQ(-1, stats.FractionLostInPercent());

  // First block.
  stats.Store(RtcpReportBlockToRtcpStatistics(block1_1_), kRemoteSsrc, kSsrc1);
  EXPECT_EQ(-1, stats.FractionLostInPercent());
  // fl: 100 * (15-10) / (24100-24000) = 5%
  stats.Store(RtcpReportBlockToRtcpStatistics(block1_2_), kRemoteSsrc, kSsrc1);
  EXPECT_EQ(5, stats.FractionLostInPercent());
  // fl: 100 * (50-10) / (24200-24000) = 20%
  stats.Store(RtcpReportBlockToRtcpStatistics(block1_3_), kRemoteSsrc, kSsrc1);
  EXPECT_EQ(20, stats.FractionLostInPercent());
}

}  // namespace webrtc

