/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_audio.h"
#include "modules/rtp_rtcp/test/testAPI/test_api.h"
#include "rtc_base/rate_limiter.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const uint32_t kTestRate = 64000u;
const uint8_t kTestPayload[] = {'t', 'e', 's', 't'};
const uint8_t kPcmuPayloadType = 96;
const uint8_t kDtmfPayloadType = 97;
const uint32_t kSsrc = 3456;
const uint32_t kTimestamp = 4567;

struct CngCodecSpec {
  int payload_type;
  int clockrate_hz;
};

const CngCodecSpec kCngCodecs[] = {{13, 8000},
                                   {103, 16000},
                                   {104, 32000},
                                   {105, 48000}};

// Rough sanity check of DTMF payload.
void VerifyDtmf(const uint8_t* payloadData,
                size_t payloadSize) {
  EXPECT_EQ(payloadSize, 4u);
  uint8_t p0 = (payloadSize > 0) ? payloadData[0] : 0xff;
  uint8_t p1 = (payloadSize > 1) ? payloadData[1] : 0xff;
  uint8_t p2 = (payloadSize > 2) ? payloadData[2] : 0xff;
  uint8_t p3 = (payloadSize > 3) ? payloadData[3] : 0xff;
  uint8_t event = p0;
  bool reserved = (p1 >> 6) & 1;
  uint8_t volume = p1 & 63;
  uint16_t duration = (p2 << 8) | p3;

  // 0-15 are digits, #, *, A-D, 32 is answer tone (see rfc 4734)
  EXPECT_LE(event, 32u);
  EXPECT_TRUE(event < 16u || event == 32u);
  EXPECT_FALSE(reserved);
  EXPECT_EQ(volume, 10u);
  // Long duration for answer tone events only
  EXPECT_TRUE(duration <= 1280 || event == 32u);
}

class VerifyingAudioReceiver : public RtpData {
 public:
  int32_t OnReceivedPayloadData(
      const uint8_t* payloadData,
      size_t payloadSize,
      const webrtc::WebRtcRTPHeader* rtpHeader) override {
    const uint8_t payload_type = rtpHeader->header.payloadType;
    if (payload_type == kPcmuPayloadType) {
      EXPECT_EQ(sizeof(kTestPayload), payloadSize);
      // All our test vectors for PCMU are equal to |kTestPayload|.
      const size_t min_size = std::min(sizeof(kTestPayload), payloadSize);
      EXPECT_EQ(0, memcmp(payloadData, kTestPayload, min_size));
    } else if (payload_type == kDtmfPayloadType) {
      VerifyDtmf(payloadData, payloadSize);
    }

    return 0;
  }
};

}  // namespace

class RtpRtcpAudioTest : public ::testing::Test {
 protected:
  RtpRtcpAudioTest()
      : fake_clock_(123456), retransmission_rate_limiter_(&fake_clock_, 1000) {}
  ~RtpRtcpAudioTest() override = default;

  void SetUp() override {
    receive_statistics1_.reset(ReceiveStatistics::Create(&fake_clock_));
    receive_statistics2_.reset(ReceiveStatistics::Create(&fake_clock_));

    RtpRtcp::Configuration configuration;
    configuration.audio = true;
    configuration.clock = &fake_clock_;
    configuration.receive_statistics = receive_statistics1_.get();
    configuration.outgoing_transport = &transport1;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;

    module1.reset(RtpRtcp::CreateRtpRtcp(configuration));
    rtp_receiver1_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock_, &data_receiver1, &rtp_payload_registry1_));

    configuration.receive_statistics = receive_statistics2_.get();
    configuration.outgoing_transport = &transport2;

    module2.reset(RtpRtcp::CreateRtpRtcp(configuration));
    rtp_receiver2_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock_, &data_receiver2, &rtp_payload_registry2_));

    transport1.SetSendModule(module2.get(), &rtp_payload_registry2_,
                             rtp_receiver2_.get(), receive_statistics2_.get());
    transport2.SetSendModule(module1.get(), &rtp_payload_registry1_,
                             rtp_receiver1_.get(), receive_statistics1_.get());
  }

  void RegisterPayload(const CodecInst& codec) {
    EXPECT_EQ(0, module1->RegisterSendPayload(codec));
    EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(codec.pltype,
                                                        CodecInstToSdp(codec)));
    EXPECT_EQ(0, module2->RegisterSendPayload(codec));
    EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(codec.pltype,
                                                        CodecInstToSdp(codec)));
  }

  VerifyingAudioReceiver data_receiver1;
  VerifyingAudioReceiver data_receiver2;
  std::unique_ptr<ReceiveStatistics> receive_statistics1_;
  std::unique_ptr<ReceiveStatistics> receive_statistics2_;
  RTPPayloadRegistry rtp_payload_registry1_;
  RTPPayloadRegistry rtp_payload_registry2_;
  std::unique_ptr<RtpReceiver> rtp_receiver1_;
  std::unique_ptr<RtpReceiver> rtp_receiver2_;
  std::unique_ptr<RtpRtcp> module1;
  std::unique_ptr<RtpRtcp> module2;
  LoopBackTransport transport1;
  LoopBackTransport transport2;
  SimulatedClock fake_clock_;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpAudioTest, Basic) {
  module1->SetSSRC(kSsrc);
  module1->SetStartTimestamp(kTimestamp);

  // Test detection at the end of a DTMF tone.
  // EXPECT_EQ(0, module2->SetTelephoneEventForwardToDecoder(true));

  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Start basic RTP test.

  // Send an empty RTP packet.
  // Should fail since we have not registered the payload type.
  EXPECT_FALSE(module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                         kPcmuPayloadType, 0, -1, nullptr, 0,
                                         nullptr, nullptr, nullptr));

  CodecInst voice_codec = {};
  voice_codec.pltype = kPcmuPayloadType;
  voice_codec.plfreq = 8000;
  voice_codec.rate = kTestRate;
  memcpy(voice_codec.plname, "PCMU", 5);
  RegisterPayload(voice_codec);

  EXPECT_TRUE(module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                        kPcmuPayloadType, 0, -1, kTestPayload,
                                        4, nullptr, nullptr, nullptr));

  EXPECT_EQ(kSsrc, rtp_receiver2_->SSRC());
  uint32_t timestamp;
  int64_t receive_time_ms;
  EXPECT_TRUE(
      rtp_receiver2_->GetLatestTimestamps(&timestamp, &receive_time_ms));
  EXPECT_EQ(kTimestamp, timestamp);
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), receive_time_ms);
}

