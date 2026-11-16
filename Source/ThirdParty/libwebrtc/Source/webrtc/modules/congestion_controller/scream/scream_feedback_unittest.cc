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
#include "modules/congestion_controller/scream/scream_v2_parameters.h"
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
  EXPECT_FALSE(parsed.delay_metrics.has_value());
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

  ScreamV2Parameters params;
  ScreamFeedback parsed = ParseScreamFeedback(msg, params);

  EXPECT_EQ(parsed.num_received_packets, 2);
  EXPECT_EQ(parsed.num_ce_marked_packets, 1);
  // Only packet1 is not CE-marked, so size should be 1000 bytes
  EXPECT_EQ(parsed.acked_not_marked_size, DataSize::Bytes(1000));

  // Packet 1 one-way delay = 950 - 900 = 50ms
  // Packet 2 one-way delay = 970 - 910 = 60ms
  // Both packets are within the 25ms tail window.
  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(50));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(60));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay -
                parsed.delay_metrics->min_one_way_delay,
            TimeDelta::Millis(10));

  // hold_time = last_packet.receive_time + offset - first_packet.receive_time
  // hold_time = 970 + 10 - 950 = 30ms
  EXPECT_EQ(parsed.delay_metrics->feedback_hold_time, TimeDelta::Millis(30));

  // rtt_sample = feedback_time - last_packet.send_time -
  // last_packet.arrival_time_offset rtt_sample = 1050 - 910 - 10 = 130ms
  EXPECT_EQ(parsed.delay_metrics->rtt_sample, TimeDelta::Millis(130));
}

TEST(ScreamFeedbackTest, IgnoresDelayOfPacketsSentBeforeTailWindow) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1200);

  // Packet 1 sent at 900ms, received at 950ms (owd = 50ms).
  PacketResult packet1;
  packet1.sent_packet.send_time = Timestamp::Millis(900);
  packet1.receive_time = Timestamp::Millis(950);

  // Packet 2 sent at 950ms (50ms > 25ms burst window), received at 1020ms
  // (owd = 70ms).
  PacketResult packet2;
  packet2.sent_packet.send_time = Timestamp::Millis(950);
  packet2.receive_time = Timestamp::Millis(1020);

  msg.packet_feedbacks.push_back(packet1);
  msg.packet_feedbacks.push_back(packet2);

  // With default 25ms threshold and 50ms gap > 3ms max gap, packet 1 is
  // excluded from the tail window.
  ScreamV2Parameters params;
  ScreamFeedback parsed = ParseScreamFeedback(msg, params);
  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(70));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(70));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay -
                parsed.delay_metrics->min_one_way_delay,
            TimeDelta::Zero());
}

TEST(ScreamFeedbackTest, ExtendsBurstWindowIfConsecutivePacketsWithinGap) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1200);

  // Packet 1 sent at 0ms (owd = 50ms).
  PacketResult p1;
  p1.sent_packet.send_time = Timestamp::Millis(0);
  p1.receive_time = Timestamp::Millis(50);

  // Packet 2 sent at 1ms (owd = 52ms).
  PacketResult p2;
  p2.sent_packet.send_time = Timestamp::Millis(1);
  p2.receive_time = Timestamp::Millis(53);

  // Packet 3 sent at 26ms (owd = 55ms).
  PacketResult p3;
  p3.sent_packet.send_time = Timestamp::Millis(26);
  p3.receive_time = Timestamp::Millis(81);

  msg.packet_feedbacks = {p1, p2, p3};

  // Last packet is at 26ms.
  // Packet 2 (1ms) is within burst_window_min (25ms).
  // Packet 1 (0ms) is 26ms from the end (> 25ms, <= 50ms), and the gap to
  // Packet 2 is 1ms (<= 3ms burst_window_max_gap). It should be included.
  ScreamV2Parameters params;
  ScreamFeedback parsed = ParseScreamFeedback(msg, params);
  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(50));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(55));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay -
                parsed.delay_metrics->min_one_way_delay,
            TimeDelta::Millis(5));
}

TEST(ScreamFeedbackTest, CapsBurstWindowAtMaxWindow) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1200);

  // Continuously paced packets every 2ms from 0ms to 100ms.
  for (int t = 0; t <= 100; t += 2) {
    PacketResult p;
    p.sent_packet.send_time = Timestamp::Millis(t);
    // Earlier packets (< 50ms) have delay 10ms, tail packets have delay 60ms.
    p.receive_time = Timestamp::Millis(t + (t < 50 ? 10 : 60));
    msg.packet_feedbacks.push_back(p);
  }

  // Last packet is at 100ms.
  // With burst_window_max = 50ms, window cannot extend before 50ms.
  // All packets in [50ms, 100ms] have delay 60ms. Packets < 50ms have delay
  // 10ms.
  ScreamV2Parameters params;
  ScreamFeedback parsed = ParseScreamFeedback(msg, params);
  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(60));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(60));
}

