/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"

#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/simulated_network.h"

namespace webrtc {
namespace {

constexpr DataSize kPacketSize = DataSize::Bytes(1000);
using ::testing::MockFunction;
using ::testing::SizeIs;

TEST(CcFeedbackGeneratorTest, BasicFeedback) {
  // Link capacity of 1000Kbps means it will take 1000*8/1000 = 8ms to send one
  // packet through the narrow section + 25ms fixed one way delay.
  // With perfect pacing, smoothed_rtt should be 58ms.
  // With a send rate of 500Kbps, where each packet is 1000bytes, one packet is
  // sent every 16ms.
  // The first feedback packet is expected to be sent as soon as the first
  // packet has been delivered and received 25ms later. At that time, there
  // should be 3 packets in flight (3*16=48ms).
  SimulatedClock clock(Timestamp::Seconds(1234));
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25,
                          .link_capacity = DataRate::KilobitsPerSec(1000)},
       .time_between_feedback = TimeDelta::Millis(50)});

  TransportPacketsFeedback feedback_1 =
      feedback_generator.ProcessUntilNextFeedback(
          /*send_rate=*/DataRate::KilobitsPerSec(500), clock, nullptr);

  EXPECT_EQ(feedback_1.feedback_time, clock.CurrentTime());
  EXPECT_EQ(feedback_1.data_in_flight, 3 * kPacketSize);
  ASSERT_THAT(feedback_1.packet_feedbacks, SizeIs(1));
  EXPECT_EQ(feedback_1.packet_feedbacks[0].arrival_time_offset,
            TimeDelta::Zero());
  for (const PacketResult& packet : feedback_1.packet_feedbacks) {
    EXPECT_EQ((packet.receive_time - packet.sent_packet.send_time),
              TimeDelta::Millis(25 + 8));
    EXPECT_EQ(packet.sent_packet.size, kPacketSize);
    EXPECT_EQ(packet.ecn, EcnMarking::kEct1);
  }

  TransportPacketsFeedback feedback_2 =
      feedback_generator.ProcessUntilNextFeedback(
          /*send_rate=*/DataRate::KilobitsPerSec(500), clock, nullptr);
  EXPECT_EQ((feedback_2.feedback_time - feedback_1.feedback_time).ms(), 50);
}

TEST(CcFeedbackGeneratorTest, CeMarksPacketsIfSendRateIsTooHigh) {
  SimulatedClock clock(Timestamp::Seconds(1234));
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25,
                          .link_capacity = DataRate::KilobitsPerSec(1000)}});

  int number_of_ce_marks = 0;
  for (int i = 0; i < 5; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            /*send_rate=*/DataRate::KilobitsPerSec(1100), clock, nullptr);
    number_of_ce_marks += CcFeedbackGenerator::CountCeMarks(feedback);
  }
  EXPECT_GE(number_of_ce_marks, 2);
}

TEST(CcFeedbackGeneratorTest, NoCeMarksIfSendRateIsBelowLinkCapacity) {
  SimulatedClock clock(Timestamp::Seconds(1234));
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25,
                          .link_capacity = DataRate::KilobitsPerSec(1000)},
       .time_between_feedback = TimeDelta::Millis(25)});

  int number_of_ce_marks = 0;
  for (int i = 0; i < 5; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            /*send_rate=*/DataRate::KilobitsPerSec(1000), clock, nullptr);
    number_of_ce_marks += CcFeedbackGenerator::CountCeMarks(feedback);
  }
  EXPECT_EQ(number_of_ce_marks, 0);
}

TEST(CcFeedbackGeneratorTest, DropPacketsIfSendRateIsTooHigh) {
  SimulatedClock clock(Timestamp::Seconds(1234));
  SimulatedNetwork::Config network_config = {
      .queue_length_packets = 1,  // Only one packet is allowed to be queued
      .queue_delay_ms = 25,
      .link_capacity = DataRate::KilobitsPerSec(1000),
  };
  CcFeedbackGenerator feedback_generator(
      {.network_config = network_config, .send_as_ect1 = false});

  int number_of_lost_packets = 0;
  for (int i = 0; i < 5; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            /*send_rate=*/DataRate::KilobitsPerSec(1100), clock, nullptr);
    number_of_lost_packets += feedback.LostWithSendInfo().size();
  }
  EXPECT_GE(number_of_lost_packets, 2);
}

TEST(CcFeedbackGeneratorTest, InvokesPacketSentCallbackWithDataInflight) {
  SimulatedClock clock(Timestamp::Seconds(1234));
  SimulatedNetwork::Config network_config = {};
  CcFeedbackGenerator feedback_generator({
      .network_config = network_config,
  });
  MockFunction<void(const SentPacket&)> packet_sent_cb;

  DataSize last_data_in_flight;
  EXPECT_CALL(packet_sent_cb, Call)
      .WillRepeatedly([&](const SentPacket& packet) {
        last_data_in_flight = packet.data_in_flight;
      });
  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(
          /*send_rate=*/DataRate::KilobitsPerSec(1000), clock,
          packet_sent_cb.AsStdFunction());
  // After feedback, data in flight should be less than after sending last
  // packet.
  EXPECT_GT(last_data_in_flight, feedback.data_in_flight);
}

}  // namespace
}  // namespace webrtc
