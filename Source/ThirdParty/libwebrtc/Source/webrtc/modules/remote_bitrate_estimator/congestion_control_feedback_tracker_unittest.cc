/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/congestion_control_feedback_tracker.h"

#include <cstdint>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/network/ecn_marking.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Property;
using ::testing::SizeIs;

RtpPacketReceived CreatePacket(Timestamp arrival_time,
                               uint16_t seq = 1,
                               rtc::EcnMarking ecn = rtc::EcnMarking::kNotEct) {
  RtpPacketReceived packet;
  packet.SetSsrc(1234);
  packet.SetSequenceNumber(seq);
  packet.set_arrival_time(arrival_time);
  packet.set_ecn(ecn);
  return packet;
}

TEST(CongestionControlFeedbackTrackerTest,
     FeedbackIncludeReceivedPacketsInSequenceNumberOrder) {
  RtpPacketReceived packet_1 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(123), /*seq =*/2);
  RtpPacketReceived packet_2 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(125), /*seq=*/1);

  CongestionControlFeedbackTracker tracker;
  tracker.ReceivedPacket(packet_1);
  tracker.ReceivedPacket(packet_2);

  Timestamp feedback_time = Timestamp::Millis(567);
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(2));
  EXPECT_THAT(
      feedback_info[0],
      AllOf(
          Field(&rtcp::CongestionControlFeedback::PacketInfo::sequence_number,
                packet_2.SequenceNumber()),
          Field(
              &rtcp::CongestionControlFeedback::PacketInfo::arrival_time_offset,
              feedback_time - packet_2.arrival_time())));
  EXPECT_THAT(
      feedback_info[1],
      AllOf(
          Field(&rtcp::CongestionControlFeedback::PacketInfo::sequence_number,
                packet_1.SequenceNumber()),
          Field(
              &rtcp::CongestionControlFeedback::PacketInfo::arrival_time_offset,
              feedback_time - packet_1.arrival_time())));
}

TEST(CongestionControlFeedbackTrackerTest,
     ReportsFirstReceivedPacketArrivalTimeButEcnFromCePacketIfDuplicate) {
  RtpPacketReceived packet_1 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(123), /*seq =*/1,
                   rtc::EcnMarking::kEct1);
  RtpPacketReceived packet_2 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/1, rtc::EcnMarking::kCe);
  RtpPacketReceived packet_3 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(126), /*seq=*/1,
      rtc::EcnMarking::kEct1);

  CongestionControlFeedbackTracker tracker;
  tracker.ReceivedPacket(packet_1);
  tracker.ReceivedPacket(packet_2);
  tracker.ReceivedPacket(packet_3);

  Timestamp feedback_time = Timestamp::Millis(567);
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_THAT(
      feedback_info[0],
      AllOf(
          Field(
              &rtcp::CongestionControlFeedback::PacketInfo::arrival_time_offset,
              feedback_time - packet_1.arrival_time()),
          Field(&rtcp::CongestionControlFeedback::PacketInfo::ecn,
                rtc::EcnMarking::kCe)));
}

TEST(CongestionControlFeedbackTrackerTest,
     FeedbackGeneratesContinouseSequenceNumbers) {
  RtpPacketReceived packet_1 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(123), /*seq =*/1);
  // Packet with sequence number 2 is lost or reordered.
  RtpPacketReceived packet_2 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/3);

  CongestionControlFeedbackTracker tracker;
  tracker.ReceivedPacket(packet_1);
  tracker.ReceivedPacket(packet_2);

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  Timestamp feedback_time = Timestamp::Millis(567);
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(3));
  EXPECT_THAT(feedback_info[0].sequence_number, 1);
  EXPECT_THAT(feedback_info[0].arrival_time_offset,
              feedback_time - packet_1.arrival_time());
  EXPECT_THAT(feedback_info[1].sequence_number, 2);
  EXPECT_THAT(feedback_info[1].arrival_time_offset, TimeDelta::MinusInfinity());
  EXPECT_THAT(feedback_info[2].sequence_number, 3);
  EXPECT_THAT(feedback_info[2].arrival_time_offset,
              feedback_time - packet_2.arrival_time());
}

TEST(CongestionControlFeedbackTrackerTest,
     FeedbackGeneratesContinouseSequenceNumbersBetweenFeedbackPackets) {
  RtpPacketReceived packet_1 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(123), /*seq =*/1);
  RtpPacketReceived packet_2 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/3);

  CongestionControlFeedbackTracker tracker;
  tracker.ReceivedPacket(packet_1);

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  Timestamp feedback_time = Timestamp::Millis(567);
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_THAT(feedback_info[0].sequence_number, 1);
  EXPECT_THAT(feedback_info[0].arrival_time_offset,
              feedback_time - packet_1.arrival_time());

  feedback_info.clear();
  feedback_time = Timestamp::Millis(678);
  tracker.ReceivedPacket(packet_2);
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(2));
  EXPECT_THAT(feedback_info[0].sequence_number, 2);
  EXPECT_THAT(feedback_info[0].arrival_time_offset, TimeDelta::MinusInfinity());
  EXPECT_THAT(feedback_info[1].sequence_number, 3);
  EXPECT_THAT(feedback_info[1].arrival_time_offset,
              feedback_time - packet_2.arrival_time());
}

TEST(CongestionControlFeedbackTrackerTest,
     FeedbackGeneratesRepeatedSequenceNumbersOnReorderingBetweenFeedback) {
  RtpPacketReceived packet_1 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(123), /*seq =*/2);
  RtpPacketReceived packet_2 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/1);
  RtpPacketReceived packet_3 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/3);

  CongestionControlFeedbackTracker tracker;
  tracker.ReceivedPacket(packet_1);

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  Timestamp feedback_time = Timestamp::Millis(567);
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_THAT(feedback_info[0].sequence_number, 2);
  EXPECT_THAT(feedback_info[0].arrival_time_offset,
              feedback_time - packet_1.arrival_time());

  feedback_info.clear();
  feedback_time = Timestamp::Millis(678);
  tracker.ReceivedPacket(packet_2);
  tracker.ReceivedPacket(packet_3);
  tracker.AddPacketsToFeedback(feedback_time, feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(3));
  EXPECT_THAT(feedback_info[0].sequence_number, 1);
  EXPECT_THAT(feedback_info[0].arrival_time_offset,
              feedback_time - packet_2.arrival_time());
  EXPECT_THAT(feedback_info[1].sequence_number, 2);
  // TODO: bugs.webrtc.org/374550342 -  This is against the spec. According to
  // the specification, we should have kept the history.
  EXPECT_THAT(feedback_info[1].arrival_time_offset, TimeDelta::MinusInfinity());
  EXPECT_THAT(feedback_info[2].sequence_number, 3);
  EXPECT_THAT(feedback_info[2].arrival_time_offset,
              feedback_time - packet_3.arrival_time());
}

}  // namespace
}  // namespace webrtc
