/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/receive_statistics.h"

#include <memory>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

const size_t kPacketSize1 = 100;
const size_t kPacketSize2 = 300;
const uint32_t kSsrc1 = 101;
const uint32_t kSsrc2 = 202;
const uint32_t kSsrc3 = 203;
const uint32_t kSsrc4 = 304;

RtpPacketReceived CreateRtpPacket(uint32_t ssrc,
                                  size_t header_size,
                                  size_t payload_size,
                                  size_t padding_size) {
  RtpPacketReceived packet;
  packet.SetSsrc(ssrc);
  packet.SetSequenceNumber(100);
  packet.set_payload_type_frequency(90000);
  RTC_CHECK_GE(header_size, 12);
  RTC_CHECK_EQ(header_size % 4, 0);
  if (header_size > 12) {
    // Insert csrcs to increase header size.
    const int num_csrcs = (header_size - 12) / 4;
    std::vector<uint32_t> csrcs(num_csrcs);
    packet.SetCsrcs(csrcs);
  }
  packet.SetPayloadSize(payload_size);
  packet.SetPadding(padding_size);
  return packet;
}

RtpPacketReceived CreateRtpPacket(uint32_t ssrc, size_t packet_size) {
  return CreateRtpPacket(ssrc, 12, packet_size - 12, 0);
}

void IncrementSequenceNumber(RtpPacketReceived* packet, uint16_t incr) {
  packet->SetSequenceNumber(packet->SequenceNumber() + incr);
}

void IncrementSequenceNumber(RtpPacketReceived* packet) {
  IncrementSequenceNumber(packet, 1);
}

class ReceiveStatisticsTest : public ::testing::TestWithParam<bool> {
 public:
  ReceiveStatisticsTest()
      : clock_(0),
        receive_statistics_(
            GetParam() ? ReceiveStatistics::Create(&clock_)
                       : ReceiveStatistics::CreateThreadCompatible(&clock_)) {
    packet1_ = CreateRtpPacket(kSsrc1, kPacketSize1);
    packet2_ = CreateRtpPacket(kSsrc2, kPacketSize2);
  }

 protected:
  SimulatedClock clock_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  RtpPacketReceived packet1_;
  RtpPacketReceived packet2_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ReceiveStatisticsTest,
                         ::testing::Bool(),
                         [](::testing::TestParamInfo<bool> info) {
                           return info.param ? "WithMutex" : "WithoutMutex";
                         });

TEST_P(ReceiveStatisticsTest, TwoIncomingSsrcs) {
  receive_statistics_->OnRtpPacket(packet1_);
  IncrementSequenceNumber(&packet1_);
  receive_statistics_->OnRtpPacket(packet2_);
  IncrementSequenceNumber(&packet2_);
  clock_.AdvanceTimeMilliseconds(100);
  receive_statistics_->OnRtpPacket(packet1_);
  IncrementSequenceNumber(&packet1_);
  receive_statistics_->OnRtpPacket(packet2_);
  IncrementSequenceNumber(&packet2_);

  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  StreamDataCounters counters = statistician->GetReceiveStreamDataCounters();
  EXPECT_EQ(176u, counters.transmitted.payload_bytes);
  EXPECT_EQ(24u, counters.transmitted.header_bytes);
  EXPECT_EQ(0u, counters.transmitted.padding_bytes);
  EXPECT_EQ(2u, counters.transmitted.packets);

  statistician = receive_statistics_->GetStatistician(kSsrc2);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  counters = statistician->GetReceiveStreamDataCounters();
  EXPECT_EQ(576u, counters.transmitted.payload_bytes);
  EXPECT_EQ(24u, counters.transmitted.header_bytes);
  EXPECT_EQ(0u, counters.transmitted.padding_bytes);
  EXPECT_EQ(2u, counters.transmitted.packets);

  EXPECT_EQ(2u, receive_statistics_->RtcpReportBlocks(3).size());
  // Add more incoming packets and verify that they are registered in both
  // access methods.
  receive_statistics_->OnRtpPacket(packet1_);
  IncrementSequenceNumber(&packet1_);
  receive_statistics_->OnRtpPacket(packet2_);
  IncrementSequenceNumber(&packet2_);

  counters = receive_statistics_->GetStatistician(kSsrc1)
                 ->GetReceiveStreamDataCounters();
  EXPECT_EQ(264u, counters.transmitted.payload_bytes);
  EXPECT_EQ(36u, counters.transmitted.header_bytes);
  EXPECT_EQ(0u, counters.transmitted.padding_bytes);
  EXPECT_EQ(3u, counters.transmitted.packets);

  counters = receive_statistics_->GetStatistician(kSsrc2)
                 ->GetReceiveStreamDataCounters();
  EXPECT_EQ(864u, counters.transmitted.payload_bytes);
  EXPECT_EQ(36u, counters.transmitted.header_bytes);
  EXPECT_EQ(0u, counters.transmitted.padding_bytes);
  EXPECT_EQ(3u, counters.transmitted.packets);
}

