/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>
#include <random>
#include <vector>

#include "modules/congestion_controller/rtp/send_time_history.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

static const int kDefaultHistoryLengthMs = 1000;

class SendTimeHistoryTest : public ::testing::Test {
 protected:
  SendTimeHistoryTest()
      : clock_(0), history_(&clock_, kDefaultHistoryLengthMs) {}
  ~SendTimeHistoryTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}

  void AddPacketWithSendTime(uint16_t sequence_number,
                             size_t length,
                             int64_t send_time_ms,
                             const PacedPacketInfo& pacing_info) {
    PacketFeedback packet(clock_.TimeInMilliseconds(), sequence_number, length,
                          0, 0, pacing_info);
    history_.AddAndRemoveOld(packet);
    history_.OnSentPacket(sequence_number, send_time_ms);
  }

  webrtc::SimulatedClock clock_;
  SendTimeHistory history_;
};

TEST_F(SendTimeHistoryTest, SaveAndRestoreNetworkId) {
  const PacedPacketInfo kPacingInfo(0, 5, 1200);
  uint16_t sequence_number = 0;
  int64_t now_ms = clock_.TimeInMilliseconds();
  for (int i = 1; i < 5; ++i) {
    PacketFeedback packet(now_ms, sequence_number, 1000, i, i - 1, kPacingInfo);
    history_.AddAndRemoveOld(packet);
    history_.OnSentPacket(sequence_number, now_ms);
    PacketFeedback restored(now_ms, sequence_number);
    EXPECT_TRUE(history_.GetFeedback(&restored, sequence_number++));
    EXPECT_EQ(packet.local_net_id, restored.local_net_id);
    EXPECT_EQ(packet.remote_net_id, restored.remote_net_id);
  }
}

TEST_F(SendTimeHistoryTest, AddRemoveOne) {
  const uint16_t kSeqNo = 10;
  // TODO(philipel): Fix PacedPacketInfo constructor?
  const PacedPacketInfo kPacingInfo(0, 5, 1200);
  const PacketFeedback kSentPacket(0, 1, kSeqNo, 1, kPacingInfo);
  AddPacketWithSendTime(kSeqNo, 1, 1, kPacingInfo);

  PacketFeedback received_packet(0, 0, kSeqNo, 0, kPacingInfo);
  EXPECT_TRUE(history_.GetFeedback(&received_packet, false));
  EXPECT_EQ(kSentPacket, received_packet);

  PacketFeedback received_packet2(0, 0, kSeqNo, 0, kPacingInfo);
  EXPECT_TRUE(history_.GetFeedback(&received_packet2, true));
  EXPECT_EQ(kSentPacket, received_packet2);

  PacketFeedback received_packet3(0, 0, kSeqNo, 0, kPacingInfo);
  EXPECT_FALSE(history_.GetFeedback(&received_packet3, true));
}

TEST_F(SendTimeHistoryTest, GetPacketReturnsSentPacket) {
  const uint16_t kSeqNo = 10;
  const PacedPacketInfo kPacingInfo(0, 5, 1200);
  const PacketFeedback kSentPacket(0, -1, 1, kSeqNo, 123, 0, 0, kPacingInfo);
  AddPacketWithSendTime(kSeqNo, 123, 1, kPacingInfo);
  auto sent_packet = history_.GetPacket(kSeqNo);
  EXPECT_EQ(kSentPacket, *sent_packet);
}

TEST_F(SendTimeHistoryTest, GetPacketEmptyForRemovedPacket) {
  const uint16_t kSeqNo = 10;
  const PacedPacketInfo kPacingInfo(0, 5, 1200);
  AddPacketWithSendTime(kSeqNo, 123, 1, kPacingInfo);
  auto sent_packet = history_.GetPacket(kSeqNo);
  PacketFeedback received_packet(0, 0, kSeqNo, 0, kPacingInfo);
  EXPECT_TRUE(history_.GetFeedback(&received_packet, true));
  sent_packet = history_.GetPacket(kSeqNo);
  EXPECT_FALSE(sent_packet.has_value());
}

