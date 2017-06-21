/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

class PacketRouterTest : public ::testing::Test {
 public:
  PacketRouterTest() : packet_router_(new PacketRouter()) {}
 protected:
  static const int kProbeMinProbes = 5;
  static const int kProbeMinBytes = 1000;
  const std::unique_ptr<PacketRouter> packet_router_;
};

TEST_F(PacketRouterTest, TimeToSendPacket) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  packet_router_->AddSendRtpModule(&rtp_1);
  packet_router_->AddSendRtpModule(&rtp_2);

  const uint16_t kSsrc1 = 1234;
  uint16_t sequence_number = 17;
  uint64_t timestamp = 7890;
  bool retransmission = false;

  // Send on the first module by letting rtp_1 be sending with correct ssrc.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC()).Times(1).WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_1, TimeToSendPacket(
                         kSsrc1, sequence_number, timestamp, retransmission,
                         Field(&PacedPacketInfo::probe_cluster_id, 1)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(
      kSsrc1, sequence_number, timestamp, retransmission,
      PacedPacketInfo(1, kProbeMinProbes, kProbeMinBytes)));

  // Send on the second module by letting rtp_2 be sending, but not rtp_1.
  ++sequence_number;
  timestamp += 30;
  retransmission = true;
  const uint16_t kSsrc2 = 4567;
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(
                         kSsrc2, sequence_number, timestamp, retransmission,
                         Field(&PacedPacketInfo::probe_cluster_id, 2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(packet_router_->TimeToSendPacket(
      kSsrc2, sequence_number, timestamp, retransmission,
      PacedPacketInfo(2, kProbeMinProbes, kProbeMinBytes)));

  // No module is sending, hence no packet should be sent.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(
      kSsrc1, sequence_number, timestamp, retransmission,
      PacedPacketInfo(1, kProbeMinProbes, kProbeMinBytes)));

  // Add a packet with incorrect ssrc and test it's dropped in the router.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC()).Times(1).WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(
      kSsrc1 + kSsrc2, sequence_number, timestamp, retransmission,
      PacedPacketInfo(1, kProbeMinProbes, kProbeMinBytes)));

  packet_router_->RemoveSendRtpModule(&rtp_1);

  // rtp_1 has been removed, try sending a packet on that ssrc and make sure
  // it is dropped as expected by not expecting any calls to rtp_1.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(
      kSsrc1, sequence_number, timestamp, retransmission,
      PacedPacketInfo(PacedPacketInfo::kNotAProbe, kProbeMinBytes,
                      kProbeMinBytes)));

  packet_router_->RemoveSendRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, TimeToSendPadding) {
  const uint16_t kSsrc1 = 1234;
  const uint16_t kSsrc2 = 4567;

  MockRtpRtcp rtp_1;
  EXPECT_CALL(rtp_1, RtxSendStatus()).WillOnce(Return(kRtxOff));
  EXPECT_CALL(rtp_1, SSRC()).WillRepeatedly(Return(kSsrc1));
  MockRtpRtcp rtp_2;
  // rtp_2 will be prioritized for padding.
  EXPECT_CALL(rtp_2, RtxSendStatus()).WillOnce(Return(kRtxRedundantPayloads));
  EXPECT_CALL(rtp_2, SSRC()).WillRepeatedly(Return(kSsrc2));
  packet_router_->AddSendRtpModule(&rtp_1);
  packet_router_->AddSendRtpModule(&rtp_2);

  // Default configuration, sending padding on all modules sending media,
  // ordered by priority (based on rtx mode).
  const size_t requested_padding_bytes = 1000;
  const size_t sent_padding_bytes = 890;
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, HasBweExtensions()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2,
              TimeToSendPadding(requested_padding_bytes,
                                Field(&PacedPacketInfo::probe_cluster_id, 111)))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, HasBweExtensions()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1,
              TimeToSendPadding(requested_padding_bytes - sent_padding_bytes,
                                Field(&PacedPacketInfo::probe_cluster_id, 111)))
      .Times(1)
      .WillOnce(Return(requested_padding_bytes - sent_padding_bytes));
  EXPECT_EQ(requested_padding_bytes,
            packet_router_->TimeToSendPadding(
                requested_padding_bytes,
                PacedPacketInfo(111, kProbeMinBytes, kProbeMinBytes)));

  // Let only the lower priority module be sending and verify the padding
  // request is routed there.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPadding(requested_padding_bytes, _)).Times(0);
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, HasBweExtensions()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, TimeToSendPadding(_, _))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_EQ(sent_padding_bytes,
            packet_router_->TimeToSendPadding(
                requested_padding_bytes,
                PacedPacketInfo(PacedPacketInfo::kNotAProbe, kProbeMinBytes,
                                kProbeMinBytes)));

  // No sending module at all.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_, _)).Times(0);
  EXPECT_EQ(0u,
            packet_router_->TimeToSendPadding(
                requested_padding_bytes,
                PacedPacketInfo(PacedPacketInfo::kNotAProbe, kProbeMinBytes,
                                kProbeMinBytes)));

  // Only one module has BWE extensions.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, HasBweExtensions()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, HasBweExtensions()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(requested_padding_bytes, _))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_EQ(sent_padding_bytes,
            packet_router_->TimeToSendPadding(
                requested_padding_bytes,
                PacedPacketInfo(PacedPacketInfo::kNotAProbe, kProbeMinBytes,
                                kProbeMinBytes)));

  packet_router_->RemoveSendRtpModule(&rtp_1);

  // rtp_1 has been removed, try sending padding and make sure rtp_1 isn't asked
  // to send by not expecting any calls. Instead verify rtp_2 is called.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, HasBweExtensions()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(requested_padding_bytes, _)).Times(1);
  EXPECT_EQ(0u,
            packet_router_->TimeToSendPadding(
                requested_padding_bytes,
                PacedPacketInfo(PacedPacketInfo::kNotAProbe, kProbeMinBytes,
                                kProbeMinBytes)));

  packet_router_->RemoveSendRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, SenderOnlyFunctionsRespectSendingMedia) {
  MockRtpRtcp rtp;
  packet_router_->AddSendRtpModule(&rtp);
  static const uint16_t kSsrc = 1234;
  EXPECT_CALL(rtp, SSRC()).WillRepeatedly(Return(kSsrc));
  EXPECT_CALL(rtp, SendingMedia()).WillRepeatedly(Return(false));

  // Verify that TimeToSendPacket does not end up in a receiver.
  EXPECT_CALL(rtp, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(
      kSsrc, 1, 1, false, PacedPacketInfo(PacedPacketInfo::kNotAProbe,
                                          kProbeMinBytes, kProbeMinBytes)));
  // Verify that TimeToSendPadding does not end up in a receiver.
  EXPECT_CALL(rtp, TimeToSendPadding(_, _)).Times(0);
  EXPECT_EQ(0u,
            packet_router_->TimeToSendPadding(
                200, PacedPacketInfo(PacedPacketInfo::kNotAProbe,
                                     kProbeMinBytes, kProbeMinBytes)));

  packet_router_->RemoveSendRtpModule(&rtp);
}

