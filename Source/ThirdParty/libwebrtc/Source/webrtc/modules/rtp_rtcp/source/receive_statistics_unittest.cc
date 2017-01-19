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

#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

const size_t kPacketSize1 = 100;
const size_t kPacketSize2 = 300;
const uint32_t kSsrc1 = 1;
const uint32_t kSsrc2 = 2;

class ReceiveStatisticsTest : public ::testing::Test {
 public:
  ReceiveStatisticsTest() :
      clock_(0),
      receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    memset(&header1_, 0, sizeof(header1_));
    header1_.ssrc = kSsrc1;
    header1_.sequenceNumber = 100;
    memset(&header2_, 0, sizeof(header2_));
    header2_.ssrc = kSsrc2;
    header2_.sequenceNumber = 100;
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

  statistician =
      receive_statistics_->GetStatistician(kSsrc2);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(600u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  StatisticianMap statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(2u, statisticians.size());
  // Add more incoming packets and verify that they are registered in both
  // access methods.
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  statisticians[kSsrc1]->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  statisticians[kSsrc2]->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);

  receive_statistics_->GetStatistician(kSsrc1)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  receive_statistics_->GetStatistician(kSsrc2)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);
}

TEST_F(ReceiveStatisticsTest, ActiveStatisticians) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(1000);
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  StatisticianMap statisticians = receive_statistics_->GetActiveStatisticians();
  // Nothing should time out since only 1000 ms has passed since the first
  // packet came in.
  EXPECT_EQ(2u, statisticians.size());

  clock_.AdvanceTimeMilliseconds(7000);
  // kSsrc1 should have timed out.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(1u, statisticians.size());

  clock_.AdvanceTimeMilliseconds(1000);
  // kSsrc2 should have timed out.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(0u, statisticians.size());

  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  // kSsrc1 should be active again and the data counters should have survived.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(1u, statisticians.size());
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

TEST_F(ReceiveStatisticsTest, RtcpCallbacks) {
  class TestCallback : public RtcpStatisticsCallback {
   public:
    TestCallback()
        : RtcpStatisticsCallback(), num_calls_(0), ssrc_(0), stats_() {}
    virtual ~TestCallback() {}

    void StatisticsUpdated(const RtcpStatistics& statistics,
                           uint32_t ssrc) override {
      ssrc_ = ssrc;
      stats_ = statistics;
      ++num_calls_;
    }

    void CNameChanged(const char* cname, uint32_t ssrc) override {}

    uint32_t num_calls_;
    uint32_t ssrc_;
    RtcpStatistics stats_;
  } callback;

  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

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
  ++header1_.sequenceNumber;

  EXPECT_EQ(0u, callback.num_calls_);

  // Call GetStatistics, simulating a timed rtcp sender thread.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  EXPECT_EQ(1u, callback.num_calls_);
  EXPECT_EQ(callback.ssrc_, kSsrc1);
  EXPECT_EQ(statistics.cumulative_lost, callback.stats_.cumulative_lost);
  EXPECT_EQ(statistics.extended_max_sequence_number,
            callback.stats_.extended_max_sequence_number);
  EXPECT_EQ(statistics.fraction_lost, callback.stats_.fraction_lost);
  EXPECT_EQ(statistics.jitter, callback.stats_.jitter);
  EXPECT_EQ(51, statistics.fraction_lost);
  EXPECT_EQ(1u, statistics.cumulative_lost);
  EXPECT_EQ(5u, statistics.extended_max_sequence_number);
  EXPECT_EQ(4u, statistics.jitter);

  receive_statistics_->RegisterRtcpStatisticsCallback(NULL);

  // Add some more data.
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
  ++header1_.sequenceNumber;

  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  // Should not have been called after deregister.
  EXPECT_EQ(1u, callback.num_calls_);
}

class RtpTestCallback : public StreamDataCountersCallback {
 public:
  RtpTestCallback()
      : StreamDataCountersCallback(), num_calls_(0), ssrc_(0), stats_() {}
  virtual ~RtpTestCallback() {}

  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) {
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
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, false);
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
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, false);
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
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, true);
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

  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, false);
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
}  // namespace webrtc