TEST_F(SendTimeHistoryTest, PopulatesExpectedFields) {
  const uint16_t kSeqNo = 10;
  const int64_t kSendTime = 1000;
  const int64_t kReceiveTime = 2000;
  const size_t kPayloadSize = 42;
  const PacedPacketInfo kPacingInfo(3, 10, 1212);

  AddPacketWithSendTime(kSeqNo, kPayloadSize, kSendTime, kPacingInfo);

  PacketFeedback packet_feedback(kReceiveTime, kSeqNo);
  EXPECT_TRUE(history_.GetFeedback(&packet_feedback, true));
  EXPECT_EQ(kReceiveTime, packet_feedback.arrival_time_ms);
  EXPECT_EQ(kSendTime, packet_feedback.send_time_ms);
  EXPECT_EQ(kSeqNo, packet_feedback.sequence_number);
  EXPECT_EQ(kPayloadSize, packet_feedback.payload_size);
  EXPECT_EQ(kPacingInfo, packet_feedback.pacing_info);
}

TEST_F(SendTimeHistoryTest, AddThenRemoveOutOfOrder) {
  std::vector<PacketFeedback> sent_packets;
  std::vector<PacketFeedback> received_packets;
  const size_t num_items = 100;
  const size_t kPacketSize = 400;
  const size_t kTransmissionTime = 1234;
  const PacedPacketInfo kPacingInfo(1, 2, 200);
  for (size_t i = 0; i < num_items; ++i) {
    sent_packets.push_back(PacketFeedback(0, static_cast<int64_t>(i),
                                          static_cast<uint16_t>(i), kPacketSize,
                                          kPacingInfo));
    received_packets.push_back(PacketFeedback(
        static_cast<int64_t>(i) + kTransmissionTime, 0,
        static_cast<uint16_t>(i), kPacketSize, PacedPacketInfo()));
  }
  for (size_t i = 0; i < num_items; ++i) {
    PacketFeedback packet = sent_packets[i];
    packet.arrival_time_ms = PacketFeedback::kNotReceived;
    packet.send_time_ms = PacketFeedback::kNoSendTime;
    history_.AddAndRemoveOld(packet);
  }
  for (size_t i = 0; i < num_items; ++i)
    history_.OnSentPacket(sent_packets[i].sequence_number,
                          sent_packets[i].send_time_ms);
  std::shuffle(received_packets.begin(), received_packets.end(),
               std::mt19937(std::random_device()()));
  for (size_t i = 0; i < num_items; ++i) {
    PacketFeedback packet = received_packets[i];
    EXPECT_TRUE(history_.GetFeedback(&packet, false));
    PacketFeedback sent_packet = sent_packets[packet.sequence_number];
    sent_packet.arrival_time_ms = packet.arrival_time_ms;
    EXPECT_EQ(sent_packet, packet);
    EXPECT_TRUE(history_.GetFeedback(&packet, true));
  }
  for (PacketFeedback packet : sent_packets)
    EXPECT_FALSE(history_.GetFeedback(&packet, false));
}

TEST_F(SendTimeHistoryTest, HistorySize) {
  const int kItems = kDefaultHistoryLengthMs / 100;
  for (int i = 0; i < kItems; ++i) {
    clock_.AdvanceTimeMilliseconds(100);
    AddPacketWithSendTime(i, 0, i * 100, PacedPacketInfo());
  }
  for (int i = 0; i < kItems; ++i) {
    PacketFeedback packet(0, 0, static_cast<uint16_t>(i), 0, PacedPacketInfo());
    EXPECT_TRUE(history_.GetFeedback(&packet, false));
    EXPECT_EQ(i * 100, packet.send_time_ms);
  }
  clock_.AdvanceTimeMilliseconds(101);
  AddPacketWithSendTime(kItems, 0, kItems * 101, PacedPacketInfo());
  PacketFeedback packet(0, 0, 0, 0, PacedPacketInfo());
  EXPECT_FALSE(history_.GetFeedback(&packet, false));
  for (int i = 1; i < (kItems + 1); ++i) {
    PacketFeedback packet2(0, 0, static_cast<uint16_t>(i), 0,
                           PacedPacketInfo());
    EXPECT_TRUE(history_.GetFeedback(&packet2, false));
    int64_t expected_time_ms = (i == kItems) ? i * 101 : i * 100;
    EXPECT_EQ(expected_time_ms, packet2.send_time_ms);
  }
}

