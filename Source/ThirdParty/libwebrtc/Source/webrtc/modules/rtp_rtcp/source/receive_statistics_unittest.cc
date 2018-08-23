/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

const size_t kPacketSize1 = 100;
const size_t kPacketSize2 = 300;
const uint32_t kSsrc1 = 101;
const uint32_t kSsrc2 = 202;
const uint32_t kSsrc3 = 203;
const uint32_t kSsrc4 = 304;

RTPHeader CreateRtpHeader(uint32_t ssrc) {
  RTPHeader header;
  memset(&header, 0, sizeof(header));
  header.ssrc = ssrc;
  header.sequenceNumber = 100;
  return header;
}

class ReceiveStatisticsTest : public ::testing::Test {
 public:
  ReceiveStatisticsTest()
      : clock_(0), receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    header1_ = CreateRtpHeader(kSsrc1);
    header2_ = CreateRtpHeader(kSsrc2);
  }

 protected:
  SimulatedClock clock_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  RTPHeader header1_;
  RTPHeader header2_;
};

TEST_F(ReceiveStatisticsTest, TwoIncomingSsrcs) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(100);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  size_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  statistician = receive_statistics_->GetStatistician(kSsrc2);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(600u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  EXPECT_EQ(2u, receive_statistics_->RtcpReportBlocks(3).size());
  // Add more incoming packets and verify that they are registered in both
  // access methods.
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  receive_statistics_->GetStatistician(kSsrc1)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  receive_statistics_->GetStatistician(kSsrc2)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);
}

TEST_F(ReceiveStatisticsTest,
       RtcpReportBlocksReturnsMaxBlocksWhenThereAreMoreStatisticians) {
  RTPHeader header1 = CreateRtpHeader(kSsrc1);
  RTPHeader header2 = CreateRtpHeader(kSsrc2);
  RTPHeader header3 = CreateRtpHeader(kSsrc3);
  receive_statistics_->IncomingPacket(header1, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header2, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header3, kPacketSize1, false);

  EXPECT_THAT(receive_statistics_->RtcpReportBlocks(2), SizeIs(2));
  EXPECT_THAT(receive_statistics_->RtcpReportBlocks(2), SizeIs(2));
  EXPECT_THAT(receive_statistics_->RtcpReportBlocks(2), SizeIs(2));
}

TEST_F(ReceiveStatisticsTest,
       RtcpReportBlocksReturnsAllObservedSsrcsWithMultipleCalls) {
  RTPHeader header1 = CreateRtpHeader(kSsrc1);
  RTPHeader header2 = CreateRtpHeader(kSsrc2);
  RTPHeader header3 = CreateRtpHeader(kSsrc3);
  RTPHeader header4 = CreateRtpHeader(kSsrc4);
  receive_statistics_->IncomingPacket(header1, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header2, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header3, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header4, kPacketSize1, false);

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

TEST_F(ReceiveStatisticsTest, ActiveStatisticians) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(1000);
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  // Nothing should time out since only 1000 ms has passed since the first
  // packet came in.
  EXPECT_EQ(2u, receive_statistics_->RtcpReportBlocks(3).size());

  clock_.AdvanceTimeMilliseconds(7000);
  // kSsrc1 should have timed out.
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());

  clock_.AdvanceTimeMilliseconds(1000);
  // kSsrc2 should have timed out.
  EXPECT_EQ(0u, receive_statistics_->RtcpReportBlocks(3).size());

  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  // kSsrc1 should be active again and the data counters should have survived.
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  size_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);
}

TEST_F(ReceiveStatisticsTest, GetReceiveStreamDataCounters) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);

  StreamDataCounters counters;
  statistician->GetReceiveStreamDataCounters(&counters);
  EXPECT_GT(counters.first_packet_time_ms, -1);
  EXPECT_EQ(1u, counters.transmitted.packets);

  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  statistician->GetReceiveStreamDataCounters(&counters);
  EXPECT_GT(counters.first_packet_time_ms, -1);
  EXPECT_EQ(2u, counters.transmitted.packets);
}

