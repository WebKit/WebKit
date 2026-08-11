/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_feedback.h"

#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ScreamFeedbackTest, ParsesEmptyFeedback) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1000);
  msg.data_in_flight = DataSize::Bytes(5000);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  EXPECT_EQ(parsed.feedback_time, Timestamp::Millis(1000));
  EXPECT_EQ(parsed.data_in_flight, DataSize::Bytes(5000));
  EXPECT_EQ(parsed.num_received_packets, 0);
  EXPECT_EQ(parsed.num_ce_marked_packets, 0);
  EXPECT_EQ(parsed.acked_not_marked_size, DataSize::Zero());
  EXPECT_EQ(parsed.min_one_way_delay, TimeDelta::PlusInfinity());
  EXPECT_EQ(parsed.max_one_way_delay, TimeDelta::Zero());
  EXPECT_EQ(parsed.feedback_hold_time, TimeDelta::Zero());
  EXPECT_EQ(parsed.rtt_sample, TimeDelta::Zero());
  EXPECT_EQ(parsed.num_lost_packets, 0);
  EXPECT_EQ(parsed.num_recovered_packets, 0);
}

TEST(ScreamFeedbackTest, ParsesReceivedPacketsMetrics) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1050);
  msg.data_in_flight = DataSize::Bytes(2000);

  // Packet 1: Received, Not CE-marked
  PacketResult packet1;
  packet1.sent_packet.size = DataSize::Bytes(1000);
  packet1.sent_packet.send_time = Timestamp::Millis(900);
  packet1.receive_time = Timestamp::Millis(950);
  packet1.arrival_time_offset = TimeDelta::Millis(5);
  packet1.ecn = EcnMarking::kNotEct;

  // Packet 2: Received, CE-marked
  PacketResult packet2;
  packet2.sent_packet.size = DataSize::Bytes(1200);
  packet2.sent_packet.send_time = Timestamp::Millis(910);
  packet2.receive_time = Timestamp::Millis(970);
  packet2.arrival_time_offset = TimeDelta::Millis(10);
  packet2.ecn = EcnMarking::kCe;

  msg.packet_feedbacks.push_back(packet1);
  msg.packet_feedbacks.push_back(packet2);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  EXPECT_EQ(parsed.num_received_packets, 2);
  EXPECT_EQ(parsed.num_ce_marked_packets, 1);
  // Only packet1 is not CE-marked, so size should be 1000 bytes
  EXPECT_EQ(parsed.acked_not_marked_size, DataSize::Bytes(1000));

  // Packet 1 one-way delay = 950 - 900 = 50ms
  // Packet 2 one-way delay = 970 - 910 = 60ms
  EXPECT_EQ(parsed.min_one_way_delay, TimeDelta::Millis(50));
  EXPECT_EQ(parsed.max_one_way_delay, TimeDelta::Millis(60));

  // hold_time = last_packet.receive_time + offset - first_packet.receive_time
  // hold_time = 970 + 10 - 950 = 30ms
  EXPECT_EQ(parsed.feedback_hold_time, TimeDelta::Millis(30));

  // rtt_sample = feedback_time - last_packet.send_time -
  // last_packet.arrival_time_offset rtt_sample = 1050 - 910 - 10 = 130ms
  EXPECT_EQ(parsed.rtt_sample, TimeDelta::Millis(130));
}

TEST(ScreamFeedbackTest, ParsesLostAndRecoveredPackets) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1000);

  // Packet 1: Lost for the first time
  PacketResult packet1;
  packet1.receive_time = Timestamp::PlusInfinity();
  packet1.reported_lost_for_the_first_time = true;

  // Packet 2: Recovered for the first time
  PacketResult packet2;
  packet2.receive_time = Timestamp::Millis(900);
  packet2.reported_recovered_for_the_first_time = true;

  msg.packet_feedbacks.push_back(packet1);
  msg.packet_feedbacks.push_back(packet2);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  EXPECT_EQ(parsed.num_lost_packets, 1);
  EXPECT_EQ(parsed.num_recovered_packets, 1);
}

}  // namespace
}  // namespace webrtc
