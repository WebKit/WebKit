/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"

#include <utility>

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::make_tuple;
using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::ReportBlock;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kFractionLost = 55;
const int32_t kCumulativeLost = 0x111213;
const uint32_t kExtHighestSeqNum = 0x22232425;
const uint32_t kJitter = 0x33343536;
const uint32_t kLastSr = 0x44454647;
const uint32_t kDelayLastSr = 0x55565758;
// Manually created ReceiverReport with one ReportBlock matching constants
// above.
// Having this block allows to test Create and Parse separately.
const uint8_t kPacket[] = {0x81, 201,  0x00, 0x07, 0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89, 55,   0x11, 0x12, 0x13,
                           0x22, 0x23, 0x24, 0x25, 0x33, 0x34, 0x35, 0x36,
                           0x44, 0x45, 0x46, 0x47, 0x55, 0x56, 0x57, 0x58};
}  // namespace

TEST(RtcpPacketReceiverReportTest, ParseWithOneReportBlock) {
  ReceiverReport rr;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &rr));
  const ReceiverReport& parsed = rr;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(1u, parsed.report_blocks().size());
  const ReportBlock& rb = parsed.report_blocks().front();
  EXPECT_EQ(kRemoteSsrc, rb.source_ssrc());
  EXPECT_EQ(kFractionLost, rb.fraction_lost());
  EXPECT_EQ(kCumulativeLost, rb.cumulative_lost_signed());
  EXPECT_EQ(kExtHighestSeqNum, rb.extended_high_seq_num());
  EXPECT_EQ(kJitter, rb.jitter());
  EXPECT_EQ(kLastSr, rb.last_sr());
  EXPECT_EQ(kDelayLastSr, rb.delay_since_last_sr());
}

TEST(RtcpPacketReceiverReportTest, ParseFailsOnIncorrectSize) {
  rtc::Buffer damaged_packet(kPacket);
  damaged_packet[0]++;  // Damage the packet: increase count field.
  ReceiverReport rr;
  EXPECT_FALSE(test::ParseSinglePacket(damaged_packet, &rr));
}

TEST(RtcpPacketReceiverReportTest, CreateWithOneReportBlock) {
  ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  ReportBlock rb;
  rb.SetMediaSsrc(kRemoteSsrc);
  rb.SetFractionLost(kFractionLost);
  rb.SetCumulativeLost(kCumulativeLost);
  rb.SetExtHighestSeqNum(kExtHighestSeqNum);
  rb.SetJitter(kJitter);
  rb.SetLastSr(kLastSr);
  rb.SetDelayLastSr(kDelayLastSr);
  rr.AddReportBlock(rb);

  rtc::Buffer raw = rr.Build();

  EXPECT_THAT(make_tuple(raw.data(), raw.size()), ElementsAreArray(kPacket));
}

TEST(RtcpPacketReceiverReportTest, CreateAndParseWithoutReportBlocks) {
  ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);

  rtc::Buffer raw = rr.Build();
  ReceiverReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.report_blocks(), IsEmpty());
}

TEST(RtcpPacketReceiverReportTest, CreateAndParseWithTwoReportBlocks) {
  ReceiverReport rr;
  ReportBlock rb1;
  rb1.SetMediaSsrc(kRemoteSsrc);
  ReportBlock rb2;
  rb2.SetMediaSsrc(kRemoteSsrc + 1);

  rr.SetSenderSsrc(kSenderSsrc);
  EXPECT_TRUE(rr.AddReportBlock(rb1));
  EXPECT_TRUE(rr.AddReportBlock(rb2));

  rtc::Buffer raw = rr.Build();
  ReceiverReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(2u, parsed.report_blocks().size());
  EXPECT_EQ(kRemoteSsrc, parsed.report_blocks()[0].source_ssrc());
  EXPECT_EQ(kRemoteSsrc + 1, parsed.report_blocks()[1].source_ssrc());
}

TEST(RtcpPacketReceiverReportTest, CreateWithTooManyReportBlocks) {
  ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  ReportBlock rb;
  for (size_t i = 0; i < ReceiverReport::kMaxNumberOfReportBlocks; ++i) {
    rb.SetMediaSsrc(kRemoteSsrc + i);
    EXPECT_TRUE(rr.AddReportBlock(rb));
  }
  rb.SetMediaSsrc(kRemoteSsrc + ReceiverReport::kMaxNumberOfReportBlocks);
  EXPECT_FALSE(rr.AddReportBlock(rb));
}

TEST(RtcpPacketReceiverReportTest, SetReportBlocksOverwritesOldBlocks) {
  ReceiverReport rr;
  ReportBlock report_block;
  // Use jitter field of the report blocks to distinguish them.
  report_block.SetJitter(1001u);
  rr.AddReportBlock(report_block);
  ASSERT_EQ(rr.report_blocks().size(), 1u);
  ASSERT_EQ(rr.report_blocks()[0].jitter(), 1001u);

  std::vector<ReportBlock> blocks(3u);
  blocks[0].SetJitter(2001u);
  blocks[1].SetJitter(3001u);
  blocks[2].SetJitter(4001u);
  EXPECT_TRUE(rr.SetReportBlocks(blocks));
  ASSERT_EQ(rr.report_blocks().size(), 3u);
  EXPECT_EQ(rr.report_blocks()[0].jitter(), 2001u);
  EXPECT_EQ(rr.report_blocks()[1].jitter(), 3001u);
  EXPECT_EQ(rr.report_blocks()[2].jitter(), 4001u);
}

TEST(RtcpPacketReceiverReportTest, SetReportBlocksMaxLimit) {
  ReceiverReport rr;
  std::vector<ReportBlock> max_blocks(ReceiverReport::kMaxNumberOfReportBlocks);
  EXPECT_TRUE(rr.SetReportBlocks(std::move(max_blocks)));

  std::vector<ReportBlock> one_too_many_blocks(
      ReceiverReport::kMaxNumberOfReportBlocks + 1);
  EXPECT_FALSE(rr.SetReportBlocks(std::move(one_too_many_blocks)));
}

}  // namespace webrtc