class MockRtcpCallback : public RtcpStatisticsCallback {
 public:
  MOCK_METHOD2(StatisticsUpdated,
               void(const RtcpStatistics& statistics, uint32_t ssrc));
  MOCK_METHOD2(CNameChanged, void(const char* cname, uint32_t ssrc));
};

// Test that the RTCP statistics callback is invoked every time a packet is
// received (so that at the application level, GetStats will return up-to-date
// stats, not just stats from the last generated RTCP SR or RR).
TEST_F(ReceiveStatisticsTest,
       RtcpStatisticsCallbackInvokedForEveryPacketReceived) {
  MockRtcpCallback callback;
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Just receive the same packet multiple times; doesn't really matter for the
  // purposes of this test.
  EXPECT_CALL(callback, StatisticsUpdated(_, _)).Times(3);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
}

// The callback should also be invoked when |fraction_lost| is updated due to
// GetStatistics being called.
TEST_F(ReceiveStatisticsTest,
       RtcpStatisticsCallbackInvokedWhenFractionLostUpdated) {
  MockRtcpCallback callback;
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  EXPECT_CALL(callback, StatisticsUpdated(_, _)).Times(2);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  // This just returns the current statistics without updating anything, so no
  // need to invoke the callback.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/false);

  // Update fraction lost, expecting a new callback.
  EXPECT_CALL(callback, StatisticsUpdated(_, _)).Times(1);
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
}

TEST_F(ReceiveStatisticsTest,
       RtcpStatisticsCallbackNotInvokedAfterDeregistered) {
  // Register the callback and receive a couple packets.
  MockRtcpCallback callback;
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);
  EXPECT_CALL(callback, StatisticsUpdated(_, _)).Times(2);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  // Dereigster the callback. Neither receiving a packet nor generating a
  // report (calling GetStatistics) should result in another callback.
  receive_statistics_->RegisterRtcpStatisticsCallback(nullptr);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
}

// Test that the RtcpStatisticsCallback sees the exact same values as returned
// from GetStatistics.
TEST_F(ReceiveStatisticsTest,
       RtcpStatisticsFromCallbackMatchThoseFromGetStatistics) {
  MockRtcpCallback callback;
  RtcpStatistics stats_from_callback;
  EXPECT_CALL(callback, StatisticsUpdated(_, _))
      .WillRepeatedly(SaveArg<0>(&stats_from_callback));
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Using units of milliseconds.
  header1_.payload_type_frequency = 1000;
  // Add some arbitrary data, with loss and jitter.
  header1_.sequenceNumber = 1;
  clock_.AdvanceTimeMilliseconds(7);
  header1_.timestamp += 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber += 2;
  clock_.AdvanceTimeMilliseconds(9);
  header1_.timestamp += 9;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  --header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(13);
  header1_.timestamp += 47;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  header1_.sequenceNumber += 3;
  clock_.AdvanceTimeMilliseconds(11);
  header1_.timestamp += 17;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  // The stats from the last callback due to IncomingPacket should match
  // those returned by GetStatistics afterwards.
  RtcpStatistics stats_from_getstatistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &stats_from_getstatistics, /*update_fraction_lost=*/false);

  EXPECT_EQ(stats_from_getstatistics.packets_lost,
            stats_from_callback.packets_lost);
  EXPECT_EQ(stats_from_getstatistics.extended_highest_sequence_number,
            stats_from_callback.extended_highest_sequence_number);
  EXPECT_EQ(stats_from_getstatistics.fraction_lost,
            stats_from_callback.fraction_lost);
  EXPECT_EQ(stats_from_getstatistics.jitter, stats_from_callback.jitter);

  // Now update fraction lost, and check that we got matching values from the
  // new callback.
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &stats_from_getstatistics, /*update_fraction_lost=*/true);
  EXPECT_EQ(stats_from_getstatistics.packets_lost,
            stats_from_callback.packets_lost);
  EXPECT_EQ(stats_from_getstatistics.extended_highest_sequence_number,
            stats_from_callback.extended_highest_sequence_number);
  EXPECT_EQ(stats_from_getstatistics.fraction_lost,
            stats_from_callback.fraction_lost);
  EXPECT_EQ(stats_from_getstatistics.jitter, stats_from_callback.jitter);
}

