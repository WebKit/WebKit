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
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

class PacketRouterTest : public ::testing::Test {
 public:
  PacketRouterTest() : packet_router_(new PacketRouter()) {}
 protected:
  const std::unique_ptr<PacketRouter> packet_router_;
};

TEST_F(PacketRouterTest, TimeToSendPacket) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  packet_router_->AddRtpModule(&rtp_1);
  packet_router_->AddRtpModule(&rtp_2);

  const uint16_t kSsrc1 = 1234;
  uint16_t sequence_number = 17;
  uint64_t timestamp = 7890;
  bool retransmission = false;

  // Send on the first module by letting rtp_1 be sending with correct ssrc.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC()).Times(1).WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_1, TimeToSendPacket(kSsrc1, sequence_number, timestamp,
                                      retransmission, 1))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1, sequence_number,
                                               timestamp, retransmission, 1));

  // Send on the second module by letting rtp_2 be sending, but not rtp_1.
  ++sequence_number;
  timestamp += 30;
  retransmission = true;
  const uint16_t kSsrc2 = 4567;
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(kSsrc2, sequence_number, timestamp,
                                      retransmission, 2))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc2, sequence_number,
                                               timestamp, retransmission, 2));

  // No module is sending, hence no packet should be sent.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1, sequence_number,
                                               timestamp, retransmission, 1));

  // Add a packet with incorrect ssrc and test it's dropped in the router.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC()).Times(1).WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1 + kSsrc2, sequence_number,
                                               timestamp, retransmission, 1));

  packet_router_->RemoveRtpModule(&rtp_1);

  // rtp_1 has been removed, try sending a packet on that ssrc and make sure
  // it is dropped as expected by not expecting any calls to rtp_1.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1, sequence_number,
                                               timestamp, retransmission,
                                               PacketInfo::kNotAProbe));

  packet_router_->RemoveRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, TimeToSendPadding) {
  const uint16_t kSsrc1 = 1234;
  const uint16_t kSsrc2 = 4567;

  MockRtpRtcp rtp_1;
  EXPECT_CALL(rtp_1, SSRC()).WillRepeatedly(Return(kSsrc1));
  MockRtpRtcp rtp_2;
  EXPECT_CALL(rtp_2, SSRC()).WillRepeatedly(Return(kSsrc2));
  packet_router_->AddRtpModule(&rtp_1);
  packet_router_->AddRtpModule(&rtp_2);

  // Default configuration, sending padding on all modules sending media,
  // ordered by SSRC.
  const size_t requested_padding_bytes = 1000;
  const size_t sent_padding_bytes = 890;
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes, 111))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(
                         requested_padding_bytes - sent_padding_bytes, 111))
      .Times(1)
      .WillOnce(Return(requested_padding_bytes - sent_padding_bytes));
  EXPECT_EQ(requested_padding_bytes,
            packet_router_->TimeToSendPadding(requested_padding_bytes, 111));

  // Let only the second module be sending and verify the padding request is
  // routed there.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_, _))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_EQ(sent_padding_bytes,
            packet_router_->TimeToSendPadding(requested_padding_bytes,
                                              PacketInfo::kNotAProbe));

  // No sending module at all.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_, _)).Times(0);
  EXPECT_EQ(0u, packet_router_->TimeToSendPadding(requested_padding_bytes,
                                                  PacketInfo::kNotAProbe));

  packet_router_->RemoveRtpModule(&rtp_1);

  // rtp_1 has been removed, try sending padding and make sure rtp_1 isn't asked
  // to send by not expecting any calls. Instead verify rtp_2 is called.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(requested_padding_bytes, _)).Times(1);
  EXPECT_EQ(0u, packet_router_->TimeToSendPadding(requested_padding_bytes,
                                                  PacketInfo::kNotAProbe));

  packet_router_->RemoveRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, SenderOnlyFunctionsRespectSendingMedia) {
  MockRtpRtcp rtp;
  packet_router_->AddRtpModule(&rtp);
  static const uint16_t kSsrc = 1234;
  EXPECT_CALL(rtp, SSRC()).WillRepeatedly(Return(kSsrc));
  EXPECT_CALL(rtp, SendingMedia()).WillRepeatedly(Return(false));

  // Verify that TimeToSendPacket does not end up in a receiver.
  EXPECT_CALL(rtp, TimeToSendPacket(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc, 1, 1, false,
                                               PacketInfo::kNotAProbe));
  // Verify that TimeToSendPadding does not end up in a receiver.
  EXPECT_CALL(rtp, TimeToSendPadding(_, _)).Times(0);
  EXPECT_EQ(0u, packet_router_->TimeToSendPadding(200, PacketInfo::kNotAProbe));

  packet_router_->RemoveRtpModule(&rtp);
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

TEST_F(PacketRouterTest, SendFeedback) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  packet_router_->AddRtpModule(&rtp_1);
  packet_router_->AddRtpModule(&rtp_2);

  rtcp::TransportFeedback feedback;
  EXPECT_CALL(rtp_1, SendFeedbackPacket(_)).Times(1);
  packet_router_->SendFeedback(&feedback);
  packet_router_->RemoveRtpModule(&rtp_1);
  EXPECT_CALL(rtp_2, SendFeedbackPacket(_)).Times(1);
  packet_router_->SendFeedback(&feedback);
  packet_router_->RemoveRtpModule(&rtp_2);
}
}  // namespace webrtc