TEST_F(PacketRouterTest, AllocateSequenceNumbers) {
  const uint16_t kStartSeq = 0xFFF0;
  const size_t kNumPackets = 32;

  packet_router_->SetTransportWideSequenceNumber(kStartSeq - 1);

  for (size_t i = 0; i < kNumPackets; ++i) {
    uint16_t seq = packet_router_->AllocateSequenceNumber();
    uint32_t expected_unwrapped_seq = static_cast<uint32_t>(kStartSeq) + i;
    EXPECT_EQ(static_cast<uint16_t>(expected_unwrapped_seq & 0xFFFF), seq);
  }
}

TEST_F(PacketRouterTest, SendTransportFeedback) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  packet_router_->AddSendRtpModule(&rtp_1);
  packet_router_->AddReceiveRtpModule(&rtp_2);

  rtcp::TransportFeedback feedback;
  EXPECT_CALL(rtp_1, SendFeedbackPacket(_)).Times(1).WillOnce(Return(true));
  packet_router_->SendTransportFeedback(&feedback);
  packet_router_->RemoveSendRtpModule(&rtp_1);
  EXPECT_CALL(rtp_2, SendFeedbackPacket(_)).Times(1).WillOnce(Return(true));
  packet_router_->SendTransportFeedback(&feedback);
  packet_router_->RemoveReceiveRtpModule(&rtp_2);
}