// Test that |fraction_lost| is only updated when a report is generated (when
// GetStatistics is called with |update_fraction_lost| set to true). Meaning
// that it will always represent a value computed between two RTCP SR or RRs.
TEST_F(ReceiveStatisticsTest, FractionLostOnlyUpdatedWhenReportGenerated) {
  MockRtcpCallback callback;
  RtcpStatistics stats_from_callback;
  EXPECT_CALL(callback, StatisticsUpdated(_, _))
      .WillRepeatedly(SaveArg<0>(&stats_from_callback));
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Simulate losing one packet.
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 2;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 4;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  // Haven't generated a report yet, so |fraction_lost| should still be 0.
  EXPECT_EQ(0u, stats_from_callback.fraction_lost);

  // Call GetStatistics with |update_fraction_lost| set to false; should be a
  // no-op.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/false);
  EXPECT_EQ(0u, stats_from_callback.fraction_lost);

  // Call GetStatistics with |update_fraction_lost| set to true, simulating a
  // report being generated.
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 25% = 63/255.
  EXPECT_EQ(63u, stats_from_callback.fraction_lost);

  // Lose another packet.
  header1_.sequenceNumber = 6;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  // Should return same value as before since we haven't generated a new report
  // yet.
  EXPECT_EQ(63u, stats_from_callback.fraction_lost);

  // Simulate another report being generated.
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 50% = 127/255.
  EXPECT_EQ(127, stats_from_callback.fraction_lost);
}

// Simple test for fraction/cumulative loss computation, with only loss, no
// duplicates or reordering.
TEST_F(ReceiveStatisticsTest, SimpleLossComputation) {
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 4;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 5;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 20% = 51/255.
  EXPECT_EQ(51u, statistics.fraction_lost);
  EXPECT_EQ(1, statistics.packets_lost);
}

// Test that fraction/cumulative loss is computed correctly when there's some
// reordering.
TEST_F(ReceiveStatisticsTest, LossComputationWithReordering) {
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 2;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 5;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 20% = 51/255.
  EXPECT_EQ(51u, statistics.fraction_lost);
}

// Somewhat unintuitively, duplicate packets count against lost packets
// according to RFC3550.
TEST_F(ReceiveStatisticsTest, LossComputationWithDuplicates) {
  // Lose 2 packets, but also receive 1 duplicate. Should actually count as
  // only 1 packet being lost.
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 4;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 4;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 5;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 20% = 51/255.
  EXPECT_EQ(51u, statistics.fraction_lost);
  EXPECT_EQ(1, statistics.packets_lost);
}

// Test that sequence numbers wrapping around doesn't screw up loss
// computations.
TEST_F(ReceiveStatisticsTest, LossComputationWithSequenceNumberWrapping) {
  // First, test loss computation over a period that included a sequence number
  // rollover.
  header1_.sequenceNumber = 65533;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 0;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 65534;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  // Only one packet was actually lost, 65535.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 20% = 51/255.
  EXPECT_EQ(51u, statistics.fraction_lost);
  EXPECT_EQ(1, statistics.packets_lost);

  // Now test losing one packet *after* the rollover.
  header1_.sequenceNumber = 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 50% = 127/255.
  EXPECT_EQ(127u, statistics.fraction_lost);
  EXPECT_EQ(2, statistics.packets_lost);
}

