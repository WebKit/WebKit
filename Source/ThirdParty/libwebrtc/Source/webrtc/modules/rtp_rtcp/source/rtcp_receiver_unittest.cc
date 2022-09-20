/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_receiver.h"

#include <memory>
#include <set>
#include <utility>

#include "api/array_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_bitrate_allocator.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/mocks/mock_rtcp_bandwidth_observer.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/fir.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtcp_packet/pli.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rapid_resync_request.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using rtcp::ReceiveTimeInfo;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

class MockRtcpPacketTypeCounterObserver : public RtcpPacketTypeCounterObserver {
 public:
  MOCK_METHOD(void,
              RtcpPacketTypesCounterUpdated,
              (uint32_t, const RtcpPacketTypeCounter&),
              (override));
};

class MockRtcpIntraFrameObserver : public RtcpIntraFrameObserver {
 public:
  MOCK_METHOD(void, OnReceivedIntraFrameRequest, (uint32_t), (override));
};

class MockRtcpLossNotificationObserver : public RtcpLossNotificationObserver {
 public:
  ~MockRtcpLossNotificationObserver() override = default;
  MOCK_METHOD(void,
              OnReceivedLossNotification,
              (uint32_t ssrc,
               uint16_t seq_num_of_last_decodable,
               uint16_t seq_num_of_last_received,
               bool decodability_flag),
              (override));
};

class MockCnameCallbackImpl : public RtcpCnameCallback {
 public:
  MOCK_METHOD(void, OnCname, (uint32_t, absl::string_view), (override));
};

class MockReportBlockDataObserverImpl : public ReportBlockDataObserver {
 public:
  MOCK_METHOD(void, OnReportBlockDataUpdated, (ReportBlockData), (override));
};

class MockTransportFeedbackObserver : public TransportFeedbackObserver {
 public:
  MOCK_METHOD(void, OnAddPacket, (const RtpPacketSendInfo&), (override));
  MOCK_METHOD(void,
              OnTransportFeedback,
              (const rtcp::TransportFeedback&),
              (override));
};

class MockModuleRtpRtcp : public RTCPReceiver::ModuleRtpRtcp {
 public:
  MOCK_METHOD(void, SetTmmbn, (std::vector<rtcp::TmmbItem>), (override));
  MOCK_METHOD(void, OnRequestSendReport, (), (override));
  MOCK_METHOD(void, OnReceivedNack, (const std::vector<uint16_t>&), (override));
  MOCK_METHOD(void,
              OnReceivedRtcpReportBlocks,
              (const ReportBlockList&),
              (override));
};

class MockVideoBitrateAllocationObserver
    : public VideoBitrateAllocationObserver {
 public:
  MOCK_METHOD(void,
              OnBitrateAllocationUpdated,
              (const VideoBitrateAllocation& allocation),
              (override));
};

// SSRC of remote peer, that sends rtcp packet to the rtcp receiver under test.
constexpr uint32_t kSenderSsrc = 0x10203;
// SSRCs of local peer, that rtcp packet addressed to.
constexpr uint32_t kReceiverMainSsrc = 0x123456;
// RtcpReceiver can accept several ssrc, e.g. regular and rtx streams.
constexpr uint32_t kReceiverExtraSsrc = 0x1234567;
// SSRCs to ignore (i.e. not configured in RtcpReceiver).
constexpr uint32_t kNotToUsSsrc = 0x654321;
constexpr uint32_t kUnknownSenderSsrc = 0x54321;

constexpr int64_t kRtcpIntervalMs = 1000;

}  // namespace

struct ReceiverMocks {
  ReceiverMocks() : clock(1335900000) {}

  SimulatedClock clock;
  // Callbacks to packet_type_counter_observer are frequent but most of the time
  // are not interesting.
  NiceMock<MockRtcpPacketTypeCounterObserver> packet_type_counter_observer;
  StrictMock<MockRtcpBandwidthObserver> bandwidth_observer;
  StrictMock<MockRtcpIntraFrameObserver> intra_frame_observer;
  StrictMock<MockRtcpLossNotificationObserver> rtcp_loss_notification_observer;
  StrictMock<MockTransportFeedbackObserver> transport_feedback_observer;
  StrictMock<MockVideoBitrateAllocationObserver> bitrate_allocation_observer;
  StrictMock<MockModuleRtpRtcp> rtp_rtcp_impl;
};

RtpRtcpInterface::Configuration DefaultConfiguration(ReceiverMocks* mocks) {
  RtpRtcpInterface::Configuration config;
  config.clock = &mocks->clock;
  config.receiver_only = false;
  config.rtcp_packet_type_counter_observer =
      &mocks->packet_type_counter_observer;
  config.bandwidth_callback = &mocks->bandwidth_observer;
  config.intra_frame_callback = &mocks->intra_frame_observer;
  config.rtcp_loss_notification_observer =
      &mocks->rtcp_loss_notification_observer;
  config.transport_feedback_callback = &mocks->transport_feedback_observer;
  config.bitrate_allocation_observer = &mocks->bitrate_allocation_observer;
  config.rtcp_report_interval_ms = kRtcpIntervalMs;
  config.local_media_ssrc = kReceiverMainSsrc;
  config.rtx_send_ssrc = kReceiverExtraSsrc;
  return config;
}

TEST(RtcpReceiverTest, BrokenPacketIsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);

  const uint8_t bad_packet[] = {0, 0, 0, 0};
  EXPECT_CALL(mocks.packet_type_counter_observer, RtcpPacketTypesCounterUpdated)
      .Times(0);
  receiver.IncomingPacket(bad_packet);
}

TEST(RtcpReceiverTest, InvalidFeedbackPacketIsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);

  // Too short feedback packet.
  const uint8_t bad_packet[] = {0x81, rtcp::Rtpfb::kPacketType, 0, 0};

  // TODO(danilchap): Add expectation RtcpPacketTypesCounterUpdated
  // is not called once parser would be adjusted to avoid that callback on
  // semi-valid packets.
  receiver.IncomingPacket(bad_packet);
}

TEST(RtcpReceiverTest, InjectSrPacket) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  EXPECT_FALSE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr));

  int64_t now = mocks.clock.TimeInMilliseconds();
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(IsEmpty()));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(IsEmpty(), _, now));
  receiver.IncomingPacket(sr.Build());

  EXPECT_TRUE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr));
}

TEST(RtcpReceiverTest, InjectSrPacketFromUnknownSender) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t now = mocks.clock.TimeInMilliseconds();
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kUnknownSenderSsrc);

  // The parser will handle report blocks in Sender Report from other than their
  // expected peer.
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(_, _, now));
  receiver.IncomingPacket(sr.Build());

  // But will not flag that he's gotten sender information.
  EXPECT_FALSE(receiver.NTP(nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr));
}

TEST(RtcpReceiverTest, InjectSrPacketCalculatesRTT) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const TimeDelta kRtt = TimeDelta::Millis(123);
  const uint32_t kDelayNtp = 0x4321;
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);

  int64_t rtt_ms = 0;
  EXPECT_EQ(-1, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));

  uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kReceiverMainSsrc);
  block.SetLastSr(sent_ntp);
  block.SetDelayLastSr(kDelayNtp);
  sr.AddReportBlock(block);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(sr.Build());

  EXPECT_EQ(0, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));
  EXPECT_NEAR(rtt_ms, kRtt.ms(), 1);
}

TEST(RtcpReceiverTest, InjectSrPacketCalculatesNegativeRTTAsOne) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const TimeDelta kRtt = TimeDelta::Millis(-13);
  const uint32_t kDelayNtp = 0x4321;
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);

  int64_t rtt_ms = 0;
  EXPECT_EQ(-1, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));

  uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kReceiverMainSsrc);
  block.SetLastSr(sent_ntp);
  block.SetDelayLastSr(kDelayNtp);
  sr.AddReportBlock(block);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(1)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(1), _, _));
  receiver.IncomingPacket(sr.Build());

  EXPECT_EQ(0, receiver.RTT(kSenderSsrc, &rtt_ms, nullptr, nullptr, nullptr));
  EXPECT_EQ(1, rtt_ms);
}