TEST_F(SendTimeHistoryTest, HistorySizeWithWraparound) {
  const uint16_t kMaxSeqNo = std::numeric_limits<uint16_t>::max();
  AddPacketWithSendTime(kMaxSeqNo - 2, 0, 0, PacedPacketInfo());

  clock_.AdvanceTimeMilliseconds(100);
  AddPacketWithSendTime(kMaxSeqNo - 1, 1, 100, PacedPacketInfo());

  clock_.AdvanceTimeMilliseconds(100);
  AddPacketWithSendTime(kMaxSeqNo, 0, 200, PacedPacketInfo());

  clock_.AdvanceTimeMilliseconds(kDefaultHistoryLengthMs - 200 + 1);
  AddPacketWithSendTime(0, 0, kDefaultHistoryLengthMs, PacedPacketInfo());

  PacketFeedback packet(0, static_cast<uint16_t>(kMaxSeqNo - 2));
  EXPECT_FALSE(history_.GetFeedback(&packet, false));
  PacketFeedback packet2(0, static_cast<uint16_t>(kMaxSeqNo - 1));
  EXPECT_TRUE(history_.GetFeedback(&packet2, false));
  PacketFeedback packet3(0, static_cast<uint16_t>(kMaxSeqNo));
  EXPECT_TRUE(history_.GetFeedback(&packet3, false));
  PacketFeedback packet4(0, 0);
  EXPECT_TRUE(history_.GetFeedback(&packet4, false));

  // Create a gap (kMaxSeqNo - 1) -> 0.
  PacketFeedback packet5(0, kMaxSeqNo);
  EXPECT_TRUE(history_.GetFeedback(&packet5, true));

  clock_.AdvanceTimeMilliseconds(100);
  AddPacketWithSendTime(1, 0, 1100, PacedPacketInfo());

  PacketFeedback packet6(0, static_cast<uint16_t>(kMaxSeqNo - 2));
  EXPECT_FALSE(history_.GetFeedback(&packet6, false));
  PacketFeedback packet7(0, static_cast<uint16_t>(kMaxSeqNo - 1));
  EXPECT_FALSE(history_.GetFeedback(&packet7, false));
  PacketFeedback packet8(0, kMaxSeqNo);
  EXPECT_FALSE(history_.GetFeedback(&packet8, false));
  PacketFeedback packet9(0, 0);
  EXPECT_TRUE(history_.GetFeedback(&packet9, false));
  PacketFeedback packet10(0, 1);
  EXPECT_TRUE(history_.GetFeedback(&packet10, false));
}

TEST_F(SendTimeHistoryTest, InterlievedGetAndRemove) {
  const uint16_t kSeqNo = 1;
  const int64_t kTimestamp = 2;
  const PacedPacketInfo kPacingInfo1(1, 1, 100);
  const PacedPacketInfo kPacingInfo2(2, 2, 200);
  const PacedPacketInfo kPacingInfo3(3, 3, 300);
  PacketFeedback packets[3] = {
      {0, kTimestamp, kSeqNo, 0, kPacingInfo1},
      {0, kTimestamp + 1, kSeqNo + 1, 0, kPacingInfo2},
      {0, kTimestamp + 2, kSeqNo + 2, 0, kPacingInfo3}};

  AddPacketWithSendTime(packets[0].sequence_number, packets[0].payload_size,
                        packets[0].send_time_ms, packets[0].pacing_info);
  AddPacketWithSendTime(packets[1].sequence_number, packets[1].payload_size,
                        packets[1].send_time_ms, packets[1].pacing_info);
  PacketFeedback packet(0, 0, packets[0].sequence_number, 0, PacedPacketInfo());
  EXPECT_TRUE(history_.GetFeedback(&packet, true));
  EXPECT_EQ(packets[0], packet);

  AddPacketWithSendTime(packets[2].sequence_number, packets[2].payload_size,
                        packets[2].send_time_ms, packets[2].pacing_info);

  PacketFeedback packet2(0, 0, packets[1].sequence_number, 0, kPacingInfo1);
  EXPECT_TRUE(history_.GetFeedback(&packet2, true));
  EXPECT_EQ(packets[1], packet2);

  PacketFeedback packet3(0, 0, packets[2].sequence_number, 0, kPacingInfo2);
  EXPECT_TRUE(history_.GetFeedback(&packet3, true));
  EXPECT_EQ(packets[2], packet3);
}
}  // namespace test
}  // namespace webrtc