TEST_P(ReceiveStatisticsTest,
       RtcpReportBlocksReturnsMaxBlocksWhenThereAreMoreStatisticians) {
  RtpPacketReceived packet1 = CreateRtpPacket(kSsrc1, kPacketSize1);
  RtpPacketReceived packet2 = CreateRtpPacket(kSsrc2, kPacketSize1);
  RtpPacketReceived packet3 = CreateRtpPacket(kSsrc3, kPacketSize1);
  receive_statistics_->OnRtpPacket(packet1);
  receive_statistics_->OnRtpPacket(packet2);
  receive_statistics_->OnRtpPacket(packet3);

  EXPECT_THAT(receive_statistics_->RtcpReportBlocks(2), SizeIs(2));
  EXPECT_THAT(receive_statistics_->RtcpReportBlocks(2), SizeIs(2));
  EXPECT_THAT(receive_statistics_->RtcpReportBlocks(2), SizeIs(2));
}

TEST_P(ReceiveStatisticsTest,
       RtcpReportBlocksReturnsAllObservedSsrcsWithMultipleCalls) {
  RtpPacketReceived packet1 = CreateRtpPacket(kSsrc1, kPacketSize1);
  RtpPacketReceived packet2 = CreateRtpPacket(kSsrc2, kPacketSize1);
  RtpPacketReceived packet3 = CreateRtpPacket(kSsrc3, kPacketSize1);
  RtpPacketReceived packet4 = CreateRtpPacket(kSsrc4, kPacketSize1);
  receive_statistics_->OnRtpPacket(packet1);
  receive_statistics_->OnRtpPacket(packet2);
  receive_statistics_->OnRtpPacket(packet3);
  receive_statistics_->OnRtpPacket(packet4);

  std::vector<uint32_t> observed_ssrcs;
  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(2);
  ASSERT_THAT(report_blocks, SizeIs(2));
  observed_ssrcs.push_back(report_blocks[0].source_ssrc());
  observed_ssrcs.push_back(report_blocks[1].source_ssrc());

  report_blocks = receive_statistics_->RtcpReportBlocks(2);
  ASSERT_THAT(report_blocks, SizeIs(2));
  observed_ssrcs.push_back(report_blocks[0].source_ssrc());
  observed_ssrcs.push_back(report_blocks[1].source_ssrc());

  EXPECT_THAT(observed_ssrcs,
              UnorderedElementsAre(kSsrc1, kSsrc2, kSsrc3, kSsrc4));
}