TEST(RtcpReceiverTest,
     TwoReportBlocksWithLastOneWithoutLastSrCalculatesRttForBandwidthObserver) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const TimeDelta kRtt = TimeDelta::Millis(120);
  const uint32_t kDelayNtp = 123000;
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);

  uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kReceiverMainSsrc);
  block.SetLastSr(sent_ntp);
  block.SetDelayLastSr(kDelayNtp);
  sr.AddReportBlock(block);
  block.SetMediaSsrc(kReceiverExtraSsrc);
  block.SetLastSr(0);
  sr.AddReportBlock(block);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(2)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(2), kRtt.ms(), _));
  receiver.IncomingPacket(sr.Build());
}

TEST(RtcpReceiverTest, InjectRrPacket) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t now = mocks.clock.TimeInMilliseconds();
  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(IsEmpty()));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(IsEmpty(), _, now));
  receiver.IncomingPacket(rr.Build());

  EXPECT_THAT(receiver.GetLatestReportBlockData(), IsEmpty());
}

TEST(RtcpReceiverTest, InjectRrPacketWithReportBlockNotToUsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t now = mocks.clock.TimeInMilliseconds();
  rtcp::ReportBlock rb;
  rb.SetMediaSsrc(kNotToUsSsrc);
  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  rr.AddReportBlock(rb);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(IsEmpty()));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(IsEmpty(), _, now));
  receiver.IncomingPacket(rr.Build());

  EXPECT_EQ(0, receiver.LastReceivedReportBlockMs());
  EXPECT_THAT(receiver.GetLatestReportBlockData(), IsEmpty());
}

TEST(RtcpReceiverTest, InjectRrPacketWithOneReportBlock) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t now = mocks.clock.TimeInMilliseconds();

  rtcp::ReportBlock rb;
  rb.SetMediaSsrc(kReceiverMainSsrc);
  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  rr.AddReportBlock(rb);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(1)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(1), _, now));
  receiver.IncomingPacket(rr.Build());

  EXPECT_EQ(now, receiver.LastReceivedReportBlockMs());
  EXPECT_THAT(receiver.GetLatestReportBlockData(), SizeIs(1));
}

TEST(RtcpReceiverTest, InjectSrPacketWithOneReportBlock) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t now = mocks.clock.TimeInMilliseconds();

  rtcp::ReportBlock rb;
  rb.SetMediaSsrc(kReceiverMainSsrc);
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  sr.AddReportBlock(rb);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(1)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(1), _, now));
  receiver.IncomingPacket(sr.Build());

  EXPECT_EQ(now, receiver.LastReceivedReportBlockMs());
  EXPECT_THAT(receiver.GetLatestReportBlockData(), SizeIs(1));
}

TEST(RtcpReceiverTest, InjectRrPacketWithTwoReportBlocks) {
  const uint16_t kSequenceNumbers[] = {10, 12423};
  const uint32_t kCumLost[] = {13, 555};
  const uint8_t kFracLost[] = {20, 11};
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t now = mocks.clock.TimeInMilliseconds();

  rtcp::ReportBlock rb1;
  rb1.SetMediaSsrc(kReceiverMainSsrc);
  rb1.SetExtHighestSeqNum(kSequenceNumbers[0]);
  rb1.SetFractionLost(10);

  rtcp::ReportBlock rb2;
  rb2.SetMediaSsrc(kReceiverExtraSsrc);
  rb2.SetExtHighestSeqNum(kSequenceNumbers[1]);
  rb2.SetFractionLost(0);

  rtcp::ReceiverReport rr1;
  rr1.SetSenderSsrc(kSenderSsrc);
  rr1.AddReportBlock(rb1);
  rr1.AddReportBlock(rb2);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(2)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(2), _, now));
  receiver.IncomingPacket(rr1.Build());

  EXPECT_EQ(now, receiver.LastReceivedReportBlockMs());
  EXPECT_THAT(receiver.GetLatestReportBlockData(),
              UnorderedElementsAre(
                  Property(&ReportBlockData::report_block,
                           Field(&RTCPReportBlock::fraction_lost, 0)),
                  Property(&ReportBlockData::report_block,
                           Field(&RTCPReportBlock::fraction_lost, 10))));

  // Insert next receiver report with same ssrc but new values.
  rtcp::ReportBlock rb3;
  rb3.SetMediaSsrc(kReceiverMainSsrc);
  rb3.SetExtHighestSeqNum(kSequenceNumbers[0]);
  rb3.SetFractionLost(kFracLost[0]);
  rb3.SetCumulativeLost(kCumLost[0]);

  rtcp::ReportBlock rb4;
  rb4.SetMediaSsrc(kReceiverExtraSsrc);
  rb4.SetExtHighestSeqNum(kSequenceNumbers[1]);
  rb4.SetFractionLost(kFracLost[1]);
  rb4.SetCumulativeLost(kCumLost[1]);

  rtcp::ReceiverReport rr2;
  rr2.SetSenderSsrc(kSenderSsrc);
  rr2.AddReportBlock(rb3);
  rr2.AddReportBlock(rb4);

  // Advance time to make 1st sent time and 2nd sent time different.
  mocks.clock.AdvanceTimeMilliseconds(500);
  now = mocks.clock.TimeInMilliseconds();

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(2)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(2), _, now));
  receiver.IncomingPacket(rr2.Build());

  EXPECT_THAT(
      receiver.GetLatestReportBlockData(),
      UnorderedElementsAre(
          Property(
              &ReportBlockData::report_block,
              AllOf(Field(&RTCPReportBlock::source_ssrc, kReceiverMainSsrc),
                    Field(&RTCPReportBlock::fraction_lost, kFracLost[0]),
                    Field(&RTCPReportBlock::packets_lost, kCumLost[0]),
                    Field(&RTCPReportBlock::extended_highest_sequence_number,
                          kSequenceNumbers[0]))),
          Property(
              &ReportBlockData::report_block,
              AllOf(Field(&RTCPReportBlock::source_ssrc, kReceiverExtraSsrc),
                    Field(&RTCPReportBlock::fraction_lost, kFracLost[1]),
                    Field(&RTCPReportBlock::packets_lost, kCumLost[1]),
                    Field(&RTCPReportBlock::extended_highest_sequence_number,
                          kSequenceNumbers[1])))));
}

TEST(RtcpReceiverTest,
     InjectRrPacketsFromTwoRemoteSsrcsReturnsLatestReportBlock) {
  const uint32_t kSenderSsrc2 = 0x20304;
  const uint16_t kSequenceNumbers[] = {10, 12423};
  const int32_t kCumLost[] = {13, 555};
  const uint8_t kFracLost[] = {20, 11};
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::ReportBlock rb1;
  rb1.SetMediaSsrc(kReceiverMainSsrc);
  rb1.SetExtHighestSeqNum(kSequenceNumbers[0]);
  rb1.SetFractionLost(kFracLost[0]);
  rb1.SetCumulativeLost(kCumLost[0]);
  rtcp::ReceiverReport rr1;
  rr1.SetSenderSsrc(kSenderSsrc);
  rr1.AddReportBlock(rb1);

  int64_t now = mocks.clock.TimeInMilliseconds();

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(1)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(1), _, now));
  receiver.IncomingPacket(rr1.Build());

  EXPECT_EQ(now, receiver.LastReceivedReportBlockMs());

  EXPECT_THAT(
      receiver.GetLatestReportBlockData(),
      ElementsAre(Property(
          &ReportBlockData::report_block,
          AllOf(Field(&RTCPReportBlock::source_ssrc, kReceiverMainSsrc),
                Field(&RTCPReportBlock::sender_ssrc, kSenderSsrc),
                Field(&RTCPReportBlock::fraction_lost, kFracLost[0]),
                Field(&RTCPReportBlock::packets_lost, kCumLost[0]),
                Field(&RTCPReportBlock::extended_highest_sequence_number,
                      kSequenceNumbers[0])))));

  rtcp::ReportBlock rb2;
  rb2.SetMediaSsrc(kReceiverMainSsrc);
  rb2.SetExtHighestSeqNum(kSequenceNumbers[1]);
  rb2.SetFractionLost(kFracLost[1]);
  rb2.SetCumulativeLost(kCumLost[1]);
  rtcp::ReceiverReport rr2;
  rr2.SetSenderSsrc(kSenderSsrc2);
  rr2.AddReportBlock(rb2);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks(SizeIs(1)));
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedRtcpReceiverReport(SizeIs(1), _, now));
  receiver.IncomingPacket(rr2.Build());

  EXPECT_THAT(
      receiver.GetLatestReportBlockData(),
      UnorderedElementsAre(
          Property(
              &ReportBlockData::report_block,
              AllOf(Field(&RTCPReportBlock::source_ssrc, kReceiverMainSsrc),
                    Field(&RTCPReportBlock::sender_ssrc, kSenderSsrc2),
                    Field(&RTCPReportBlock::fraction_lost, kFracLost[1]),
                    Field(&RTCPReportBlock::packets_lost, kCumLost[1]),
                    Field(&RTCPReportBlock::extended_highest_sequence_number,
                          kSequenceNumbers[1])))));
}

