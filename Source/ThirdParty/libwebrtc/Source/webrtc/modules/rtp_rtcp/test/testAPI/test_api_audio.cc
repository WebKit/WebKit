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

#include "webrtc/base/rate_limiter.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_audio.h"
#include "webrtc/modules/rtp_rtcp/test/testAPI/test_api.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

const uint32_t kTestRate = 64000u;
const uint8_t kTestPayload[] = { 't', 'e', 's', 't' };
const uint8_t kPcmuPayloadType = 96;
const uint8_t kDtmfPayloadType = 97;

struct CngCodecSpec {
  int payload_type;
  int clockrate_hz;
};

const CngCodecSpec kCngCodecs[] = {{13, 8000},
                                   {103, 16000},
                                   {104, 32000},
                                   {105, 48000}};

bool IsComfortNoisePayload(uint8_t payload_type) {
  for (const auto& c : kCngCodecs) {
    if (c.payload_type == payload_type)
      return true;
  }

  return false;
}

class VerifyingAudioReceiver : public NullRtpData {
 public:
  int32_t OnReceivedPayloadData(
      const uint8_t* payloadData,
      size_t payloadSize,
      const webrtc::WebRtcRTPHeader* rtpHeader) override {
    const uint8_t payload_type = rtpHeader->header.payloadType;
    if (payload_type == kPcmuPayloadType || payload_type == kDtmfPayloadType) {
      EXPECT_EQ(sizeof(kTestPayload), payloadSize);
      // All our test vectors for PCMU and DTMF are equal to |kTestPayload|.
      const size_t min_size = std::min(sizeof(kTestPayload), payloadSize);
      EXPECT_EQ(0, memcmp(payloadData, kTestPayload, min_size));
    } else if (IsComfortNoisePayload(payload_type)) {
      // CNG types should be recognized properly.
      EXPECT_EQ(kAudioFrameCN, rtpHeader->frameType);
      EXPECT_TRUE(rtpHeader->type.Audio.isCNG);
    }
    return 0;
  }
};

class RTPCallback : public NullRtpFeedback {
 public:
  int32_t OnInitializeDecoder(int8_t payloadType,
                              const char payloadName[RTP_PAYLOAD_NAME_SIZE],
                              int frequency,
                              size_t channels,
                              uint32_t rate) override {
    EXPECT_EQ(0u, rate) << "The rate should be zero";
    return 0;
  }
};

}  // namespace

class RtpRtcpAudioTest : public ::testing::Test {
 protected:
  RtpRtcpAudioTest()
      : fake_clock(123456), retransmission_rate_limiter_(&fake_clock, 1000) {
    test_CSRC[0] = 1234;
    test_CSRC[2] = 2345;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpAudioTest() {}

  void SetUp() override {
    receive_statistics1_.reset(ReceiveStatistics::Create(&fake_clock));
    receive_statistics2_.reset(ReceiveStatistics::Create(&fake_clock));

    rtp_payload_registry1_.reset(new RTPPayloadRegistry());
    rtp_payload_registry2_.reset(new RTPPayloadRegistry());

    RtpRtcp::Configuration configuration;
    configuration.audio = true;
    configuration.clock = &fake_clock;
    configuration.receive_statistics = receive_statistics1_.get();
    configuration.outgoing_transport = &transport1;
    configuration.retransmission_rate_limiter = &retransmission_rate_limiter_;

    module1.reset(RtpRtcp::CreateRtpRtcp(configuration));
    rtp_receiver1_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock, &data_receiver1, &rtp_callback,
        rtp_payload_registry1_.get()));

    configuration.receive_statistics = receive_statistics2_.get();
    configuration.outgoing_transport = &transport2;

    module2.reset(RtpRtcp::CreateRtpRtcp(configuration));
    rtp_receiver2_.reset(RtpReceiver::CreateAudioReceiver(
        &fake_clock, &data_receiver2, &rtp_callback,
        rtp_payload_registry2_.get()));

