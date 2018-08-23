/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/test/testAPI/test_api.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/rate_limiter.h"
#include "test/null_transport.h"

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
  const auto pl =
      rtp_payload_registry_->PayloadTypeToPayload(header.payloadType);
  if (!pl) {
    return false;
  }
  const uint8_t* payload = data + header.headerLength;
  RTC_CHECK_GE(len, header.headerLength);
  const size_t payload_length = len - header.headerLength;
  receive_statistics_->IncomingPacket(header, len, false);
  return rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                          pl->typeSpecific);
}

bool LoopBackTransport::SendRtcp(const uint8_t* data, size_t len) {
  rtp_rtcp_module_->IncomingRtcpPacket((const uint8_t*)data, len);
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
  }
  ~RtpRtcpAPITest() override = default;

  void SetUp() override {
    const uint32_t kInitialSsrc = 8888;
    RtpRtcp::Configuration configuration;
    configuration.audio = true;
    configuration.clock = &fake_clock_;
    configuration.outgoing_transport = &null_transport_;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;
    module_.reset(RtpRtcp::CreateRtpRtcp(configuration));
    module_->SetSSRC(kInitialSsrc);
  }

  std::unique_ptr<RtpRtcp> module_;
  SimulatedClock fake_clock_;
  test::NullTransport null_transport_;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpAPITest, Basic) {
  const uint16_t kSequenceNumber = 2345;
  module_->SetSequenceNumber(kSequenceNumber);
  EXPECT_EQ(kSequenceNumber, module_->SequenceNumber());

  const uint32_t kTimestamp = 4567;
  module_->SetStartTimestamp(kTimestamp);
  EXPECT_EQ(kTimestamp, module_->StartTimestamp());

  EXPECT_FALSE(module_->Sending());
  EXPECT_EQ(0, module_->SetSendingStatus(true));
  EXPECT_TRUE(module_->Sending());
}

TEST_F(RtpRtcpAPITest, PacketSize) {
  module_->SetMaxRtpPacketSize(1234);
  EXPECT_EQ(1234u, module_->MaxRtpPacketSize());
}

TEST_F(RtpRtcpAPITest, SSRC) {
  const uint32_t kSsrc = 3456;
  module_->SetSSRC(kSsrc);
  EXPECT_EQ(kSsrc, module_->SSRC());
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

TEST_F(RtpRtcpAPITest, LegalMidName) {
  static const std::string kLegalMidNames[] = {
      // clang-format off
      "audio",
      "audio0",
      "audio_0",
      // clang-format on
  };
  for (const auto& name : kLegalMidNames) {
    EXPECT_TRUE(StreamId::IsLegalMidName(name))
        << "Mid should be legal: " << name;
  }

  static const std::string kNonLegalMidNames[] = {
      // clang-format off
      "",
      "(audio0)",
      // clang-format on
  };
  for (const auto& name : kNonLegalMidNames) {
    EXPECT_FALSE(StreamId::IsLegalMidName(name))
        << "Mid should not be legal: " << name;
  }
}

TEST_F(RtpRtcpAPITest, LegalRsidName) {
  static const std::string kLegalRsidNames[] = {
      // clang-format off
      "audio",
      "audio0",
      // clang-format on
  };
  for (const auto& name : kLegalRsidNames) {
    EXPECT_TRUE(StreamId::IsLegalRsidName(name))
        << "Rsid should be legal: " << name;
  }

  static const std::string kNonLegalRsidNames[] = {
      // clang-format off
      "",
      "audio_0",
      "(audio0)",
      // clang-format on
  };
  for (const auto& name : kNonLegalRsidNames) {
    EXPECT_FALSE(StreamId::IsLegalRsidName(name))
        << "Rsid should not be legal: " << name;
  }
}

}  // namespace webrtc