TEST(RtcpReceiverTest, GetRtt) {
  const uint32_t kSentCompactNtp = 0x1234;
  const uint32_t kDelayCompactNtp = 0x222;
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  // No report block received.
  EXPECT_EQ(-1, receiver.RTT(kSenderSsrc, nullptr, nullptr, nullptr, nullptr));

  rtcp::ReportBlock rb;
  rb.SetMediaSsrc(kReceiverMainSsrc);
  rb.SetLastSr(kSentCompactNtp);
  rb.SetDelayLastSr(kDelayCompactNtp);

  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  rr.AddReportBlock(rb);
  int64_t now = mocks.clock.TimeInMilliseconds();

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr.Build());

  EXPECT_EQ(now, receiver.LastReceivedReportBlockMs());
  EXPECT_EQ(0, receiver.RTT(kSenderSsrc, nullptr, nullptr, nullptr, nullptr));
}

// App packets are ignored.
TEST(RtcpReceiverTest, InjectApp) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::App app;
  app.SetSubType(30);
  app.SetName(0x17a177e);
  const uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  app.SetData(kData, sizeof(kData));

  receiver.IncomingPacket(app.Build());
}

TEST(RtcpReceiverTest, InjectSdesWithOneChunk) {
  ReceiverMocks mocks;
  MockCnameCallbackImpl callback;
  RtpRtcpInterface::Configuration config = DefaultConfiguration(&mocks);
  config.rtcp_cname_callback = &callback;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const char kCname[] = "alice@host";
  rtcp::Sdes sdes;
  sdes.AddCName(kSenderSsrc, kCname);

  EXPECT_CALL(callback, OnCname(kSenderSsrc, StrEq(kCname)));
  receiver.IncomingPacket(sdes.Build());
}

TEST(RtcpReceiverTest, InjectByePacket_RemovesReportBlocks) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::ReportBlock rb1;
  rb1.SetMediaSsrc(kReceiverMainSsrc);
  rtcp::ReportBlock rb2;
  rb2.SetMediaSsrc(kReceiverExtraSsrc);
  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(kSenderSsrc);
  rr.AddReportBlock(rb1);
  rr.AddReportBlock(rb2);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr.Build());

  EXPECT_THAT(receiver.GetLatestReportBlockData(), SizeIs(2));

  // Verify that BYE removes the report blocks.
  rtcp::Bye bye;
  bye.SetSenderSsrc(kSenderSsrc);

  receiver.IncomingPacket(bye.Build());

  EXPECT_THAT(receiver.GetLatestReportBlockData(), IsEmpty());

  // Inject packet again.
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr.Build());

  EXPECT_THAT(receiver.GetLatestReportBlockData(), SizeIs(2));
}

TEST(RtcpReceiverTest, InjectByePacketRemovesReferenceTimeInfo) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(NtpTime(0x10203, 0x40506));
  xr.SetRrtr(rrtr);
  receiver.IncomingPacket(xr.Build());

  rtcp::Bye bye;
  bye.SetSenderSsrc(kSenderSsrc);
  receiver.IncomingPacket(bye.Build());

  EXPECT_THAT(receiver.ConsumeReceivedXrReferenceTimeInfo(), IsEmpty());
}

TEST(RtcpReceiverTest, InjectPliPacket) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::Pli pli;
  pli.SetMediaSsrc(kReceiverMainSsrc);

  EXPECT_CALL(
      mocks.packet_type_counter_observer,
      RtcpPacketTypesCounterUpdated(
          kReceiverMainSsrc, Field(&RtcpPacketTypeCounter::pli_packets, 1)));
  EXPECT_CALL(mocks.intra_frame_observer,
              OnReceivedIntraFrameRequest(kReceiverMainSsrc));
  receiver.IncomingPacket(pli.Build());
}

TEST(RtcpReceiverTest, PliPacketNotToUsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::Pli pli;
  pli.SetMediaSsrc(kNotToUsSsrc);

  EXPECT_CALL(
      mocks.packet_type_counter_observer,
      RtcpPacketTypesCounterUpdated(
          kReceiverMainSsrc, Field(&RtcpPacketTypeCounter::pli_packets, 0)));
  EXPECT_CALL(mocks.intra_frame_observer, OnReceivedIntraFrameRequest).Times(0);
  receiver.IncomingPacket(pli.Build());
}

TEST(RtcpReceiverTest, InjectFirPacket) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::Fir fir;
  fir.AddRequestTo(kReceiverMainSsrc, 13);

  EXPECT_CALL(
      mocks.packet_type_counter_observer,
      RtcpPacketTypesCounterUpdated(
          kReceiverMainSsrc, Field(&RtcpPacketTypeCounter::fir_packets, 1)));
  EXPECT_CALL(mocks.intra_frame_observer,
              OnReceivedIntraFrameRequest(kReceiverMainSsrc));
  receiver.IncomingPacket(fir.Build());
}

TEST(RtcpReceiverTest, FirPacketNotToUsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::Fir fir;
  fir.AddRequestTo(kNotToUsSsrc, 13);

  EXPECT_CALL(mocks.intra_frame_observer, OnReceivedIntraFrameRequest).Times(0);
  receiver.IncomingPacket(fir.Build());
}

TEST(RtcpReceiverTest, ExtendedReportsPacketWithZeroReportBlocksIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);

  receiver.IncomingPacket(xr.Build());
}

TEST(RtcpReceiverTest, InjectExtendedReportsReceiverReferenceTimePacket) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const NtpTime kNtp(0x10203, 0x40506);
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(kNtp);
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(rrtr);

  std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis =
      receiver.ConsumeReceivedXrReferenceTimeInfo();
  EXPECT_THAT(last_xr_rtis, IsEmpty());

  receiver.IncomingPacket(xr.Build());

  last_xr_rtis = receiver.ConsumeReceivedXrReferenceTimeInfo();
  ASSERT_THAT(last_xr_rtis, SizeIs(1));
  EXPECT_EQ(kSenderSsrc, last_xr_rtis[0].ssrc);
  EXPECT_EQ(CompactNtp(kNtp), last_xr_rtis[0].last_rr);
  EXPECT_EQ(0U, last_xr_rtis[0].delay_since_last_rr);
}

