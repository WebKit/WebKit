/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/test/testAPI/test_api.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/test/null_transport.h"

namespace webrtc {

void LoopBackTransport::SetSendModule(RtpRtcp* rtp_rtcp_module,
                                      RTPPayloadRegistry* payload_registry,
                                      RtpReceiver* receiver,
                                      ReceiveStatistics* receive_statistics) {
  rtp_rtcp_module_ = rtp_rtcp_module;
  rtp_payload_registry_ = payload_registry;
  rtp_receiver_ = receiver;
  receive_statistics_ = receive_statistics;
}

void LoopBackTransport::DropEveryNthPacket(int n) {
  packet_loss_ = n;
}

bool LoopBackTransport::SendRtp(const uint8_t* data,
                                size_t len,
                                const PacketOptions& options) {
  count_++;
  if (packet_loss_ > 0) {
    if ((count_ % packet_loss_) == 0) {
      return true;
    }
  }
  RTPHeader header;
  std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
  if (!parser->Parse(data, len, &header)) {
    return false;
  }
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                  &payload_specific)) {
    return false;
  }
  const uint8_t* payload = data + header.headerLength;
  RTC_CHECK_GE(len, header.headerLength);
  const size_t payload_length = len - header.headerLength;
  receive_statistics_->IncomingPacket(header, len, false);
  if (!rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                        payload_specific, true)) {
    return false;
  }
  return true;
}

bool LoopBackTransport::SendRtcp(const uint8_t* data, size_t len) {
  if (rtp_rtcp_module_->IncomingRtcpPacket((const uint8_t*)data, len) < 0) {
    return false;
  }
  return true;
}

int32_t TestRtpReceiver::OnReceivedPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const webrtc::WebRtcRTPHeader* rtp_header) {
  EXPECT_LE(payload_size, sizeof(payload_data_));
  memcpy(payload_data_, payload_data, payload_size);
  memcpy(&rtp_header_, rtp_header, sizeof(rtp_header_));
  payload_size_ = payload_size;
  return 0;
}

class RtpRtcpAPITest : public ::testing::Test {
 protected:
  RtpRtcpAPITest()
      : fake_clock_(123456), retransmission_rate_limiter_(&fake_clock_, 1000) {
    test_csrcs_.push_back(1234);
    test_csrcs_.push_back(2345);
    test_ssrc_ = 3456;
    test_timestamp_ = 4567;
    test_sequence_number_ = 2345;
  }
  ~RtpRtcpAPITest() {}

  const uint32_t initial_ssrc = 8888;

  void SetUp() override {
    RtpRtcp::Configuration configuration;
    configuration.audio = true;
    configuration.clock = &fake_clock_;
    configuration.outgoing_transport = &null_transport_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;
    module_.reset(RtpRtcp::CreateRtpRtcp(configuration));
    module_->SetSSRC(initial_ssrc);
    rtp_payload_registry_.reset(new RTPPayloadRegistry());
    rtp_receiver_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock_, NULL, NULL, rtp_payload_registry_.get()));
  }

  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
  std::unique_ptr<RtpRtcp> module_;
  uint32_t test_ssrc_;
  uint32_t test_timestamp_;
  uint16_t test_sequence_number_;
  std::vector<uint32_t> test_csrcs_;
  SimulatedClock fake_clock_;
  test::NullTransport null_transport_;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpAPITest, Basic) {
  module_->SetSequenceNumber(test_sequence_number_);
  EXPECT_EQ(test_sequence_number_, module_->SequenceNumber());

  module_->SetStartTimestamp(test_timestamp_);
  EXPECT_EQ(test_timestamp_, module_->StartTimestamp());

  EXPECT_FALSE(module_->Sending());
  EXPECT_EQ(0, module_->SetSendingStatus(true));
  EXPECT_TRUE(module_->Sending());
}

TEST_F(RtpRtcpAPITest, PacketSize) {
  module_->SetMaxRtpPacketSize(1234);
  EXPECT_EQ(1234u, module_->MaxRtpPacketSize());
  EXPECT_EQ(1234u - 12u /* Minimum RTP header */, module_->MaxPayloadSize());
}

TEST_F(RtpRtcpAPITest, SSRC) {
  module_->SetSSRC(test_ssrc_);
  EXPECT_EQ(test_ssrc_, module_->SSRC());
}

TEST_F(RtpRtcpAPITest, RTCP) {
  EXPECT_EQ(RtcpMode::kOff, module_->RTCP());
  module_->SetRTCPStatus(RtcpMode::kCompound);
  EXPECT_EQ(RtcpMode::kCompound, module_->RTCP());

  EXPECT_EQ(0, module_->SetCNAME("john.doe@test.test"));

  EXPECT_FALSE(module_->TMMBR());
  module_->SetTMMBRStatus(true);
  EXPECT_TRUE(module_->TMMBR());
  module_->SetTMMBRStatus(false);
  EXPECT_FALSE(module_->TMMBR());
}

TEST_F(RtpRtcpAPITest, RtxSender) {
  module_->SetRtxSendStatus(kRtxRetransmitted);
  EXPECT_EQ(kRtxRetransmitted, module_->RtxSendStatus());

  module_->SetRtxSendStatus(kRtxOff);
  EXPECT_EQ(kRtxOff, module_->RtxSendStatus());

  module_->SetRtxSendStatus(kRtxRetransmitted);
  EXPECT_EQ(kRtxRetransmitted, module_->RtxSendStatus());
}

TEST_F(RtpRtcpAPITest, RtxReceiver) {
  const uint32_t kRtxSsrc = 1;
  const int kRtxPayloadType = 119;
  const int kPayloadType = 100;
  EXPECT_FALSE(rtp_payload_registry_->RtxEnabled());
  rtp_payload_registry_->SetRtxSsrc(kRtxSsrc);
  rtp_payload_registry_->SetRtxPayloadType(kRtxPayloadType, kPayloadType);
  EXPECT_TRUE(rtp_payload_registry_->RtxEnabled());
  RTPHeader rtx_header;
  rtx_header.ssrc = kRtxSsrc;
  rtx_header.payloadType = kRtxPayloadType;
  EXPECT_TRUE(rtp_payload_registry_->IsRtx(rtx_header));
  rtx_header.ssrc = 0;
  EXPECT_FALSE(rtp_payload_registry_->IsRtx(rtx_header));
  rtx_header.ssrc = kRtxSsrc;
  rtx_header.payloadType = 0;
  EXPECT_TRUE(rtp_payload_registry_->IsRtx(rtx_header));
}

}  // namespace webrtc
