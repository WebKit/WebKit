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

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "media/base/fakemediaengine.h"
#include "ortc/ortcfactory.h"
#include "ortc/testrtpparameters.h"
#include "p2p/base/fakepackettransport.h"
#include "rtc_base/gunit.h"

namespace webrtc {

// This test uses fake packet transports and a fake media engine, in order to
// test the RtpTransportController at only an API level. Any end-to-end test
// should go in ortcfactory_integrationtest.cc instead.
//
// Currently, this test mainly focuses on the limitations of the "adapter"
// RtpTransportController implementation. Only one of each type of
// sender/receiver can be created, and the sender/receiver of the same media
// type must use the same transport.
class RtpTransportControllerTest : public testing::Test {
 public:
  RtpTransportControllerTest() {
    // Note: This doesn't need to use fake network classes, since it uses
    // FakePacketTransports.
    auto result = OrtcFactory::Create(
        nullptr, nullptr, nullptr, nullptr, nullptr,
        std::unique_ptr<cricket::MediaEngineInterface>(
            new cricket::FakeMediaEngine()),
        CreateBuiltinAudioEncoderFactory(), CreateBuiltinAudioDecoderFactory());
    ortc_factory_ = result.MoveValue();
    rtp_transport_controller_ =
        ortc_factory_->CreateRtpTransportController().MoveValue();
  }

 protected:
  std::unique_ptr<OrtcFactoryInterface> ortc_factory_;
  std::unique_ptr<RtpTransportControllerInterface> rtp_transport_controller_;
};

TEST_F(RtpTransportControllerTest, GetTransports) {
  rtc::FakePacketTransport packet_transport1("one");
  rtc::FakePacketTransport packet_transport2("two");

  auto rtp_transport_result1 = ortc_factory_->CreateRtpTransport(
      MakeRtcpMuxParameters(), &packet_transport1, nullptr,
      rtp_transport_controller_.get());
  ASSERT_TRUE(rtp_transport_result1.ok());

  auto rtp_transport_result2 = ortc_factory_->CreateRtpTransport(
      MakeRtcpMuxParameters(), &packet_transport2, nullptr,
      rtp_transport_controller_.get());
  ASSERT_TRUE(rtp_transport_result2.ok());

  auto returned_transports = rtp_transport_controller_->GetTransports();
  ASSERT_EQ(2u, returned_transports.size());
  EXPECT_EQ(rtp_transport_result1.value().get(), returned_transports[0]);
  EXPECT_EQ(rtp_transport_result2.value().get(), returned_transports[1]);

  // If a transport is deleted, it shouldn't be returned any more.
  rtp_transport_result1.MoveValue().reset();
  returned_transports = rtp_transport_controller_->GetTransports();
  ASSERT_EQ(1u, returned_transports.size());
  EXPECT_EQ(rtp_transport_result2.value().get(), returned_transports[0]);
}

// Create RtpSenders and RtpReceivers on top of RtpTransports controlled by the
// same RtpTransportController. Currently only one each of audio/video is
// supported.
TEST_F(RtpTransportControllerTest, AttachMultipleSendersAndReceivers) {
  rtc::FakePacketTransport audio_packet_transport("audio");
  rtc::FakePacketTransport video_packet_transport("video");

  auto audio_rtp_transport_result = ortc_factory_->CreateRtpTransport(
      MakeRtcpMuxParameters(), &audio_packet_transport, nullptr,
      rtp_transport_controller_.get());
  ASSERT_TRUE(audio_rtp_transport_result.ok());
  auto audio_rtp_transport = audio_rtp_transport_result.MoveValue();

  auto video_rtp_transport_result = ortc_factory_->CreateRtpTransport(
      MakeRtcpMuxParameters(), &video_packet_transport, nullptr,
      rtp_transport_controller_.get());
  ASSERT_TRUE(video_rtp_transport_result.ok());
  auto video_rtp_transport = video_rtp_transport_result.MoveValue();

  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, audio_rtp_transport.get());
  EXPECT_TRUE(audio_sender_result.ok());
  auto audio_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, audio_rtp_transport.get());
  EXPECT_TRUE(audio_receiver_result.ok());
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, video_rtp_transport.get());
  EXPECT_TRUE(video_sender_result.ok());
  auto video_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, video_rtp_transport.get());
  EXPECT_TRUE(video_receiver_result.ok());

  // Now that we have one each of audio/video senders/receivers, trying to
  // create more on top of the same controller is expected to fail.
  // TODO(deadbeef): Update this test once multiple senders/receivers on top of
  // the same controller is supported.
  auto failed_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, audio_rtp_transport.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            failed_sender_result.error().type());
  auto failed_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, audio_rtp_transport.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            failed_receiver_result.error().type());
  failed_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, video_rtp_transport.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            failed_sender_result.error().type());
  failed_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, video_rtp_transport.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            failed_receiver_result.error().type());

  // If we destroy the existing sender/receiver using a transport controller,
  // we should be able to make a new one, despite the above limitation.
  audio_sender_result.MoveValue().reset();
  audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, audio_rtp_transport.get());
  EXPECT_TRUE(audio_sender_result.ok());
  audio_receiver_result.MoveValue().reset();
  audio_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, audio_rtp_transport.get());
  EXPECT_TRUE(audio_receiver_result.ok());
  video_sender_result.MoveValue().reset();
  video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, video_rtp_transport.get());
  EXPECT_TRUE(video_sender_result.ok());
  video_receiver_result.MoveValue().reset();
  video_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, video_rtp_transport.get());
  EXPECT_TRUE(video_receiver_result.ok());
}

// Given the current limitations of the BaseChannel-based implementation, it's
// not possible for an audio sender and receiver to use different RtpTransports.
// TODO(deadbeef): Once this is supported, update/replace this test.
TEST_F(RtpTransportControllerTest,
       SenderAndReceiverUsingDifferentTransportsUnsupported) {
  rtc::FakePacketTransport packet_transport1("one");
  rtc::FakePacketTransport packet_transport2("two");

  auto rtp_transport_result1 = ortc_factory_->CreateRtpTransport(
      MakeRtcpMuxParameters(), &packet_transport1, nullptr,
      rtp_transport_controller_.get());
  ASSERT_TRUE(rtp_transport_result1.ok());
  auto rtp_transport1 = rtp_transport_result1.MoveValue();

  auto rtp_transport_result2 = ortc_factory_->CreateRtpTransport(
      MakeRtcpMuxParameters(), &packet_transport2, nullptr,
      rtp_transport_controller_.get());
  ASSERT_TRUE(rtp_transport_result2.ok());
  auto rtp_transport2 = rtp_transport_result2.MoveValue();

  // Create an audio sender on transport 1, then try to create a receiver on 2.
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport1.get());
  EXPECT_TRUE(audio_sender_result.ok());
  auto audio_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport2.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            audio_receiver_result.error().type());
  // Delete the sender; now we should be ok to create the receiver on 2.
  audio_sender_result.MoveValue().reset();
  audio_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport2.get());
  EXPECT_TRUE(audio_receiver_result.ok());

  // Do the same thing for video, reversing 1 and 2 (for variety).
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport2.get());
  EXPECT_TRUE(video_sender_result.ok());
  auto video_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport1.get());
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            video_receiver_result.error().type());
  video_sender_result.MoveValue().reset();
  video_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport1.get());
  EXPECT_TRUE(video_receiver_result.ok());
}

}  // namespace webrtc