TEST(RtcpReceiverTest, ExtendedReportsDlrrPacketNotToUsIgnored) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  // Allow calculate rtt using dlrr/rrtr, simulating media receiver side.
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kNotToUsSsrc, 0x12345, 0x67890));

  receiver.IncomingPacket(xr.Build());

  int64_t rtt_ms = 0;
  EXPECT_FALSE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_TRUE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_EQ(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, InjectExtendedReportsDlrrPacketWithSubBlock) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint32_t kLastRR = 0x12345;
  const uint32_t kDelay = 0x23456;
  int64_t rtt_ms = 0;
  EXPECT_FALSE(receiver.GetAndResetXrRrRtt(&rtt_ms));

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, kLastRR, kDelay));

  receiver.IncomingPacket(xr.Build());

  uint32_t compact_ntp_now = CompactNtp(mocks.clock.CurrentNtpTime());
  EXPECT_TRUE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  uint32_t rtt_ntp = compact_ntp_now - kDelay - kLastRR;
  EXPECT_NEAR(CompactNtpRttToTimeDelta(rtt_ntp).ms(), rtt_ms, 1);
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_GT(non_sender_rtt_stats.round_trip_time(), TimeDelta::Zero());
  EXPECT_FALSE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, InjectExtendedReportsDlrrPacketWithMultipleSubBlocks) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint32_t kLastRR = 0x12345;
  const uint32_t kDelay = 0x56789;

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, kLastRR, kDelay));
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc + 1, 0x12345, 0x67890));
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc + 2, 0x12345, 0x67890));

  receiver.IncomingPacket(xr.Build());

  uint32_t compact_ntp_now = CompactNtp(mocks.clock.CurrentNtpTime());
  int64_t rtt_ms = 0;
  EXPECT_TRUE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  uint32_t rtt_ntp = compact_ntp_now - kDelay - kLastRR;
  EXPECT_NEAR(CompactNtpRttToTimeDelta(rtt_ntp).ms(), rtt_ms, 1);
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_GT(non_sender_rtt_stats.round_trip_time(), TimeDelta::Zero());
  EXPECT_FALSE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, InjectExtendedReportsPacketWithMultipleReportBlocks) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::Rrtr rrtr;
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(rrtr);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, 0x12345, 0x67890));

  receiver.IncomingPacket(xr.Build());

  std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis =
      receiver.ConsumeReceivedXrReferenceTimeInfo();
  EXPECT_THAT(last_xr_rtis, SizeIs(1));
  int64_t rtt_ms = 0;
  EXPECT_TRUE(receiver.GetAndResetXrRrRtt(&rtt_ms));
}

TEST(RtcpReceiverTest, InjectExtendedReportsPacketWithUnknownReportBlock) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::Rrtr rrtr;
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(rrtr);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, 0x12345, 0x67890));

  rtc::Buffer packet = xr.Build();
  // Modify the DLRR block to have an unsupported block type, from 5 to 6.
  ASSERT_EQ(5, packet.data()[20]);
  packet.data()[20] = 6;
  receiver.IncomingPacket(packet);

  // Validate Rrtr was received and processed.
  std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis =
      receiver.ConsumeReceivedXrReferenceTimeInfo();
  EXPECT_THAT(last_xr_rtis, SizeIs(1));
  // Validate Dlrr report wasn't processed.
  int64_t rtt_ms = 0;
  EXPECT_FALSE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_TRUE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_EQ(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, TestExtendedReportsRrRttInitiallyFalse) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  int64_t rtt_ms;
  EXPECT_FALSE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_TRUE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_EQ(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, RttCalculatedAfterExtendedReportsDlrr) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  int64_t rtt_ms = 0;
  EXPECT_TRUE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  EXPECT_NEAR(kRtt.ms(), rtt_ms, 1);
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().value().IsZero());
  EXPECT_FALSE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

// Same test as above but enables receive-side RTT using the setter instead of
// the config struct.
TEST(RtcpReceiverTest, SetterEnablesReceiverRtt) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = false;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);
  receiver.SetNonSenderRttMeasurement(true);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  int64_t rtt_ms = 0;
  EXPECT_TRUE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  EXPECT_NEAR(rtt_ms, kRtt.ms(), 1);
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().value().IsZero());
  EXPECT_FALSE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

// Same test as above but disables receive-side RTT using the setter instead of
// the config struct.
TEST(RtcpReceiverTest, DoesntCalculateRttOnReceivedDlrr) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);
  receiver.SetNonSenderRttMeasurement(false);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  // We expect that no RTT is available (because receive-side RTT was disabled).
  int64_t rtt_ms = 0;
  EXPECT_FALSE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_TRUE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_EQ(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, XrDlrrCalculatesNegativeRttAsOne) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(-3600 * 1000, -1));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  int64_t rtt_ms = 0;
  EXPECT_TRUE(receiver.GetAndResetXrRrRtt(&rtt_ms));
  EXPECT_EQ(1, rtt_ms);
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().value().IsZero());
  EXPECT_FALSE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats.round_trip_time_measurements(), 0);
}

// Test receiver RTT stats with multiple measurements.
TEST(RtcpReceiverTest, ReceiverRttWithMultipleMeasurements) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  // Check that the non-sender RTT stats are valid and based on a single
  // measurement.
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_NEAR(non_sender_rtt_stats.round_trip_time()->ms(), kRtt.ms(), 1);
  EXPECT_EQ(non_sender_rtt_stats.round_trip_time_measurements(), 1);
  EXPECT_EQ(non_sender_rtt_stats.total_round_trip_time().ms(),
            non_sender_rtt_stats.round_trip_time()->ms());

  // Generate another XR report with the same RTT and delay.
  NtpTime now2 = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp2 = CompactNtp(now2);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr2;
  xr2.SetSenderSsrc(kSenderSsrc);
  xr2.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp2, kDelayNtp));

  receiver.IncomingPacket(xr2.Build());

  // Check that the non-sender RTT stats are based on 2 measurements, and that
  // the values are as expected.
  non_sender_rtt_stats = receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_NEAR(non_sender_rtt_stats.round_trip_time()->ms(), kRtt.ms(), 1);
  EXPECT_EQ(non_sender_rtt_stats.round_trip_time_measurements(), 2);
  EXPECT_NEAR(non_sender_rtt_stats.total_round_trip_time().ms(), 2 * kRtt.ms(),
              2);
}

// Test that the receiver RTT stat resets when receiving a SR without XR. This
// behavior is described in the standard, see
// https://www.w3.org/TR/webrtc-stats/#dom-rtcremoteoutboundrtpstreamstats-roundtriptime.
TEST(RtcpReceiverTest, ReceiverRttResetOnSrWithoutXr) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_NEAR(non_sender_rtt_stats.round_trip_time()->ms(), kRtt.ms(), 1);

  // Generate a SR without XR.
  rtcp::ReportBlock rb;
  rb.SetMediaSsrc(kReceiverMainSsrc);
  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  sr.AddReportBlock(rb);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);

  receiver.IncomingPacket(sr.Build());

  // Check that the non-sender RTT stat is not set.
  non_sender_rtt_stats = receiver.GetNonSenderRTT();
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().has_value());
}

// Test that the receiver RTT stat resets when receiving a DLRR with a timestamp
// of zero. This behavior is described in the standard, see
// https://www.w3.org/TR/webrtc-stats/#dom-rtcremoteoutboundrtpstreamstats-roundtriptime.
TEST(RtcpReceiverTest, ReceiverRttResetOnDlrrWithZeroTimestamp) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = true;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_NEAR(non_sender_rtt_stats.round_trip_time()->ms(), kRtt.ms(), 1);

  // Generate an XR+DLRR with zero timestamp.
  rtcp::ExtendedReports xr2;
  xr2.SetSenderSsrc(kSenderSsrc);
  xr2.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, 0, kDelayNtp));

  receiver.IncomingPacket(xr2.Build());

  // Check that the non-sender RTT stat is not set.
  non_sender_rtt_stats = receiver.GetNonSenderRTT();
  EXPECT_FALSE(non_sender_rtt_stats.round_trip_time().has_value());
}