// Somewhat unintuitively, since duplicate packets count against loss, you can
// actually end up with negative loss. |fraction_lost| should be clamped to
// zero in this case, since it's signed, while |packets_lost| is signed so it
// should be negative.
TEST_F(ReceiveStatisticsTest, NegativeLoss) {
  // Receive one packet and simulate a report being generated by calling
  // GetStatistics, to establish a baseline for |fraction_lost|.
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);

  // Receive some duplicate packets. Results in "negative" loss, since
  // "expected packets since last report" is 3 and "received" is 4, and 3 minus
  // 4 is -1. See RFC3550 Appendix A.3.
  header1_.sequenceNumber = 4;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 2;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 2;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber = 2;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  EXPECT_EQ(0u, statistics.fraction_lost);
  // TODO(bugs.webrtc.org/9598): Since old WebRTC implementations reads this
  // value as unsigned we currently limit it to 0.
  EXPECT_EQ(0, statistics.packets_lost);

  // Lose 2 packets; now cumulative loss should become positive again.
  header1_.sequenceNumber = 7;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/true);
  // 66% = 170/255.
  EXPECT_EQ(170u, statistics.fraction_lost);
  EXPECT_EQ(1, statistics.packets_lost);
}

// Since cumulative loss is carried in a signed 24-bit field, it should be
// clamped to 0x7fffff in the positive direction, 0x800000 in the negative
// direction.
TEST_F(ReceiveStatisticsTest, PositiveCumulativeLossClamped) {
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  // Lose 2^23 packets, expecting loss to be clamped to 2^23-1.
  for (int i = 0; i < 0x800000; ++i) {
    header1_.sequenceNumber = (header1_.sequenceNumber + 2 % 65536);
    receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  }
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/false);
  EXPECT_EQ(0x7fffff, statistics.packets_lost);
}

TEST_F(ReceiveStatisticsTest, NegativeCumulativeLossClamped) {
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);

  // Receive 2^23+1 duplicate packets (counted as negative loss), expecting
  // loss to be clamped to -2^23.
  for (int i = 0; i < 0x800001; ++i) {
    receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  }
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)->GetStatistics(
      &statistics, /*update_fraction_lost=*/false);
  // TODO(bugs.webrtc.org/9598): Since old WebRTC implementations reads this
  // value as unsigned we currently limit it to 0.
  EXPECT_EQ(0, statistics.packets_lost);
}

// Test that the extended highest sequence number is computed correctly when
// sequence numbers wrap around or packets are received out of order.
TEST_F(ReceiveStatisticsTest, ExtendedHighestSequenceNumberComputation) {
  MockRtcpCallback callback;
  RtcpStatistics stats_from_callback;
  EXPECT_CALL(callback, StatisticsUpdated(_, _))
      .WillRepeatedly(SaveArg<0>(&stats_from_callback));
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  header1_.sequenceNumber = 65535;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(65535u, stats_from_callback.extended_highest_sequence_number);

  // Wrap around.
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(65536u + 1u, stats_from_callback.extended_highest_sequence_number);

  // Should be treated as out of order; shouldn't increment highest extended
  // sequence number.
  header1_.sequenceNumber = 65530;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(65536u + 1u, stats_from_callback.extended_highest_sequence_number);

  // Receive a couple packets then wrap around again.
  // TODO(bugs.webrtc.org/9445): With large jumps like this, RFC3550 suggests
  // for the receiver to assume the other side restarted, and reset all its
  // sequence number counters. Why aren't we doing this?
  header1_.sequenceNumber = 30000;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(65536u + 30000u,
            stats_from_callback.extended_highest_sequence_number);

  header1_.sequenceNumber = 50000;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(65536u + 50000u,
            stats_from_callback.extended_highest_sequence_number);

  header1_.sequenceNumber = 10000;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(2 * 65536u + 10000u,
            stats_from_callback.extended_highest_sequence_number);

  // If a packet is received more than "MaxReorderingThreshold" packets out of
  // order (defaults to 50), it's assumed to be in order.
  // TODO(bugs.webrtc.org/9445): RFC3550 would recommend treating this as a
  // restart as mentioned above.
  header1_.sequenceNumber = 9900;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(3 * 65536u + 9900u,
            stats_from_callback.extended_highest_sequence_number);
}

