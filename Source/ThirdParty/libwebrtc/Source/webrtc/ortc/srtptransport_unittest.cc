/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/gunit.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/ortc/ortcfactory.h"
#include "webrtc/ortc/testrtpparameters.h"
#include "webrtc/p2p/base/fakepackettransport.h"

namespace webrtc {

static const char kTestSha1KeyParams1[] =
    "inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz";
static const char kTestSha1KeyParams2[] =
    "inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR";
static const char kTestGcmKeyParams3[] =
    "inline:e166KFlKzJsGW0d5apX+rrI05vxbrvMJEzFI14aTDCa63IRTlLK4iH66uOI=";

static const cricket::CryptoParams kTestSha1CryptoParams1(
    1,
    "AES_CM_128_HMAC_SHA1_80",
    kTestSha1KeyParams1,
    "");
static const cricket::CryptoParams kTestSha1CryptoParams2(
    1,
    "AES_CM_128_HMAC_SHA1_80",
    kTestSha1KeyParams2,
    "");
static const cricket::CryptoParams kTestGcmCryptoParams3(1,
                                                         "AEAD_AES_256_GCM",
                                                         kTestGcmKeyParams3,
                                                         "");

// This test uses fake packet transports and a fake media engine, in order to
// test the SrtpTransport at only an API level. Any end-to-end test should go in
// ortcfactory_integrationtest.cc instead.
class SrtpTransportTest : public testing::Test {
 public:
  SrtpTransportTest() {
    fake_media_engine_ = new cricket::FakeMediaEngine();
    // Note: This doesn't need to use fake network classes, since it uses
    // FakePacketTransports.
    auto result = OrtcFactory::Create(
        nullptr, nullptr, nullptr, nullptr, nullptr,
        std::unique_ptr<cricket::MediaEngineInterface>(fake_media_engine_));
    ortc_factory_ = result.MoveValue();
    rtp_transport_controller_ =
        ortc_factory_->CreateRtpTransportController().MoveValue();

    fake_packet_transport_.reset(new rtc::FakePacketTransport("fake"));
    auto srtp_transport_result = ortc_factory_->CreateSrtpTransport(
        rtcp_parameters_, fake_packet_transport_.get(), nullptr,
        rtp_transport_controller_.get());
    srtp_transport_ = srtp_transport_result.MoveValue();
  }

 protected:
  // Owned by |ortc_factory_|.
  cricket::FakeMediaEngine* fake_media_engine_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory_;
  std::unique_ptr<RtpTransportControllerInterface> rtp_transport_controller_;
  std::unique_ptr<SrtpTransportInterface> srtp_transport_;
  RtcpParameters rtcp_parameters_;
  std::unique_ptr<rtc::FakePacketTransport> fake_packet_transport_;
};

// Tests that setting the SRTP send/receive key succeeds.
TEST_F(SrtpTransportTest, SetSrtpSendAndReceiveKey) {
  EXPECT_TRUE(srtp_transport_->SetSrtpSendKey(kTestSha1CryptoParams1).ok());
  EXPECT_TRUE(srtp_transport_->SetSrtpReceiveKey(kTestSha1CryptoParams2).ok());
  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                                      srtp_transport_.get());
  EXPECT_TRUE(sender_result.ok());
  auto receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, srtp_transport_.get());
  EXPECT_TRUE(receiver_result.ok());
}

// Tests that setting the SRTP send/receive key twice is not supported.
TEST_F(SrtpTransportTest, SetSrtpSendAndReceiveKeyTwice) {
  EXPECT_TRUE(srtp_transport_->SetSrtpSendKey(kTestSha1CryptoParams1).ok());
  EXPECT_TRUE(srtp_transport_->SetSrtpReceiveKey(kTestSha1CryptoParams2).ok());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            srtp_transport_->SetSrtpSendKey(kTestSha1CryptoParams2).type());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            srtp_transport_->SetSrtpReceiveKey(kTestSha1CryptoParams1).type());
  // Ensure that the senders and receivers can be created despite the previous
  // errors.
  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                                      srtp_transport_.get());
  EXPECT_TRUE(sender_result.ok());
  auto receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, srtp_transport_.get());
  EXPECT_TRUE(receiver_result.ok());
}

// Test that the SRTP send key and receive key must have the same cipher suite.
TEST_F(SrtpTransportTest, SetSrtpSendAndReceiveKeyDifferentCipherSuite) {
  EXPECT_TRUE(srtp_transport_->SetSrtpSendKey(kTestSha1CryptoParams1).ok());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            srtp_transport_->SetSrtpReceiveKey(kTestGcmCryptoParams3).type());
  EXPECT_TRUE(srtp_transport_->SetSrtpReceiveKey(kTestSha1CryptoParams2).ok());
  // Ensure that the senders and receivers can be created despite the previous
  // error.
  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                                      srtp_transport_.get());
  EXPECT_TRUE(sender_result.ok());
  auto receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, srtp_transport_.get());
  EXPECT_TRUE(receiver_result.ok());
}

class SrtpTransportTestWithMediaType
    : public SrtpTransportTest,
      public ::testing::WithParamInterface<cricket::MediaType> {};

// Tests that the senders cannot be created before setting the keys.
TEST_P(SrtpTransportTestWithMediaType, CreateSenderBeforeSettingKeys) {
  auto sender_result =
      ortc_factory_->CreateRtpSender(GetParam(), srtp_transport_.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER, sender_result.error().type());
  EXPECT_TRUE(srtp_transport_->SetSrtpSendKey(kTestSha1CryptoParams1).ok());
  sender_result =
      ortc_factory_->CreateRtpSender(GetParam(), srtp_transport_.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER, sender_result.error().type());
  EXPECT_TRUE(srtp_transport_->SetSrtpReceiveKey(kTestSha1CryptoParams2).ok());
  // Ensure that after the keys are set, a sender can be created, despite the
  // previous errors.
  sender_result =
      ortc_factory_->CreateRtpSender(GetParam(), srtp_transport_.get());
  EXPECT_TRUE(sender_result.ok());
}

// Tests that the receivers cannot be created before setting the keys.
TEST_P(SrtpTransportTestWithMediaType, CreateReceiverBeforeSettingKeys) {
  auto receiver_result =
      ortc_factory_->CreateRtpReceiver(GetParam(), srtp_transport_.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            receiver_result.error().type());
  EXPECT_TRUE(srtp_transport_->SetSrtpSendKey(kTestSha1CryptoParams1).ok());
  receiver_result =
      ortc_factory_->CreateRtpReceiver(GetParam(), srtp_transport_.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            receiver_result.error().type());
  EXPECT_TRUE(srtp_transport_->SetSrtpReceiveKey(kTestSha1CryptoParams2).ok());
  // Ensure that after the keys are set, a receiver can be created, despite the
  // previous errors.
  receiver_result =
      ortc_factory_->CreateRtpReceiver(GetParam(), srtp_transport_.get());
  EXPECT_TRUE(receiver_result.ok());
}

INSTANTIATE_TEST_CASE_P(SenderCreatationTest,
                        SrtpTransportTestWithMediaType,
                        ::testing::Values(cricket::MEDIA_TYPE_AUDIO,
                                          cricket::MEDIA_TYPE_VIDEO));

}  // namespace webrtc