// Check that the receiver RTT works correctly when the remote SSRC changes.
TEST(RtcpReceiverTest, ReceiverRttWithMultipleRemoteSsrcs) {
  ReceiverMocks mocks;
  auto config = DefaultConfiguration(&mocks);
  config.non_sender_rtt_measurement = false;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);
  receiver.SetNonSenderRttMeasurement(true);

  Random rand(0x0123456789abcdef);
  const TimeDelta kRtt = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);
  NtpTime now = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp = CompactNtp(now);
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp, kDelayNtp));

  receiver.IncomingPacket(xr.Build());

  // Generate an XR report for another SSRC.
  const TimeDelta kRtt2 = TimeDelta::Millis(rand.Rand(1, 9 * 3600 * 1000));
  const uint32_t kDelayNtp2 = rand.Rand(0, 0x7fffffff);
  const TimeDelta kDelay2 = CompactNtpRttToTimeDelta(kDelayNtp2);
  NtpTime now2 = mocks.clock.CurrentNtpTime();
  uint32_t sent_ntp2 = CompactNtp(now2);
  mocks.clock.AdvanceTime(kRtt2 + kDelay2);

  rtcp::ExtendedReports xr2;
  xr2.SetSenderSsrc(kSenderSsrc + 1);
  xr2.AddDlrrItem(ReceiveTimeInfo(kReceiverMainSsrc, sent_ntp2, kDelayNtp2));

  receiver.IncomingPacket(xr2.Build());

  // Check that the non-sender RTT stats match the first XR.
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats.round_trip_time().has_value());
  EXPECT_NEAR(non_sender_rtt_stats.round_trip_time()->ms(), kRtt.ms(), 1);
  EXPECT_FALSE(non_sender_rtt_stats.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats.round_trip_time_measurements(), 0);

  // Change the remote SSRC and check that the stats match the second XR.
  receiver.SetRemoteSSRC(kSenderSsrc + 1);
  RTCPReceiver::NonSenderRttStats non_sender_rtt_stats2 =
      receiver.GetNonSenderRTT();
  EXPECT_TRUE(non_sender_rtt_stats2.round_trip_time().has_value());
  EXPECT_NEAR(non_sender_rtt_stats2.round_trip_time()->ms(), kRtt2.ms(), 1);
  EXPECT_FALSE(non_sender_rtt_stats2.total_round_trip_time().IsZero());
  EXPECT_GT(non_sender_rtt_stats2.round_trip_time_measurements(), 0);
}

TEST(RtcpReceiverTest, ConsumeReceivedXrReferenceTimeInfoInitiallyEmpty) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  EXPECT_THAT(receiver.ConsumeReceivedXrReferenceTimeInfo(), IsEmpty());
}

TEST(RtcpReceiverTest, ConsumeReceivedXrReferenceTimeInfo) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const NtpTime kNtp(0x10203, 0x40506);
  const uint32_t kNtpMid = CompactNtp(kNtp);

  rtcp::Rrtr rrtr;
  rrtr.SetNtp(kNtp);
  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(rrtr);

  receiver.IncomingPacket(xr.Build());

  mocks.clock.AdvanceTimeMilliseconds(1000);

  std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis =
      receiver.ConsumeReceivedXrReferenceTimeInfo();
  ASSERT_THAT(last_xr_rtis, SizeIs(1));
  EXPECT_EQ(kSenderSsrc, last_xr_rtis[0].ssrc);
  EXPECT_EQ(kNtpMid, last_xr_rtis[0].last_rr);
  EXPECT_EQ(65536U, last_xr_rtis[0].delay_since_last_rr);
}

TEST(RtcpReceiverTest,
     ReceivedRrtrFromSameSsrcUpdatesReceivedReferenceTimeInfo) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const NtpTime kNtp1(0x10203, 0x40506);
  const NtpTime kNtp2(0x11223, 0x44556);
  const int64_t kDelayMs = 2000;

  rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  rtcp::Rrtr rrtr1;
  rrtr1.SetNtp(kNtp1);
  xr.SetRrtr(rrtr1);
  receiver.IncomingPacket(xr.Build());
  mocks.clock.AdvanceTimeMilliseconds(kDelayMs);
  rtcp::Rrtr rrtr2;
  rrtr2.SetNtp(kNtp2);
  xr.SetRrtr(rrtr2);
  receiver.IncomingPacket(xr.Build());
  mocks.clock.AdvanceTimeMilliseconds(kDelayMs);

  std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis =
      receiver.ConsumeReceivedXrReferenceTimeInfo();
  ASSERT_THAT(last_xr_rtis, SizeIs(1));
  EXPECT_EQ(kSenderSsrc, last_xr_rtis[0].ssrc);
  EXPECT_EQ(CompactNtp(kNtp2), last_xr_rtis[0].last_rr);
  EXPECT_EQ(kDelayMs * 65536 / 1000, last_xr_rtis[0].delay_since_last_rr);
}

TEST(RtcpReceiverTest, StoresLastReceivedRrtrPerSsrc) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const size_t kNumBufferedReports = 1;
  const size_t kNumReports =
      rtcp::ExtendedReports::kMaxNumberOfDlrrItems + kNumBufferedReports;
  for (size_t i = 0; i < kNumReports; ++i) {
    rtcp::ExtendedReports xr;
    xr.SetSenderSsrc(i * 100);
    rtcp::Rrtr rrtr;
    rrtr.SetNtp(NtpTime(i * 200, i * 300));
    xr.SetRrtr(rrtr);
    receiver.IncomingPacket(xr.Build());
    mocks.clock.AdvanceTimeMilliseconds(1000);
  }

  std::vector<rtcp::ReceiveTimeInfo> last_xr_rtis =
      receiver.ConsumeReceivedXrReferenceTimeInfo();
  ASSERT_THAT(last_xr_rtis,
              SizeIs(rtcp::ExtendedReports::kMaxNumberOfDlrrItems));
  for (size_t i = 0; i < rtcp::ExtendedReports::kMaxNumberOfDlrrItems; ++i) {
    EXPECT_EQ(i * 100, last_xr_rtis[i].ssrc);
    EXPECT_EQ(CompactNtp(NtpTime(i * 200, i * 300)), last_xr_rtis[i].last_rr);
    EXPECT_EQ(65536U * (kNumReports - i), last_xr_rtis[i].delay_since_last_rr);
  }

  last_xr_rtis = receiver.ConsumeReceivedXrReferenceTimeInfo();
  ASSERT_THAT(last_xr_rtis, SizeIs(kNumBufferedReports));
}

TEST(RtcpReceiverTest, ReceiveReportTimeout) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint16_t kSequenceNumber = 1234;
  mocks.clock.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);

  // No RR received, shouldn't trigger a timeout.
  EXPECT_FALSE(receiver.RtcpRrTimeout());
  EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

  // Add a RR and advance the clock just enough to not trigger a timeout.
  rtcp::ReportBlock rb1;
  rb1.SetMediaSsrc(kReceiverMainSsrc);
  rb1.SetExtHighestSeqNum(kSequenceNumber);
  rtcp::ReceiverReport rr1;
  rr1.SetSenderSsrc(kSenderSsrc);
  rr1.AddReportBlock(rb1);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr1.Build());

  mocks.clock.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs - 1);
  EXPECT_FALSE(receiver.RtcpRrTimeout());
  EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

  // Add a RR with the same extended max as the previous RR to trigger a
  // sequence number timeout, but not a RR timeout.
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr1.Build());

  mocks.clock.AdvanceTimeMilliseconds(2);
  EXPECT_FALSE(receiver.RtcpRrTimeout());
  EXPECT_TRUE(receiver.RtcpRrSequenceNumberTimeout());

  // Advance clock enough to trigger an RR timeout too.
  mocks.clock.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);
  EXPECT_TRUE(receiver.RtcpRrTimeout());

  // We should only get one timeout even though we still haven't received a new
  // RR.
  EXPECT_FALSE(receiver.RtcpRrTimeout());
  EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

  // Add a new RR with increase sequence number to reset timers.
  rtcp::ReportBlock rb2;
  rb2.SetMediaSsrc(kReceiverMainSsrc);
  rb2.SetExtHighestSeqNum(kSequenceNumber + 1);
  rtcp::ReceiverReport rr2;
  rr2.SetSenderSsrc(kSenderSsrc);
  rr2.AddReportBlock(rb2);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr2.Build());

  EXPECT_FALSE(receiver.RtcpRrTimeout());
  EXPECT_FALSE(receiver.RtcpRrSequenceNumberTimeout());

  // Verify we can get a timeout again once we've received new RR.
  mocks.clock.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rr2.Build());

  mocks.clock.AdvanceTimeMilliseconds(kRtcpIntervalMs + 1);
  EXPECT_FALSE(receiver.RtcpRrTimeout());
  EXPECT_TRUE(receiver.RtcpRrSequenceNumberTimeout());

  mocks.clock.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  EXPECT_TRUE(receiver.RtcpRrTimeout());
}

TEST(RtcpReceiverTest, TmmbrReceivedWithNoIncomingPacket) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  EXPECT_THAT(receiver.TmmbrReceived(), IsEmpty());
}