// Test jitter computation with no loss/reordering/etc.
TEST_F(ReceiveStatisticsTest, SimpleJitterComputation) {
  MockRtcpCallback callback;
  RtcpStatistics stats_from_callback;
  EXPECT_CALL(callback, StatisticsUpdated(_, _))
      .WillRepeatedly(SaveArg<0>(&stats_from_callback));
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Using units of milliseconds.
  header1_.payload_type_frequency = 1000;

  // Regardless of initial timestamps, jitter should start at 0.
  header1_.sequenceNumber = 1;
  clock_.AdvanceTimeMilliseconds(7);
  header1_.timestamp += 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(0u, stats_from_callback.jitter);

  // Incrementing timestamps by the same amount shouldn't increase jitter.
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(50);
  header1_.timestamp += 50;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(0u, stats_from_callback.jitter);

  // Difference of 16ms, divided by 16 yields exactly 1.
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(32);
  header1_.timestamp += 16;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  EXPECT_EQ(1u, stats_from_callback.jitter);

  // (90 + 1 * 15) / 16 = 6.5625; should round down to 6.
  // TODO(deadbeef): Why don't we round to the nearest integer?
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(10);
  header1_.timestamp += 100;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  EXPECT_EQ(6u, stats_from_callback.jitter);

  // (30 + 6.5625 * 15) / 16 = 8.0273; should round down to 8.
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(50);
  header1_.timestamp += 20;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  EXPECT_EQ(8u, stats_from_callback.jitter);
}

// TODO(deadbeef): Why do we do this? It goes against RFC3550, which explicitly
// says the calculation should be based on order of arrival and packets may not
// necessarily arrive in sequence.
TEST_F(ReceiveStatisticsTest, JitterComputationIgnoresReorderedPackets) {
  MockRtcpCallback callback;
  RtcpStatistics stats_from_callback;
  EXPECT_CALL(callback, StatisticsUpdated(_, _))
      .WillRepeatedly(SaveArg<0>(&stats_from_callback));
  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Using units of milliseconds.
  header1_.payload_type_frequency = 1000;

  // Regardless of initial timestamps, jitter should start at 0.
  header1_.sequenceNumber = 1;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(0u, stats_from_callback.jitter);

  // This should be ignored, even though there's a difference of 70 here.
  header1_.sequenceNumber = 0;
  clock_.AdvanceTimeMilliseconds(50);
  header1_.timestamp -= 20;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(0u, stats_from_callback.jitter);

  // Relative to the first packet there's a difference of 181ms in arrival
  // time, 20ms in timestamp, so jitter should be 161/16 = 10.
  header1_.sequenceNumber = 2;
  clock_.AdvanceTimeMilliseconds(131);
  header1_.timestamp += 40;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  EXPECT_EQ(10u, stats_from_callback.jitter);
}

class RtpTestCallback : public StreamDataCountersCallback {
 public:
  RtpTestCallback()
      : StreamDataCountersCallback(), num_calls_(0), ssrc_(0), stats_() {}
  ~RtpTestCallback() override = default;

  void DataCountersUpdated(const StreamDataCounters& counters,
                           uint32_t ssrc) override {
    ssrc_ = ssrc;
    stats_ = counters;
    ++num_calls_;
  }

  void MatchPacketCounter(const RtpPacketCounter& expected,
                          const RtpPacketCounter& actual) {
    EXPECT_EQ(expected.payload_bytes, actual.payload_bytes);
    EXPECT_EQ(expected.header_bytes, actual.header_bytes);
    EXPECT_EQ(expected.padding_bytes, actual.padding_bytes);
    EXPECT_EQ(expected.packets, actual.packets);
  }

  void Matches(uint32_t num_calls,
               uint32_t ssrc,
               const StreamDataCounters& expected) {
    EXPECT_EQ(num_calls, num_calls_);
    EXPECT_EQ(ssrc, ssrc_);
    MatchPacketCounter(expected.transmitted, stats_.transmitted);
    MatchPacketCounter(expected.retransmitted, stats_.retransmitted);
    MatchPacketCounter(expected.fec, stats_.fec);
  }