TEST_F(RtpRtcpAudioTest, DTMF) {
  CodecInst voice_codec = {};
  voice_codec.pltype = kPcmuPayloadType;
  voice_codec.plfreq = 8000;
  voice_codec.rate = kTestRate;
  memcpy(voice_codec.plname, "PCMU", 5);
  RegisterPayload(voice_codec);

  module1->SetSSRC(kSsrc);
  module1->SetStartTimestamp(kTimestamp);
  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Prepare for DTMF.
  voice_codec.pltype = kDtmfPayloadType;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "telephone-event", 16);

  EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
                   voice_codec.pltype, CodecInstToSdp(voice_codec)));

  // Start DTMF test.
  int timeStamp = 160;

  // Send a DTMF tone using RFC 2833 (4733).
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(0, module1->SendTelephoneEventOutband(i, timeStamp, 10));
  }
  timeStamp += 160;  // Prepare for next packet.

  // Send RTP packets for 16 tones a 160 ms  100ms
  // pause between = 2560ms + 1600ms = 4160ms
  for (; timeStamp <= 250 * 160; timeStamp += 160) {
    EXPECT_TRUE(module1->SendOutgoingData(
        webrtc::kAudioFrameSpeech, kPcmuPayloadType, timeStamp, -1,
        kTestPayload, 4, nullptr, nullptr, nullptr));
    fake_clock_.AdvanceTimeMilliseconds(20);
    module1->Process();
  }
  EXPECT_EQ(0, module1->SendTelephoneEventOutband(32, 9000, 10));

  for (; timeStamp <= 740 * 160; timeStamp += 160) {
    EXPECT_TRUE(module1->SendOutgoingData(
        webrtc::kAudioFrameSpeech, kPcmuPayloadType, timeStamp, -1,
        kTestPayload, 4, nullptr, nullptr, nullptr));
    fake_clock_.AdvanceTimeMilliseconds(20);
    module1->Process();
  }
}

TEST_F(RtpRtcpAudioTest, ComfortNoise) {
  module1->SetSSRC(kSsrc);
  module1->SetStartTimestamp(kTimestamp);

  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Register PCMU and all four comfort noise codecs
  CodecInst voice_codec = {};
  voice_codec.pltype = kPcmuPayloadType;
  voice_codec.plfreq = 8000;
  voice_codec.rate = kTestRate;
  memcpy(voice_codec.plname, "PCMU", 5);
  RegisterPayload(voice_codec);

  for (const auto& c : kCngCodecs) {
    CodecInst cng_codec = {};
    cng_codec.pltype = c.payload_type;
    cng_codec.plfreq = c.clockrate_hz;
    memcpy(cng_codec.plname, "CN", 3);
    RegisterPayload(cng_codec);
  }

  // Transmit comfort noise packets interleaved by PCMU packets.
  uint32_t in_timestamp = 0;
  for (const auto& c : kCngCodecs) {
    uint32_t timestamp;
    int64_t receive_time_ms;
    EXPECT_TRUE(module1->SendOutgoingData(
        webrtc::kAudioFrameSpeech, kPcmuPayloadType, in_timestamp, -1,
        kTestPayload, 4, nullptr, nullptr, nullptr));

    EXPECT_EQ(kSsrc, rtp_receiver2_->SSRC());
    EXPECT_TRUE(
        rtp_receiver2_->GetLatestTimestamps(&timestamp, &receive_time_ms));
    EXPECT_EQ(kTimestamp + in_timestamp, timestamp);
    EXPECT_EQ(fake_clock_.TimeInMilliseconds(), receive_time_ms);
    in_timestamp += 10;
    fake_clock_.AdvanceTimeMilliseconds(20);

    EXPECT_TRUE(module1->SendOutgoingData(webrtc::kAudioFrameCN, c.payload_type,
                                          in_timestamp, -1, kTestPayload, 1,
                                          nullptr, nullptr, nullptr));

    EXPECT_EQ(kSsrc, rtp_receiver2_->SSRC());
    EXPECT_TRUE(
        rtp_receiver2_->GetLatestTimestamps(&timestamp, &receive_time_ms));
    EXPECT_EQ(kTimestamp + in_timestamp, timestamp);
    EXPECT_EQ(fake_clock_.TimeInMilliseconds(), receive_time_ms);
    in_timestamp += 10;
    fake_clock_.AdvanceTimeMilliseconds(20);
  }
}

}  // namespace webrtc