TEST(ScreamFeedbackTest,
     RttAndOwdIncreaseIfDelayIncreasesWithinFeedbackMessage) {
  ScreamV2Parameters params;

  // A feedback message where delay increases from early to late packets:
  // - Early packet sent 200ms before the tail had low delay (50ms).
  // - Mid and late packets sent within the tail burst window had increased
  //   delay (130ms and 150ms).
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1350);

  PacketResult early_packet;
  early_packet.sent_packet.send_time = Timestamp::Millis(1000);
  early_packet.receive_time = Timestamp::Millis(1050);  // owd = 50ms

  PacketResult mid_packet;
  mid_packet.sent_packet.send_time = Timestamp::Millis(1190);
  mid_packet.receive_time = Timestamp::Millis(1320);  // owd = 130ms

  PacketResult late_packet;
  late_packet.sent_packet.send_time = Timestamp::Millis(1200);
  late_packet.receive_time = Timestamp::Millis(1350);  // owd = 150ms

  msg.packet_feedbacks = {early_packet, mid_packet, late_packet};

  ScreamFeedback parsed = ParseScreamFeedback(msg, params);

  // Because early_packet is outside the tail burst window (sent 200ms before
  // late_packet), its 50ms delay is excluded. One-way delay and RTT sample
  // reflect the increased delay in the recent burst.
  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(130));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(150));
  EXPECT_EQ(parsed.delay_metrics->rtt_sample, TimeDelta::Millis(150));
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
  packet2.sent_packet.send_time = Timestamp::Millis(850);
  packet2.receive_time = Timestamp::Millis(900);
  packet2.reported_recovered_for_the_first_time = true;

  msg.packet_feedbacks.push_back(packet1);
  msg.packet_feedbacks.push_back(packet2);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  EXPECT_EQ(parsed.num_lost_packets, 1);
  EXPECT_EQ(parsed.num_recovered_packets, 1);
}

TEST(ScreamFeedbackTest, ParsesNegativeOneWayDelay) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1050);

  PacketResult packet1;
  packet1.sent_packet.send_time = Timestamp::Millis(1000);
  packet1.receive_time = Timestamp::Millis(900);  // -100ms OWD
  packet1.sent_packet.size = DataSize::Bytes(1000);

  PacketResult packet2;
  packet2.sent_packet.send_time = Timestamp::Millis(1000);
  packet2.receive_time = Timestamp::Millis(920);  // -80ms OWD
  packet2.sent_packet.size = DataSize::Bytes(1000);

  msg.packet_feedbacks.push_back(packet1);
  msg.packet_feedbacks.push_back(packet2);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(-100));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(-80));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay -
                parsed.delay_metrics->min_one_way_delay,
            TimeDelta::Millis(20));
}

TEST(ScreamFeedbackTest, ExcludesAmbiguousReceiveTimePacketsFromDelayAndRtt) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1050);

  // Packet 1: Unambiguous
  PacketResult packet1;
  packet1.sent_packet.send_time = Timestamp::Millis(900);
  packet1.receive_time = Timestamp::Millis(950);  // 50ms OWD
  packet1.arrival_time_offset = TimeDelta::Millis(10);
  packet1.sent_packet.size = DataSize::Bytes(1000);
  packet1.ambiguous_receive_time = false;

  // Packet 2: Ambiguous (e.g. retransmission without RTX) with corrupted OWD
  PacketResult packet2;
  packet2.sent_packet.send_time = Timestamp::Millis(910);
  packet2.receive_time = Timestamp::Millis(780);  // -130ms OWD
  packet2.arrival_time_offset = TimeDelta::Millis(200);
  packet2.sent_packet.size = DataSize::Bytes(1200);
  packet2.ambiguous_receive_time = true;

  msg.packet_feedbacks.push_back(packet1);
  msg.packet_feedbacks.push_back(packet2);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  // Both packets are counted as received and bytes in flight are reduced.
  EXPECT_EQ(parsed.num_received_packets, 2);
  EXPECT_EQ(parsed.received, DataSize::Bytes(2200));

  // Delay and RTT ignore the ambiguous packet and use the unambiguous packet.
  ASSERT_TRUE(parsed.delay_metrics.has_value());
  EXPECT_EQ(parsed.delay_metrics->min_one_way_delay, TimeDelta::Millis(50));
  EXPECT_EQ(parsed.delay_metrics->max_one_way_delay, TimeDelta::Millis(50));
  // rtt_sample = feedback_time (1050) - packet1.send_time (900) - packet1.ato
  // (10) = 140ms
  EXPECT_EQ(parsed.delay_metrics->rtt_sample, TimeDelta::Millis(140));
  EXPECT_EQ(parsed.delay_metrics->last_packet_receive_time,
            Timestamp::Millis(950));
  EXPECT_EQ(parsed.delay_metrics->feedback_hold_time, TimeDelta::Millis(10));
}

TEST(ScreamFeedbackTest, AllPacketsAmbiguousLeavesDelayAndRttUnset) {
  TransportPacketsFeedback msg;
  msg.feedback_time = Timestamp::Millis(1050);

  PacketResult packet;
  packet.sent_packet.send_time = Timestamp::Millis(900);
  packet.receive_time = Timestamp::Millis(770);
  packet.arrival_time_offset = TimeDelta::Millis(200);
  packet.sent_packet.size = DataSize::Bytes(1000);
  packet.ambiguous_receive_time = true;

  msg.packet_feedbacks.push_back(packet);

  ScreamFeedback parsed = ParseScreamFeedback(msg);

  EXPECT_EQ(parsed.num_received_packets, 1);
  EXPECT_EQ(parsed.received, DataSize::Bytes(1000));
  EXPECT_FALSE(parsed.delay_metrics.has_value());
}

}  // namespace
}  // namespace webrtc