    transport1.SetSendModule(module2.get(), rtp_payload_registry2_.get(),
                             rtp_receiver2_.get(), receive_statistics2_.get());
    transport2.SetSendModule(module1.get(), rtp_payload_registry1_.get(),
                             rtp_receiver1_.get(), receive_statistics1_.get());
  }

  void RegisterPayload(const CodecInst& codec) {
    EXPECT_EQ(0, module1->RegisterSendPayload(codec));
    EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(codec));
    EXPECT_EQ(0, module2->RegisterSendPayload(codec));
    EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(codec));
  }

  VerifyingAudioReceiver data_receiver1;
  VerifyingAudioReceiver data_receiver2;
  RTPCallback rtp_callback;
  std::unique_ptr<ReceiveStatistics> receive_statistics1_;
  std::unique_ptr<ReceiveStatistics> receive_statistics2_;
  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry1_;
  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry2_;
  std::unique_ptr<RtpReceiver> rtp_receiver1_;
  std::unique_ptr<RtpReceiver> rtp_receiver2_;
  std::unique_ptr<RtpRtcp> module1;
  std::unique_ptr<RtpRtcp> module2;
  LoopBackTransport transport1;
  LoopBackTransport transport2;
  uint32_t test_ssrc;
  uint32_t test_timestamp;
  uint16_t test_sequence_number;
  uint32_t test_CSRC[webrtc::kRtpCsrcSize];
  SimulatedClock fake_clock;
  RateLimiter retransmission_rate_limiter_;
};

TEST_F(RtpRtcpAudioTest, Basic) {
  module1->SetSSRC(test_ssrc);
  module1->SetStartTimestamp(test_timestamp);

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

  EXPECT_EQ(test_ssrc, rtp_receiver2_->SSRC());
  uint32_t timestamp;
  EXPECT_TRUE(rtp_receiver2_->Timestamp(&timestamp));
  EXPECT_EQ(test_timestamp, timestamp);
}

TEST_F(RtpRtcpAudioTest, DTMF) {
  CodecInst voice_codec = {};
  voice_codec.pltype = kPcmuPayloadType;
  voice_codec.plfreq = 8000;
  voice_codec.rate = kTestRate;
  memcpy(voice_codec.plname, "PCMU", 5);
  RegisterPayload(voice_codec);

  module1->SetSSRC(test_ssrc);
  module1->SetStartTimestamp(test_timestamp);
  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Prepare for DTMF.
  voice_codec.pltype = kDtmfPayloadType;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "telephone-event", 16);

  EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(voice_codec));

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
    fake_clock.AdvanceTimeMilliseconds(20);
    module1->Process();
  }
  EXPECT_EQ(0, module1->SendTelephoneEventOutband(32, 9000, 10));

  for (; timeStamp <= 740 * 160; timeStamp += 160) {
    EXPECT_TRUE(module1->SendOutgoingData(
        webrtc::kAudioFrameSpeech, kPcmuPayloadType, timeStamp, -1,
        kTestPayload, 4, nullptr, nullptr, nullptr));
    fake_clock.AdvanceTimeMilliseconds(20);
    module1->Process();
  }
}

TEST_F(RtpRtcpAudioTest, ComfortNoise) {
  module1->SetSSRC(test_ssrc);
  module1->SetStartTimestamp(test_timestamp);

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
    EXPECT_TRUE(module1->SendOutgoingData(
        webrtc::kAudioFrameSpeech, kPcmuPayloadType, in_timestamp, -1,
        kTestPayload, 4, nullptr, nullptr, nullptr));

    EXPECT_EQ(test_ssrc, rtp_receiver2_->SSRC());
    EXPECT_TRUE(rtp_receiver2_->Timestamp(&timestamp));
    EXPECT_EQ(test_timestamp + in_timestamp, timestamp);
    in_timestamp += 10;

    EXPECT_TRUE(module1->SendOutgoingData(webrtc::kAudioFrameCN, c.payload_type,
                                          in_timestamp, -1, kTestPayload, 1,
                                          nullptr, nullptr, nullptr));

    EXPECT_EQ(test_ssrc, rtp_receiver2_->SSRC());
    EXPECT_TRUE(rtp_receiver2_->Timestamp(&timestamp));
    EXPECT_EQ(test_timestamp + in_timestamp, timestamp);
    in_timestamp += 10;
  }
}

}  // namespace webrtc