TEST(PacketRouterRembTest, PreferSendModuleOverReceiveModule) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp_recv;
  MockRtpRtcp rtp_send;
  PacketRouter packet_router;

  EXPECT_CALL(rtp_recv, SetREMBStatus(true)).Times(1);
  packet_router.AddReceiveRtpModule(&rtp_recv);

  const uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};

  ON_CALL(rtp_recv, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  EXPECT_CALL(rtp_recv, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Add a send module, which should be preferred over the receive module.
  EXPECT_CALL(rtp_recv, SetREMBStatus(false)).Times(1);
  EXPECT_CALL(rtp_send, SetREMBStatus(true)).Times(1);
  packet_router.AddSendRtpModule(&rtp_send);
  ON_CALL(rtp_recv, REMB()).WillByDefault(Return(false));
  ON_CALL(rtp_send, REMB()).WillByDefault(Return(true));

  // Lower bitrate to send another REMB packet.
  EXPECT_CALL(rtp_send, SetREMBData(bitrate_estimate - 100, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate - 100);

  EXPECT_CALL(rtp_send, SetREMBStatus(false)).Times(1);
  EXPECT_CALL(rtp_recv, SetREMBStatus(true)).Times(1);
  packet_router.RemoveSendRtpModule(&rtp_send);
  EXPECT_CALL(rtp_recv, SetREMBStatus(false)).Times(1);
  packet_router.RemoveReceiveRtpModule(&rtp_recv);
}

TEST(PacketRouterRembTest, LowerEstimateToSendRemb) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp;
  PacketRouter packet_router;

  EXPECT_CALL(rtp, SetREMBStatus(true)).Times(1);
  packet_router.AddSendRtpModule(&rtp);

  uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};

  ON_CALL(rtp, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate with more than 3% to trigger a call to SetREMBData right
  // away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  EXPECT_CALL(rtp, SetREMBStatus(false)).Times(1);
  packet_router.RemoveSendRtpModule(&rtp);
}

TEST(PacketRouterRembTest, VerifyIncreasingAndDecreasing) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp;
  PacketRouter packet_router;
  packet_router.AddSendRtpModule(&rtp);

  uint32_t bitrate_estimate[] = {456, 789};
  std::vector<uint32_t> ssrcs = {1234, 5678};

  ON_CALL(rtp, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate[0]);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate[0], ssrcs)).Times(1);
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate[0]);

  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate[1] + 100);

  // Lower the estimate to trigger a callback.
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate[1], ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate[1]);

  packet_router.RemoveSendRtpModule(&rtp);
}

TEST(PacketRouterRembTest, NoRembForIncreasedBitrate) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp;
  PacketRouter packet_router;
  packet_router.AddSendRtpModule(&rtp);

  uint32_t bitrate_estimate = 456;
  std::vector<uint32_t> ssrcs = {1234, 5678};

  ON_CALL(rtp, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Increased estimate shouldn't trigger a callback right away.
  EXPECT_CALL(rtp, SetREMBData(_, _)).Times(0);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate + 1);

  // Decreasing the estimate less than 3% shouldn't trigger a new callback.
  EXPECT_CALL(rtp, SetREMBData(_, _)).Times(0);
  int lower_estimate = bitrate_estimate * 98 / 100;
  packet_router.OnReceiveBitrateChanged(ssrcs, lower_estimate);

  packet_router.RemoveSendRtpModule(&rtp);
}

TEST(PacketRouterRembTest, ChangeSendRtpModule) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp_send;
  MockRtpRtcp rtp_recv;
  PacketRouter packet_router;
  packet_router.AddSendRtpModule(&rtp_send);
  packet_router.AddReceiveRtpModule(&rtp_recv);

  uint32_t bitrate_estimate = 456;
  std::vector<uint32_t> ssrcs = {1234, 5678};

  ON_CALL(rtp_send, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  EXPECT_CALL(rtp_send, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Decrease estimate to trigger a REMB.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp_send, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Remove the sending module -> should get remb on the second module.
  packet_router.RemoveSendRtpModule(&rtp_send);

  ON_CALL(rtp_send, REMB()).WillByDefault(Return(false));
  ON_CALL(rtp_recv, REMB()).WillByDefault(Return(true));

  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp_recv, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  packet_router.RemoveReceiveRtpModule(&rtp_recv);
}

TEST(PacketRouterRembTest, OnlyOneRembForRepeatedOnReceiveBitrateChanged) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp;
  PacketRouter packet_router;
  packet_router.AddSendRtpModule(&rtp);

  uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};

  ON_CALL(rtp, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  EXPECT_CALL(rtp, SetREMBData(_, _)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate, should trigger a call to SetREMBData right away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged again, this should not trigger a new callback.
  EXPECT_CALL(rtp, SetREMBData(_, _)).Times(0);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  packet_router.RemoveSendRtpModule(&rtp);
}

// Only register receiving modules and make sure we fallback to trigger a REMB
// packet on this one.
TEST(PacketRouterRembTest, NoSendingRtpModule) {
  rtc::ScopedFakeClock clock;
  MockRtpRtcp rtp;
  PacketRouter packet_router;

  EXPECT_CALL(rtp, SetREMBStatus(true)).Times(1);
  packet_router.AddReceiveRtpModule(&rtp);

  uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};

  ON_CALL(rtp, REMB()).WillByDefault(Return(true));
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1000));
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate to trigger a new packet REMB packet.
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate - 100, ssrcs)).Times(1);
  packet_router.OnReceiveBitrateChanged(ssrcs, bitrate_estimate - 100);

  EXPECT_CALL(rtp, SetREMBStatus(false)).Times(1);
  packet_router.RemoveReceiveRtpModule(&rtp);
}

}  // namespace webrtc