TEST(RtcpReceiverTest, TmmbrPacketAccepted) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint32_t kBitrateBps = 30000;
  auto tmmbr = std::make_unique<rtcp::Tmmbr>();
  tmmbr->SetSenderSsrc(kSenderSsrc);
  tmmbr->AddTmmbr(rtcp::TmmbItem(kReceiverMainSsrc, kBitrateBps, 0));
  auto sr = std::make_unique<rtcp::SenderReport>();
  sr->SetSenderSsrc(kSenderSsrc);
  rtcp::CompoundPacket compound;
  compound.Append(std::move(sr));
  compound.Append(std::move(tmmbr));

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.rtp_rtcp_impl, SetTmmbn(SizeIs(1)));
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedEstimatedBitrate(kBitrateBps));
  receiver.IncomingPacket(compound.Build());

  std::vector<rtcp::TmmbItem> tmmbr_received = receiver.TmmbrReceived();
  ASSERT_EQ(1u, tmmbr_received.size());
  EXPECT_EQ(kBitrateBps, tmmbr_received[0].bitrate_bps());
  EXPECT_EQ(kSenderSsrc, tmmbr_received[0].ssrc());
}

TEST(RtcpReceiverTest, TmmbrPacketNotForUsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint32_t kBitrateBps = 30000;
  auto tmmbr = std::make_unique<rtcp::Tmmbr>();
  tmmbr->SetSenderSsrc(kSenderSsrc);
  tmmbr->AddTmmbr(rtcp::TmmbItem(kNotToUsSsrc, kBitrateBps, 0));

  auto sr = std::make_unique<rtcp::SenderReport>();
  sr->SetSenderSsrc(kSenderSsrc);
  rtcp::CompoundPacket compound;
  compound.Append(std::move(sr));
  compound.Append(std::move(tmmbr));

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedEstimatedBitrate).Times(0);
  receiver.IncomingPacket(compound.Build());

  EXPECT_EQ(0u, receiver.TmmbrReceived().size());
}

TEST(RtcpReceiverTest, TmmbrPacketZeroRateIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  auto tmmbr = std::make_unique<rtcp::Tmmbr>();
  tmmbr->SetSenderSsrc(kSenderSsrc);
  tmmbr->AddTmmbr(rtcp::TmmbItem(kReceiverMainSsrc, 0, 0));
  auto sr = std::make_unique<rtcp::SenderReport>();
  sr->SetSenderSsrc(kSenderSsrc);
  rtcp::CompoundPacket compound;
  compound.Append(std::move(sr));
  compound.Append(std::move(tmmbr));

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedEstimatedBitrate).Times(0);
  receiver.IncomingPacket(compound.Build());

  EXPECT_EQ(0u, receiver.TmmbrReceived().size());
}

TEST(RtcpReceiverTest, TmmbrThreeConstraintsTimeOut) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  // Inject 3 packets "from" kSenderSsrc, kSenderSsrc+1, kSenderSsrc+2.
  // The times of arrival are starttime + 0, starttime + 5 and starttime + 10.
  for (uint32_t ssrc = kSenderSsrc; ssrc < kSenderSsrc + 3; ++ssrc) {
    auto tmmbr = std::make_unique<rtcp::Tmmbr>();
    tmmbr->SetSenderSsrc(ssrc);
    tmmbr->AddTmmbr(rtcp::TmmbItem(kReceiverMainSsrc, 30000, 0));
    auto sr = std::make_unique<rtcp::SenderReport>();
    sr->SetSenderSsrc(ssrc);
    rtcp::CompoundPacket compound;
    compound.Append(std::move(sr));
    compound.Append(std::move(tmmbr));

    EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
    EXPECT_CALL(mocks.rtp_rtcp_impl, SetTmmbn);
    EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
    EXPECT_CALL(mocks.bandwidth_observer, OnReceivedEstimatedBitrate);
    receiver.IncomingPacket(compound.Build());

    // 5 seconds between each packet.
    mocks.clock.AdvanceTimeMilliseconds(5000);
  }
  // It is now starttime + 15.
  EXPECT_THAT(receiver.TmmbrReceived(),
              AllOf(SizeIs(3),
                    Each(Property(&rtcp::TmmbItem::bitrate_bps, Eq(30'000U)))));

  // We expect the timeout to be 25 seconds. Advance the clock by 12
  // seconds, timing out the first packet.
  mocks.clock.AdvanceTimeMilliseconds(12000);
  EXPECT_THAT(receiver.TmmbrReceived(),
              UnorderedElementsAre(
                  Property(&rtcp::TmmbItem::ssrc, Eq(kSenderSsrc + 1)),
                  Property(&rtcp::TmmbItem::ssrc, Eq(kSenderSsrc + 2))));
}

TEST(RtcpReceiverTest,
     VerifyBlockAndTimestampObtainedFromReportBlockDataObserver) {
  ReceiverMocks mocks;
  MockReportBlockDataObserverImpl observer;
  RtpRtcpInterface::Configuration config = DefaultConfiguration(&mocks);
  config.report_block_data_observer = &observer;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint8_t kFractionLoss = 3;
  const uint32_t kCumulativeLoss = 7;
  const uint32_t kJitter = 9;
  const uint16_t kSequenceNumber = 1234;
  const int64_t kNtpNowMs =
      mocks.clock.CurrentNtpInMilliseconds() - rtc::kNtpJan1970Millisecs;

  rtcp::ReportBlock rtcp_block;
  rtcp_block.SetMediaSsrc(kReceiverMainSsrc);
  rtcp_block.SetExtHighestSeqNum(kSequenceNumber);
  rtcp_block.SetFractionLost(kFractionLoss);
  rtcp_block.SetCumulativeLost(kCumulativeLoss);
  rtcp_block.SetJitter(kJitter);

  rtcp::ReceiverReport rtcp_report;
  rtcp_report.SetSenderSsrc(kSenderSsrc);
  rtcp_report.AddReportBlock(rtcp_block);
  EXPECT_CALL(observer, OnReportBlockDataUpdated)
      .WillOnce([&](ReportBlockData report_block_data) {
        const auto& report_block = report_block_data.report_block();
        EXPECT_EQ(rtcp_block.source_ssrc(), report_block.source_ssrc);
        EXPECT_EQ(kSenderSsrc, report_block.sender_ssrc);
        EXPECT_EQ(rtcp_block.fraction_lost(), report_block.fraction_lost);
        EXPECT_EQ(rtcp_block.cumulative_lost_signed(),
                  report_block.packets_lost);
        EXPECT_EQ(rtcp_block.extended_high_seq_num(),
                  report_block.extended_highest_sequence_number);
        EXPECT_EQ(rtcp_block.jitter(), report_block.jitter);
        EXPECT_EQ(kNtpNowMs * rtc::kNumMicrosecsPerMillisec,
                  report_block_data.report_block_timestamp_utc_us());
        // No RTT is calculated in this test.
        EXPECT_EQ(0u, report_block_data.num_rtts());
      });
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rtcp_report.Build());
}

TEST(RtcpReceiverTest, VerifyRttObtainedFromReportBlockDataObserver) {
  ReceiverMocks mocks;
  MockReportBlockDataObserverImpl observer;
  RtpRtcpInterface::Configuration config = DefaultConfiguration(&mocks);
  config.report_block_data_observer = &observer;
  RTCPReceiver receiver(config, &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const TimeDelta kRtt = TimeDelta::Millis(120);
  const uint32_t kDelayNtp = 123000;
  const TimeDelta kDelay = CompactNtpRttToTimeDelta(kDelayNtp);

  uint32_t sent_ntp = CompactNtp(mocks.clock.CurrentNtpTime());
  mocks.clock.AdvanceTime(kRtt + kDelay);

  rtcp::SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  rtcp::ReportBlock block;
  block.SetMediaSsrc(kReceiverMainSsrc);
  block.SetLastSr(sent_ntp);
  block.SetDelayLastSr(kDelayNtp);
  sr.AddReportBlock(block);
  block.SetMediaSsrc(kReceiverExtraSsrc);
  block.SetLastSr(0);
  sr.AddReportBlock(block);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  InSequence sequence;
  EXPECT_CALL(observer, OnReportBlockDataUpdated)
      .WillOnce([&](ReportBlockData report_block_data) {
        EXPECT_EQ(kReceiverMainSsrc,
                  report_block_data.report_block().source_ssrc);
        EXPECT_EQ(1u, report_block_data.num_rtts());
        EXPECT_EQ(kRtt.ms(), report_block_data.min_rtt_ms());
        EXPECT_EQ(kRtt.ms(), report_block_data.max_rtt_ms());
        EXPECT_EQ(kRtt.ms(), report_block_data.sum_rtt_ms());
        EXPECT_EQ(kRtt.ms(), report_block_data.last_rtt_ms());
      });
  EXPECT_CALL(observer, OnReportBlockDataUpdated)
      .WillOnce([](ReportBlockData report_block_data) {
        EXPECT_EQ(kReceiverExtraSsrc,
                  report_block_data.report_block().source_ssrc);
        EXPECT_EQ(0u, report_block_data.num_rtts());
      });
  receiver.IncomingPacket(sr.Build());
}

TEST(RtcpReceiverTest, GetReportBlockDataAfterOneReportBlock) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint16_t kSequenceNumber = 1234;

  rtcp::ReportBlock rtcp_block;
  rtcp_block.SetMediaSsrc(kReceiverMainSsrc);
  rtcp_block.SetExtHighestSeqNum(kSequenceNumber);

  rtcp::ReceiverReport rtcp_report;
  rtcp_report.SetSenderSsrc(kSenderSsrc);
  rtcp_report.AddReportBlock(rtcp_block);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rtcp_report.Build());

  auto report_block_datas = receiver.GetLatestReportBlockData();
  ASSERT_THAT(report_block_datas, SizeIs(1));
  EXPECT_EQ(kReceiverMainSsrc,
            report_block_datas[0].report_block().source_ssrc);
  EXPECT_EQ(
      kSequenceNumber,
      report_block_datas[0].report_block().extended_highest_sequence_number);
}

TEST(RtcpReceiverTest, GetReportBlockDataAfterTwoReportBlocksOfSameSsrc) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint16_t kSequenceNumber1 = 1234;
  const uint16_t kSequenceNumber2 = 1235;

  rtcp::ReportBlock rtcp_block1;
  rtcp_block1.SetMediaSsrc(kReceiverMainSsrc);
  rtcp_block1.SetExtHighestSeqNum(kSequenceNumber1);

  rtcp::ReceiverReport rtcp_report1;
  rtcp_report1.SetSenderSsrc(kSenderSsrc);
  rtcp_report1.AddReportBlock(rtcp_block1);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rtcp_report1.Build());

  // Inject a report block with an increased the sequence number for the same
  // source SSRC.
  rtcp::ReportBlock rtcp_block2;
  rtcp_block2.SetMediaSsrc(kReceiverMainSsrc);
  rtcp_block2.SetExtHighestSeqNum(kSequenceNumber2);

  rtcp::ReceiverReport rtcp_report2;
  rtcp_report2.SetSenderSsrc(kSenderSsrc);
  rtcp_report2.AddReportBlock(rtcp_block2);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rtcp_report2.Build());

  // Only the latest block should be returned.
  auto report_block_datas = receiver.GetLatestReportBlockData();
  ASSERT_THAT(report_block_datas, SizeIs(1));
  EXPECT_EQ(kReceiverMainSsrc,
            report_block_datas[0].report_block().source_ssrc);
  EXPECT_EQ(
      kSequenceNumber2,
      report_block_datas[0].report_block().extended_highest_sequence_number);
}