TEST_P(ReceiveStatisticsTest, ActiveStatisticians) {
  receive_statistics_->OnRtpPacket(packet1_);
  IncrementSequenceNumber(&packet1_);
  clock_.AdvanceTimeMilliseconds(1000);
  receive_statistics_->OnRtpPacket(packet2_);
  IncrementSequenceNumber(&packet2_);
  // Nothing should time out since only 1000 ms has passed since the first
  // packet came in.
  EXPECT_EQ(2u, receive_statistics_->RtcpReportBlocks(3).size());

  clock_.AdvanceTimeMilliseconds(7000);
  // kSsrc1 should have timed out.
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());

  clock_.AdvanceTimeMilliseconds(1000);
  // kSsrc2 should have timed out.
  EXPECT_EQ(0u, receive_statistics_->RtcpReportBlocks(3).size());

  receive_statistics_->OnRtpPacket(packet1_);
  IncrementSequenceNumber(&packet1_);
  // kSsrc1 should be active again and the data counters should have survived.
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  StreamDataCounters counters = statistician->GetReceiveStreamDataCounters();
  EXPECT_EQ(176u, counters.transmitted.payload_bytes);
  EXPECT_EQ(24u, counters.transmitted.header_bytes);
  EXPECT_EQ(0u, counters.transmitted.padding_bytes);
  EXPECT_EQ(2u, counters.transmitted.packets);
}

TEST_P(ReceiveStatisticsTest,
       DoesntCreateRtcpReportBlockUntilFirstReceivedPacketForSsrc) {
  // Creates a statistician object for the ssrc.
  receive_statistics_->EnableRetransmitDetection(kSsrc1, true);
  EXPECT_TRUE(receive_statistics_->GetStatistician(kSsrc1) != nullptr);
  EXPECT_EQ(0u, receive_statistics_->RtcpReportBlocks(3).size());
  // Receive first packet
  receive_statistics_->OnRtpPacket(packet1_);
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());
}

TEST_P(ReceiveStatisticsTest, GetReceiveStreamDataCounters) {
  receive_statistics_->OnRtpPacket(packet1_);
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);

  StreamDataCounters counters = statistician->GetReceiveStreamDataCounters();
  EXPECT_GT(counters.first_packet_time_ms, -1);
  EXPECT_EQ(1u, counters.transmitted.packets);

  receive_statistics_->OnRtpPacket(packet1_);
  counters = statistician->GetReceiveStreamDataCounters();
  EXPECT_GT(counters.first_packet_time_ms, -1);
  EXPECT_EQ(2u, counters.transmitted.packets);
}

TEST_P(ReceiveStatisticsTest, SimpleLossComputation) {
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(3);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(4);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(5);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  // 20% = 51/255.
  EXPECT_EQ(51u, report_blocks[0].fraction_lost());
  EXPECT_EQ(1, report_blocks[0].cumulative_lost_signed());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  EXPECT_EQ(20, statistician->GetFractionLostInPercent());
}

TEST_P(ReceiveStatisticsTest, LossComputationWithReordering) {
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(3);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(2);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(5);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  // 20% = 51/255.
  EXPECT_EQ(51u, report_blocks[0].fraction_lost());
  EXPECT_EQ(1, report_blocks[0].cumulative_lost_signed());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  EXPECT_EQ(20, statistician->GetFractionLostInPercent());
}

TEST_P(ReceiveStatisticsTest, LossComputationWithDuplicates) {
  // Lose 2 packets, but also receive 1 duplicate. Should actually count as
  // only 1 packet being lost.
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(4);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(4);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(5);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  // 20% = 51/255.
  EXPECT_EQ(51u, report_blocks[0].fraction_lost());
  EXPECT_EQ(1, report_blocks[0].cumulative_lost_signed());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  EXPECT_EQ(20, statistician->GetFractionLostInPercent());
}

TEST_P(ReceiveStatisticsTest, LossComputationWithSequenceNumberWrapping) {
  // First, test loss computation over a period that included a sequence number
  // rollover.
  packet1_.SetSequenceNumber(0xfffd);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(0);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(0xfffe);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);

  // Only one packet was actually lost, 0xffff.
  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  // 20% = 51/255.
  EXPECT_EQ(51u, report_blocks[0].fraction_lost());
  EXPECT_EQ(1, report_blocks[0].cumulative_lost_signed());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  EXPECT_EQ(20, statistician->GetFractionLostInPercent());

  // Now test losing one packet *after* the rollover.
  packet1_.SetSequenceNumber(3);
  receive_statistics_->OnRtpPacket(packet1_);

  report_blocks = receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  // 50% = 127/255.
  EXPECT_EQ(127u, report_blocks[0].fraction_lost());
  EXPECT_EQ(2, report_blocks[0].cumulative_lost_signed());
  // 2 packets lost, 7 expected
  EXPECT_EQ(28, statistician->GetFractionLostInPercent());
}

