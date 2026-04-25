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

#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Property;
using ::testing::SizeIs;
using PacketInfo = ::webrtc::rtcp::CongestionControlFeedback::PacketInfo;

constexpr uint32_t kSsrc = 1234;

RtpPacketReceived CreatePacket(Timestamp arrival_time,
                               uint16_t seq = 1,
                               EcnMarking ecn = EcnMarking::kNotEct) {
  RtpPacketReceived packet;
  packet.SetSsrc(kSsrc);
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

  CongestionControlFeedbackTracker tracker(kSsrc);
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
  RtpPacketReceived packet_1 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(123), /*seq =*/1, EcnMarking::kEct1);
  RtpPacketReceived packet_2 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/1, EcnMarking::kCe);
  RtpPacketReceived packet_3 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(126), /*seq=*/1, EcnMarking::kEct1);

  CongestionControlFeedbackTracker tracker(kSsrc);
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
                EcnMarking::kCe)));
}

TEST(CongestionControlFeedbackTrackerTest,
     ReportsFirstArrivalTimeButEcnFromCeWhenReceivedBetweenFeedback) {
  using enum EcnMarking;
  CongestionControlFeedbackTracker tracker(kSsrc);

  RtpPacketReceived packet = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(123), /*seq=*/1, /*ecn=*/kEct1);
  tracker.ReceivedPacket(packet);
  tracker.ReceivedPacket(CreatePacket(/*arrival_time=*/Timestamp::Millis(123),
                                      /*seq=*/2, /*ecn=*/kEct1));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  Timestamp feedback_time1 = Timestamp::Millis(567);
  tracker.AddPacketsToFeedback(feedback_time1, feedback_info);
  EXPECT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 1),
                             Field(&PacketInfo::arrival_time_offset,
                                   feedback_time1 - packet.arrival_time()),
                             Field(&PacketInfo::ecn, kEct1))));

  // Re-receive packet with sequence number=1, but now with CE marking.
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(600), /*seq=*/1, /*ecn=*/kCe));

  // Expect that in the new feedbacj such packet would be re-reported with 'CE'
  // marking.
  feedback_info.clear();
  Timestamp feedback_time2 = Timestamp::Millis(700);
  tracker.AddPacketsToFeedback(Timestamp::Millis(700), feedback_info);
  EXPECT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 1),
                             Field(&PacketInfo::arrival_time_offset,
                                   feedback_time2 - packet.arrival_time()),
                             Field(&PacketInfo::ecn, kCe))));
}

TEST(CongestionControlFeedbackTrackerTest,
     FeedbackGeneratesContinouseSequenceNumbers) {
  RtpPacketReceived packet_1 =
      CreatePacket(/*arrival_time=*/Timestamp::Millis(123), /*seq =*/1);
  // Packet with sequence number 2 is lost or reordered.
  RtpPacketReceived packet_2 = CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/3);

  CongestionControlFeedbackTracker tracker(kSsrc);
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

  CongestionControlFeedbackTracker tracker(kSsrc);
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

  CongestionControlFeedbackTracker tracker(kSsrc);
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
  EXPECT_THAT(feedback_info[1].arrival_time_offset,
              feedback_time - packet_1.arrival_time());
  EXPECT_THAT(feedback_info[2].sequence_number, 3);
  EXPECT_THAT(feedback_info[2].arrival_time_offset,
              feedback_time - packet_3.arrival_time());
}

TEST(CongestionControlFeedbackTrackerTest,
     IgnoresPacketsReceivedWithTooSmallSequenceNumber) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/1065));
  // Packet with backward sequence number jump by more than '64' is ignored
  // as misordered too much.
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(130), /*seq=*/1000));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(135), feedback_info);

  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_EQ(feedback_info[0].sequence_number, 1065);
}