TEST(RtcpReceiverTest, GetReportBlockDataAfterTwoReportBlocksOfDifferentSsrcs) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint16_t kSequenceNumber1 = 1234;
  const uint16_t kSequenceNumber2 = 42;

  rtcp::ReportBlock rtcp_block1;
  rtcp_block1.SetMediaSsrc(kReceiverMainSsrc);
  rtcp_block1.SetExtHighestSeqNum(kSequenceNumber1);

  rtcp::ReceiverReport rtcp_report1;
  rtcp_report1.SetSenderSsrc(kSenderSsrc);
  rtcp_report1.AddReportBlock(rtcp_block1);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rtcp_report1.Build());

  // Inject a report block for a different source SSRC.
  rtcp::ReportBlock rtcp_block2;
  rtcp_block2.SetMediaSsrc(kReceiverExtraSsrc);
  rtcp_block2.SetExtHighestSeqNum(kSequenceNumber2);

  rtcp::ReceiverReport rtcp_report2;
  rtcp_report2.SetSenderSsrc(kSenderSsrc);
  rtcp_report2.AddReportBlock(rtcp_block2);
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedRtcpReportBlocks);
  EXPECT_CALL(mocks.bandwidth_observer, OnReceivedRtcpReceiverReport);
  receiver.IncomingPacket(rtcp_report2.Build());

  // Both report blocks should be returned.
  auto report_block_datas = receiver.GetLatestReportBlockData();
  ASSERT_THAT(report_block_datas, SizeIs(2));
  EXPECT_EQ(kReceiverMainSsrc,
            report_block_datas[0].report_block().source_ssrc);
  EXPECT_EQ(
      kSequenceNumber1,
      report_block_datas[0].report_block().extended_highest_sequence_number);
  EXPECT_EQ(kReceiverExtraSsrc,
            report_block_datas[1].report_block().source_ssrc);
  EXPECT_EQ(
      kSequenceNumber2,
      report_block_datas[1].report_block().extended_highest_sequence_number);
}

TEST(RtcpReceiverTest, ReceivesTransportFeedback) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::TransportFeedback packet;
  packet.SetMediaSsrc(kReceiverMainSsrc);
  packet.SetSenderSsrc(kSenderSsrc);
  packet.SetBase(1, Timestamp::Millis(1));
  packet.AddReceivedPacket(1, Timestamp::Millis(1));

  EXPECT_CALL(
      mocks.transport_feedback_observer,
      OnTransportFeedback(AllOf(
          Property(&rtcp::TransportFeedback::media_ssrc, kReceiverMainSsrc),
          Property(&rtcp::TransportFeedback::sender_ssrc, kSenderSsrc))));
  receiver.IncomingPacket(packet.Build());
}

TEST(RtcpReceiverTest, ReceivesRemb) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint32_t kBitrateBps = 500000;
  rtcp::Remb remb;
  remb.SetSenderSsrc(kSenderSsrc);
  remb.SetBitrateBps(kBitrateBps);

  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedEstimatedBitrate(kBitrateBps));
  receiver.IncomingPacket(remb.Build());
}

TEST(RtcpReceiverTest, HandlesInvalidTransportFeedback) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  // Send a compound packet with a TransportFeedback followed by something else.
  auto packet = std::make_unique<rtcp::TransportFeedback>();
  packet->SetMediaSsrc(kReceiverMainSsrc);
  packet->SetSenderSsrc(kSenderSsrc);
  packet->SetBase(1, Timestamp::Millis(1));
  packet->AddReceivedPacket(1, Timestamp::Millis(1));

  static uint32_t kBitrateBps = 50000;
  auto remb = std::make_unique<rtcp::Remb>();
  remb->SetSenderSsrc(kSenderSsrc);
  remb->SetBitrateBps(kBitrateBps);
  rtcp::CompoundPacket compound;
  compound.Append(std::move(packet));
  compound.Append(std::move(remb));
  rtc::Buffer built_packet = compound.Build();

  // Modify the TransportFeedback packet so that it is invalid.
  const size_t kStatusCountOffset = 14;
  ByteWriter<uint16_t>::WriteBigEndian(&built_packet.data()[kStatusCountOffset],
                                       42);

  // Stress no transport feedback is expected.
  EXPECT_CALL(mocks.transport_feedback_observer, OnTransportFeedback).Times(0);
  // But remb should be processed and cause a callback
  EXPECT_CALL(mocks.bandwidth_observer,
              OnReceivedEstimatedBitrate(kBitrateBps));
  receiver.IncomingPacket(built_packet);
}