TEST_P(ReceiveStatisticsTest, StreamRestartDoesntCountAsLoss) {
  receive_statistics_->SetMaxReorderingThreshold(kSsrc1, 200);

  packet1_.SetSequenceNumber(0);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);

  packet1_.SetSequenceNumber(400);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(0, report_blocks[0].fraction_lost());
  EXPECT_EQ(0, report_blocks[0].cumulative_lost_signed());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  EXPECT_EQ(0, statistician->GetFractionLostInPercent());

  packet1_.SetSequenceNumber(401);
  receive_statistics_->OnRtpPacket(packet1_);
  report_blocks = receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(0, report_blocks[0].fraction_lost());
  EXPECT_EQ(0, report_blocks[0].cumulative_lost_signed());
  EXPECT_EQ(0, statistician->GetFractionLostInPercent());
}

TEST_P(ReceiveStatisticsTest, CountsLossAfterStreamRestart) {
  receive_statistics_->SetMaxReorderingThreshold(kSsrc1, 200);

  packet1_.SetSequenceNumber(0);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);

  packet1_.SetSequenceNumber(400);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(401);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(403);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(1, report_blocks[0].cumulative_lost_signed());

  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  // Is this reasonable? */
  EXPECT_EQ(0, statistician->GetFractionLostInPercent());
}

TEST_P(ReceiveStatisticsTest, StreamCanRestartAtSequenceNumberWrapAround) {
  receive_statistics_->SetMaxReorderingThreshold(kSsrc1, 200);

  packet1_.SetSequenceNumber(0xffff - 401);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(0xffff - 400);
  receive_statistics_->OnRtpPacket(packet1_);

  packet1_.SetSequenceNumber(0xffff);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(0);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(2);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(1, report_blocks[0].cumulative_lost_signed());
}

TEST_P(ReceiveStatisticsTest, StreamRestartNeedsTwoConsecutivePackets) {
  receive_statistics_->SetMaxReorderingThreshold(kSsrc1, 200);

  packet1_.SetSequenceNumber(400);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(401);
  receive_statistics_->OnRtpPacket(packet1_);

  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);
  packet1_.SetSequenceNumber(3);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(401u, report_blocks[0].extended_high_seq_num());

  packet1_.SetSequenceNumber(4);
  receive_statistics_->OnRtpPacket(packet1_);

  report_blocks = receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(4u, report_blocks[0].extended_high_seq_num());
}

TEST_P(ReceiveStatisticsTest, WrapsAroundExtendedHighestSequenceNumber) {
  packet1_.SetSequenceNumber(0xffff);
  receive_statistics_->OnRtpPacket(packet1_);

  std::vector<rtcp::ReportBlock> report_blocks =
      receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(0xffffu, report_blocks[0].extended_high_seq_num());

  // Wrap around.
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);

  report_blocks = receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(0x10001u, report_blocks[0].extended_high_seq_num());

  // Should be treated as out of order; shouldn't increment highest extended
  // sequence number.
  packet1_.SetSequenceNumber(0x10000 - 6);
  report_blocks = receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(0x10001u, report_blocks[0].extended_high_seq_num());

  // Receive a couple packets then wrap around again.
  receive_statistics_->SetMaxReorderingThreshold(kSsrc1, 200);
  for (int i = 10; i < 0xffff; i += 150) {
    packet1_.SetSequenceNumber(i);
    receive_statistics_->OnRtpPacket(packet1_);
  }
  packet1_.SetSequenceNumber(1);
  receive_statistics_->OnRtpPacket(packet1_);
  report_blocks = receive_statistics_->RtcpReportBlocks(1);
  ASSERT_THAT(report_blocks, SizeIs(1));
  EXPECT_EQ(kSsrc1, report_blocks[0].source_ssrc());

  EXPECT_EQ(0x20001u, report_blocks[0].extended_high_seq_num());
}