TEST(CongestionControlFeedbackTrackerTest,
     CreatesFeedbackForPacketsReceivedWithSmallPositiveJumpInSequenceNumber) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/1'000));
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(130), /*seq=*/1'200));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(135), feedback_info);

  ASSERT_THAT(feedback_info, SizeIs(Ge(2)));
  EXPECT_EQ(feedback_info.front().sequence_number, 1'000);
  EXPECT_EQ(feedback_info.back().sequence_number, 1'200);
}

TEST(CongestionControlFeedbackTrackerTest,
     IgnoresPacketsReceivedWithLargePositiveJumpInSequenceNumber) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(125), /*seq=*/1'000));
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(130), /*seq=*/20'000));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(135), feedback_info);

  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_EQ(feedback_info.front().sequence_number, 1'000);
}

TEST(CongestionControlFeedbackTrackerTest,
     ResumeProducingReportsAfterBackwardSequenceNumberJump) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(130), /*seq=*/10'000));

  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(140), /*seq=*/1000));
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(150), feedback_info);
  // Expect packet with sn=1000 is discarded as received way out of order.
  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_EQ(feedback_info[0].sequence_number, 10'000);

  // Continue receiving packets with smaller sequence numbers and generate
  // feedbacks. eventually feedbacks should be non-empty.
  feedback_info = {};
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(160), /*seq=*/1001));
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(170), /*seq=*/1002));
  tracker.AddPacketsToFeedback(Timestamp::Millis(180), feedback_info);
  // Due to large sequence number jump, first feedback after such jump might
  // be empty.

  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(180), /*seq=*/1003));
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(190), /*seq=*/1004));
  tracker.AddPacketsToFeedback(Timestamp::Millis(200), feedback_info);

  ASSERT_THAT(feedback_info, Not(IsEmpty()));
  EXPECT_EQ(feedback_info.back().sequence_number, 1004);
}

TEST(CongestionControlFeedbackTrackerTest,
     ResumeProducingReportsAfterForwardSequenceNumberJump) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(130), /*seq=*/1'000));

  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(140), /*seq=*/20'000));
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(150), feedback_info);
  // Expect packet with sn=20'000 is discarded as received way out of order.
  ASSERT_THAT(feedback_info, SizeIs(1));
  EXPECT_EQ(feedback_info[0].sequence_number, 1'000);

  // Continue receiving packets with larger sequence numbers and generate
  // feedbacks. eventually feedbacks should be non-empty.
  feedback_info = {};
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(160), /*seq=*/20'001));
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(170), /*seq=*/20'002));
  tracker.AddPacketsToFeedback(Timestamp::Millis(180), feedback_info);
  // Due to large sequence number jump, first feedback after such jump might
  // be empty.

  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(180), /*seq=*/20'003));
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(190), /*seq=*/20'004));
  tracker.AddPacketsToFeedback(Timestamp::Millis(200), feedback_info);

  ASSERT_THAT(feedback_info, Not(IsEmpty()));
  EXPECT_EQ(feedback_info.back().sequence_number, 20'004);
}

TEST(CongestionControlFeedbackTrackerTest,
     DoesntResetStateOnPeriodsOfInactivity) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(130), /*seq=*/1'000));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(140), feedback_info);
  ASSERT_THAT(feedback_info, Not(IsEmpty()));

  feedback_info = {};
  tracker.AddPacketsToFeedback(Timestamp::Millis(150), feedback_info);
  EXPECT_THAT(feedback_info, IsEmpty());

  tracker.AddPacketsToFeedback(Timestamp::Millis(160), feedback_info);
  EXPECT_THAT(feedback_info, IsEmpty());

  tracker.ReceivedPacket(CreatePacket(
      /*arrival_time=*/Timestamp::Millis(170), /*seq=*/998));
  tracker.AddPacketsToFeedback(Timestamp::Millis(180), feedback_info);
  ASSERT_THAT(feedback_info, SizeIs(3));
  EXPECT_EQ(feedback_info[0].sequence_number, 998);
  EXPECT_EQ(feedback_info[2].sequence_number, 1000);
}

TEST(CongestionControlFeedbackTrackerTest,
     AccumulatesTotalNumberOfReportedLostPackets) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(100), /*seq=*/1));
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(110), /*seq=*/5));

  // Until reported in a feedback, missed packets are not counted as lost.
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 0);

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(120), feedback_info);
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 3);  // seq = [2,3,4]

  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(130), /*seq=*/8));
  tracker.AddPacketsToFeedback(Timestamp::Millis(140), feedback_info);
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 5);  //  [2,3,4,6,7]
}