  uint32_t num_calls_;
  uint32_t ssrc_;
  StreamDataCounters stats_;
};

TEST_F(ReceiveStatisticsTest, RtpCallbacks) {
  RtpTestCallback callback;
  receive_statistics_->RegisterRtpStatisticsCallback(&callback);

  const size_t kHeaderLength = 20;
  const size_t kPaddingLength = 9;

  // One packet of size kPacketSize1.
  header1_.headerLength = kHeaderLength;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1 + kHeaderLength,
                                      false);
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = kPacketSize1;
  expected.transmitted.header_bytes = kHeaderLength;
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.retransmitted.payload_bytes = 0;
  expected.retransmitted.header_bytes = 0;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 0;
  expected.fec.packets = 0;
  callback.Matches(1, kSsrc1, expected);

  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(5);
  header1_.paddingLength = 9;
  // Another packet of size kPacketSize1 with 9 bytes padding.
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength + kPaddingLength, false);
  expected.transmitted.payload_bytes = kPacketSize1 * 2;
  expected.transmitted.header_bytes = kHeaderLength * 2;
  expected.transmitted.padding_bytes = kPaddingLength;
  expected.transmitted.packets = 2;
  callback.Matches(2, kSsrc1, expected);

  clock_.AdvanceTimeMilliseconds(5);
  // Retransmit last packet.
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength + kPaddingLength, true);
  expected.transmitted.payload_bytes = kPacketSize1 * 3;
  expected.transmitted.header_bytes = kHeaderLength * 3;
  expected.transmitted.padding_bytes = kPaddingLength * 2;
  expected.transmitted.packets = 3;
  expected.retransmitted.payload_bytes = kPacketSize1;
  expected.retransmitted.header_bytes = kHeaderLength;
  expected.retransmitted.padding_bytes = kPaddingLength;
  expected.retransmitted.packets = 1;
  callback.Matches(3, kSsrc1, expected);

  header1_.paddingLength = 0;
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(5);
  // One FEC packet.
  receive_statistics_->IncomingPacket(header1_, kPacketSize1 + kHeaderLength,
                                      false);
  receive_statistics_->FecPacketReceived(header1_,
                                         kPacketSize1 + kHeaderLength);
  expected.transmitted.payload_bytes = kPacketSize1 * 4;
  expected.transmitted.header_bytes = kHeaderLength * 4;
  expected.transmitted.packets = 4;
  expected.fec.payload_bytes = kPacketSize1;
  expected.fec.header_bytes = kHeaderLength;
  expected.fec.packets = 1;
  callback.Matches(5, kSsrc1, expected);

  receive_statistics_->RegisterRtpStatisticsCallback(NULL);

  // New stats, but callback should not be called.
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(5);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1 + kHeaderLength,
                                      true);
  callback.Matches(5, kSsrc1, expected);
}

TEST_F(ReceiveStatisticsTest, RtpCallbacksFecFirst) {
  RtpTestCallback callback;
  receive_statistics_->RegisterRtpStatisticsCallback(&callback);

  const uint32_t kHeaderLength = 20;
  header1_.headerLength = kHeaderLength;

  // If first packet is FEC, ignore it.
  receive_statistics_->FecPacketReceived(header1_,
                                         kPacketSize1 + kHeaderLength);
  EXPECT_EQ(0u, callback.num_calls_);

  receive_statistics_->IncomingPacket(header1_, kPacketSize1 + kHeaderLength,
                                      false);
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = kPacketSize1;
  expected.transmitted.header_bytes = kHeaderLength;
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.fec.packets = 0;
  callback.Matches(1, kSsrc1, expected);

  receive_statistics_->FecPacketReceived(header1_,
                                         kPacketSize1 + kHeaderLength);
  expected.fec.payload_bytes = kPacketSize1;
  expected.fec.header_bytes = kHeaderLength;
  expected.fec.packets = 1;
  callback.Matches(2, kSsrc1, expected);
}

}  // namespace
}  // namespace webrtc