TEST(RtcpReceiverTest, Nack) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint16_t kNackList1[] = {1, 2, 3, 5};
  const uint16_t kNackList23[] = {5, 7, 30, 40, 41, 58, 59, 61, 63};
  const size_t kNackListLength2 = 4;
  const size_t kNackListLength3 = arraysize(kNackList23) - kNackListLength2;
  std::set<uint16_t> nack_set;
  nack_set.insert(std::begin(kNackList1), std::end(kNackList1));
  nack_set.insert(std::begin(kNackList23), std::end(kNackList23));

  auto nack1 = std::make_unique<rtcp::Nack>();
  nack1->SetSenderSsrc(kSenderSsrc);
  nack1->SetMediaSsrc(kReceiverMainSsrc);
  nack1->SetPacketIds(kNackList1, arraysize(kNackList1));

  EXPECT_CALL(mocks.rtp_rtcp_impl,
              OnReceivedNack(ElementsAreArray(kNackList1)));
  EXPECT_CALL(mocks.packet_type_counter_observer,
              RtcpPacketTypesCounterUpdated(
                  kReceiverMainSsrc,
                  AllOf(Field(&RtcpPacketTypeCounter::nack_requests,
                              arraysize(kNackList1)),
                        Field(&RtcpPacketTypeCounter::unique_nack_requests,
                              arraysize(kNackList1)))));
  receiver.IncomingPacket(nack1->Build());

  auto nack2 = std::make_unique<rtcp::Nack>();
  nack2->SetSenderSsrc(kSenderSsrc);
  nack2->SetMediaSsrc(kReceiverMainSsrc);
  nack2->SetPacketIds(kNackList23, kNackListLength2);

  auto nack3 = std::make_unique<rtcp::Nack>();
  nack3->SetSenderSsrc(kSenderSsrc);
  nack3->SetMediaSsrc(kReceiverMainSsrc);
  nack3->SetPacketIds(kNackList23 + kNackListLength2, kNackListLength3);

  rtcp::CompoundPacket two_nacks;
  two_nacks.Append(std::move(nack2));
  two_nacks.Append(std::move(nack3));

  EXPECT_CALL(mocks.rtp_rtcp_impl,
              OnReceivedNack(ElementsAreArray(kNackList23)));
  EXPECT_CALL(mocks.packet_type_counter_observer,
              RtcpPacketTypesCounterUpdated(
                  kReceiverMainSsrc,
                  AllOf(Field(&RtcpPacketTypeCounter::nack_requests,
                              arraysize(kNackList1) + arraysize(kNackList23)),
                        Field(&RtcpPacketTypeCounter::unique_nack_requests,
                              nack_set.size()))));
  receiver.IncomingPacket(two_nacks.Build());
}

TEST(RtcpReceiverTest, NackNotForUsIgnored) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  const uint16_t kNackList1[] = {1, 2, 3, 5};
  const size_t kNackListLength1 = std::end(kNackList1) - std::begin(kNackList1);

  rtcp::Nack nack;
  nack.SetSenderSsrc(kSenderSsrc);
  nack.SetMediaSsrc(kNotToUsSsrc);
  nack.SetPacketIds(kNackList1, kNackListLength1);

  EXPECT_CALL(mocks.packet_type_counter_observer,
              RtcpPacketTypesCounterUpdated(
                  _, Field(&RtcpPacketTypeCounter::nack_requests, 0)));
  receiver.IncomingPacket(nack.Build());
}

TEST(RtcpReceiverTest, ForceSenderReport) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  rtcp::RapidResyncRequest rr;
  rr.SetSenderSsrc(kSenderSsrc);
  rr.SetMediaSsrc(kReceiverMainSsrc);

  EXPECT_CALL(mocks.rtp_rtcp_impl, OnRequestSendReport());
  receiver.IncomingPacket(rr.Build());
}

TEST(RtcpReceiverTest, ReceivesTargetBitrate) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  VideoBitrateAllocation expected_allocation;
  expected_allocation.SetBitrate(0, 0, 10000);
  expected_allocation.SetBitrate(0, 1, 20000);
  expected_allocation.SetBitrate(1, 0, 40000);
  expected_allocation.SetBitrate(1, 1, 80000);

  rtcp::TargetBitrate bitrate;
  bitrate.AddTargetBitrate(0, 0, expected_allocation.GetBitrate(0, 0) / 1000);
  bitrate.AddTargetBitrate(0, 1, expected_allocation.GetBitrate(0, 1) / 1000);
  bitrate.AddTargetBitrate(1, 0, expected_allocation.GetBitrate(1, 0) / 1000);
  bitrate.AddTargetBitrate(1, 1, expected_allocation.GetBitrate(1, 1) / 1000);

  rtcp::ExtendedReports xr;
  xr.SetTargetBitrate(bitrate);

  // Wrong sender ssrc, target bitrate should be discarded.
  xr.SetSenderSsrc(kSenderSsrc + 1);
  EXPECT_CALL(mocks.bitrate_allocation_observer,
              OnBitrateAllocationUpdated(expected_allocation))
      .Times(0);
  receiver.IncomingPacket(xr.Build());

  // Set correct ssrc, callback should be called once.
  xr.SetSenderSsrc(kSenderSsrc);
  EXPECT_CALL(mocks.bitrate_allocation_observer,
              OnBitrateAllocationUpdated(expected_allocation));
  receiver.IncomingPacket(xr.Build());
}

TEST(RtcpReceiverTest, HandlesIncorrectTargetBitrate) {
  ReceiverMocks mocks;
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  VideoBitrateAllocation expected_allocation;
  expected_allocation.SetBitrate(0, 0, 10000);

  rtcp::TargetBitrate bitrate;
  bitrate.AddTargetBitrate(0, 0, expected_allocation.GetBitrate(0, 0) / 1000);
  bitrate.AddTargetBitrate(0, kMaxTemporalStreams, 20000);
  bitrate.AddTargetBitrate(kMaxSpatialLayers, 0, 40000);

  rtcp::ExtendedReports xr;
  xr.SetTargetBitrate(bitrate);
  xr.SetSenderSsrc(kSenderSsrc);

  EXPECT_CALL(mocks.bitrate_allocation_observer,
              OnBitrateAllocationUpdated(expected_allocation));
  receiver.IncomingPacket(xr.Build());
}

TEST(RtcpReceiverTest, ChangeLocalMediaSsrc) {
  ReceiverMocks mocks;
  // Construct a receiver with `kReceiverMainSsrc` (default) local media ssrc.
  RTCPReceiver receiver(DefaultConfiguration(&mocks), &mocks.rtp_rtcp_impl);
  receiver.SetRemoteSSRC(kSenderSsrc);

  constexpr uint32_t kSecondarySsrc = kReceiverMainSsrc + 1;

  // Expect to only get the `OnReceivedNack()` callback once since we'll
  // configure it for the `kReceiverMainSsrc` media ssrc.
  EXPECT_CALL(mocks.rtp_rtcp_impl, OnReceivedNack);

  // We'll get two callbacks to RtcpPacketTypesCounterUpdated, one for each
  // call to `IncomingPacket`, differentiated by the local media ssrc.
  EXPECT_CALL(mocks.packet_type_counter_observer,
              RtcpPacketTypesCounterUpdated(kReceiverMainSsrc, _));
  EXPECT_CALL(mocks.packet_type_counter_observer,
              RtcpPacketTypesCounterUpdated(kSecondarySsrc, _));

  // Construct a test nack packet with media ssrc set to `kReceiverMainSsrc`.
  rtcp::Nack nack;
  nack.SetSenderSsrc(kSenderSsrc);
  nack.SetMediaSsrc(kReceiverMainSsrc);
  const uint16_t kNackList[] = {1, 2, 3, 5};
  nack.SetPacketIds(kNackList, std::size(kNackList));

  // Deliver the first callback.
  receiver.IncomingPacket(nack.Build());

  // Change the set local media ssrc.
  receiver.set_local_media_ssrc(kSecondarySsrc);

  // Deliver another packet - this time there will be no callback to
  // OnReceivedNack due to the ssrc not matching.
  receiver.IncomingPacket(nack.Build());
}

}  // namespace webrtc