TEST(CongestionControlFeedbackTrackerTest,
     RecoveredPacketsDoesntDecreaseNumberOfLostPackets) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(100), /*seq=*/1));
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(110), /*seq=*/5));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(120), feedback_info);
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 3);  // seq = [2,3,4]

  // Recover packet#4, so that only packets #2 and #3 are lost, but total
  // number of reported loss stays the same.
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(130), /*seq=*/4));
  tracker.AddPacketsToFeedback(Timestamp::Millis(140), feedback_info);
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 3);
  EXPECT_EQ(tracker.GetStats().num_packets_reported_recovered, 1);
}

TEST(CongestionControlFeedbackTrackerTest, CountsOncePacketReportedLostTwice) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(100), /*seq=*/1));
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(110), /*seq=*/5));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(120), feedback_info);
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 3);  // seq = [2,3,4]

  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(130), /*seq=*/2));
  feedback_info = {};
  tracker.AddPacketsToFeedback(Timestamp::Millis(140), feedback_info);
  // Feedback includes information that packets #3 and #4 are lost.
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 3),
                             Property(&PacketInfo::received, IsFalse()))));
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 4),
                             Property(&PacketInfo::received, IsFalse()))));
  // Those losses are not counted twice.
  EXPECT_EQ(tracker.GetStats().num_packets_reported_lost, 3);
}

TEST(CongestionControlFeedbackTrackerTest,
     AccumulatesTotalNumberOfReportedRecoveredPackets) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(100), /*seq=*/1));
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(110), /*seq=*/5));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(120), feedback_info);
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 2),
                             Property(&PacketInfo::received, IsFalse()))));

  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(130), /*seq=*/2));

  // Until reported in a feedback, recovered packets are not counted.
  EXPECT_EQ(tracker.GetStats().num_packets_reported_recovered, 0);
  feedback_info = {};
  tracker.AddPacketsToFeedback(Timestamp::Millis(140), feedback_info);
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 2),
                             Property(&PacketInfo::received, IsTrue()))));
  EXPECT_EQ(tracker.GetStats().num_packets_reported_recovered, 1);

  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(150), /*seq=*/3));
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(160), /*seq=*/4));
  feedback_info = {};
  tracker.AddPacketsToFeedback(Timestamp::Millis(170), feedback_info);
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 3),
                             Property(&PacketInfo::received, IsTrue()))));
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 4),
                             Property(&PacketInfo::received, IsTrue()))));
  EXPECT_EQ(tracker.GetStats().num_packets_reported_recovered, 3);
}

TEST(CongestionControlFeedbackTrackerTest,
     CountsOncePacketReportedAsRecoveredTwice) {
  CongestionControlFeedbackTracker tracker(kSsrc);
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(100), /*seq=*/1));
  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(110), /*seq=*/5));

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> feedback_info;
  tracker.AddPacketsToFeedback(Timestamp::Millis(120), feedback_info);
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 4),
                             Property(&PacketInfo::received, IsFalse()))));

  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(130), /*seq=*/4));

  feedback_info = {};
  tracker.AddPacketsToFeedback(Timestamp::Millis(140), feedback_info);
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 4),
                             Property(&PacketInfo::received, IsTrue()))));
  // Expect packet#4 is counted as recovered.
  EXPECT_EQ(tracker.GetStats().num_packets_reported_recovered, 1);

  tracker.ReceivedPacket(CreatePacket(Timestamp::Millis(150), /*seq=*/3));
  feedback_info = {};
  tracker.AddPacketsToFeedback(Timestamp::Millis(170), feedback_info);
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 3),
                             Property(&PacketInfo::received, IsTrue()))));
  ASSERT_THAT(feedback_info,
              Contains(AllOf(Field(&PacketInfo::sequence_number, 4),
                             Property(&PacketInfo::received, IsTrue()))));

  // Expect packet#3 is counted as recovered, but packet#4 is not counted twice.
  EXPECT_EQ(tracker.GetStats().num_packets_reported_recovered, 2);
}

}  // namespace
}  // namespace webrtc