TEST_P(ReceiveStatisticsTest, StreamDataCounters) {
  receive_statistics_->EnableRetransmitDetection(kSsrc1, true);

  const size_t kHeaderLength = 20;
  const size_t kPaddingLength = 9;

  // One packet with payload size kPacketSize1.
  RtpPacketReceived packet1 =
      CreateRtpPacket(kSsrc1, kHeaderLength, kPacketSize1, 0);
  receive_statistics_->OnRtpPacket(packet1);
  StreamDataCounters counters = receive_statistics_->GetStatistician(kSsrc1)
                                    ->GetReceiveStreamDataCounters();
  EXPECT_EQ(counters.transmitted.payload_bytes, kPacketSize1);
  EXPECT_EQ(counters.transmitted.header_bytes, kHeaderLength);
  EXPECT_EQ(counters.transmitted.padding_bytes, 0u);
  EXPECT_EQ(counters.transmitted.packets, 1u);
  EXPECT_EQ(counters.retransmitted.payload_bytes, 0u);
  EXPECT_EQ(counters.retransmitted.header_bytes, 0u);
  EXPECT_EQ(counters.retransmitted.padding_bytes, 0u);
  EXPECT_EQ(counters.retransmitted.packets, 0u);
  EXPECT_EQ(counters.fec.packets, 0u);

  // Another packet of size kPacketSize1 with 9 bytes padding.
  RtpPacketReceived packet2 =
      CreateRtpPacket(kSsrc1, kHeaderLength, kPacketSize1, 9);
  packet2.SetSequenceNumber(packet1.SequenceNumber() + 1);
  clock_.AdvanceTimeMilliseconds(5);
  receive_statistics_->OnRtpPacket(packet2);
  counters = receive_statistics_->GetStatistician(kSsrc1)
                 ->GetReceiveStreamDataCounters();
  EXPECT_EQ(counters.transmitted.payload_bytes, kPacketSize1 * 2);
  EXPECT_EQ(counters.transmitted.header_bytes, kHeaderLength * 2);
  EXPECT_EQ(counters.transmitted.padding_bytes, kPaddingLength);
  EXPECT_EQ(counters.transmitted.packets, 2u);

  clock_.AdvanceTimeMilliseconds(5);
  // Retransmit last packet.
  receive_statistics_->OnRtpPacket(packet2);
  counters = receive_statistics_->GetStatistician(kSsrc1)
                 ->GetReceiveStreamDataCounters();
  EXPECT_EQ(counters.transmitted.payload_bytes, kPacketSize1 * 3);
  EXPECT_EQ(counters.transmitted.header_bytes, kHeaderLength * 3);
  EXPECT_EQ(counters.transmitted.padding_bytes, kPaddingLength * 2);
  EXPECT_EQ(counters.transmitted.packets, 3u);
  EXPECT_EQ(counters.retransmitted.payload_bytes, kPacketSize1);
  EXPECT_EQ(counters.retransmitted.header_bytes, kHeaderLength);
  EXPECT_EQ(counters.retransmitted.padding_bytes, kPaddingLength);
  EXPECT_EQ(counters.retransmitted.packets, 1u);
}

TEST_P(ReceiveStatisticsTest, LastPacketReceivedTimestamp) {
  clock_.AdvanceTimeMilliseconds(42);
  receive_statistics_->OnRtpPacket(packet1_);
  StreamDataCounters counters = receive_statistics_->GetStatistician(kSsrc1)
                                    ->GetReceiveStreamDataCounters();

  EXPECT_EQ(42, counters.last_packet_received_timestamp_ms);

  clock_.AdvanceTimeMilliseconds(3);
  receive_statistics_->OnRtpPacket(packet1_);
  counters = receive_statistics_->GetStatistician(kSsrc1)
                 ->GetReceiveStreamDataCounters();
  EXPECT_EQ(45, counters.last_packet_received_timestamp_ms);
}

}  // namespace
}  // namespace webrtc
