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

// This test uses fake packet transports and a fake media engine, in order to
// test the RtpTransport at only an API level. Any end-to-end test should go in
// ortcfactory_integrationtest.cc instead.
class RtpTransportTest : public testing::Test {
 public:
  RtpTransportTest() {
    fake_media_engine_ = new cricket::FakeMediaEngine();
    // Note: This doesn't need to use fake network classes, since it uses
    // FakePacketTransports.
    auto result = OrtcFactory::Create(
        nullptr, nullptr, nullptr, nullptr, nullptr,
        std::unique_ptr<cricket::MediaEngineInterface>(fake_media_engine_));
    ortc_factory_ = result.MoveValue();
  }

 protected:
  // Owned by |ortc_factory_|.
  cricket::FakeMediaEngine* fake_media_engine_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory_;
};

// Test GetRtpPacketTransport and GetRtcpPacketTransport, with and without RTCP
// muxing.
TEST_F(RtpTransportTest, GetPacketTransports) {
  rtc::FakePacketTransport rtp("rtp");
  rtc::FakePacketTransport rtcp("rtcp");
  // With muxed RTCP.
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  auto result = ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp,
                                                  nullptr, nullptr);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(&rtp, result.value()->GetRtpPacketTransport());
  EXPECT_EQ(nullptr, result.value()->GetRtcpPacketTransport());
  result.MoveValue().reset();
  // With non-muxed RTCP.
  rtcp_parameters.mux = false;
  result =
      ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp, &rtcp, nullptr);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(&rtp, result.value()->GetRtpPacketTransport());
  EXPECT_EQ(&rtcp, result.value()->GetRtcpPacketTransport());
}

// If an RtpTransport starts out un-muxed and then starts muxing, the RTCP
// packet transport should be forgotten and GetRtcpPacketTransport should
// return null.
TEST_F(RtpTransportTest, EnablingRtcpMuxingUnsetsRtcpTransport) {
  rtc::FakePacketTransport rtp("rtp");
  rtc::FakePacketTransport rtcp("rtcp");

  // Create non-muxed.
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = false;
  auto result =
      ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp, &rtcp, nullptr);
  ASSERT_TRUE(result.ok());
  auto rtp_transport = result.MoveValue();

  // Enable muxing.
  rtcp_parameters.mux = true;
  EXPECT_TRUE(rtp_transport->SetRtcpParameters(rtcp_parameters).ok());
  EXPECT_EQ(nullptr, rtp_transport->GetRtcpPacketTransport());
}

TEST_F(RtpTransportTest, GetAndSetRtcpParameters) {
  rtc::FakePacketTransport rtp("rtp");
  rtc::FakePacketTransport rtcp("rtcp");
  // Start with non-muxed RTCP.
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = false;
  rtcp_parameters.cname = "teST";
  rtcp_parameters.reduced_size = false;
  auto result =
      ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp, &rtcp, nullptr);
  ASSERT_TRUE(result.ok());
  auto transport = result.MoveValue();
  EXPECT_EQ(rtcp_parameters, transport->GetRtcpParameters());

  // Changing the CNAME is currently unsupported.
  rtcp_parameters.cname = "different";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            transport->SetRtcpParameters(rtcp_parameters).type());
  rtcp_parameters.cname = "teST";

  // Enable RTCP muxing and reduced-size RTCP.
  rtcp_parameters.mux = true;
  rtcp_parameters.reduced_size = true;
  EXPECT_TRUE(transport->SetRtcpParameters(rtcp_parameters).ok());
  EXPECT_EQ(rtcp_parameters, transport->GetRtcpParameters());

  // Empty CNAME should result in the existing CNAME being used.
  rtcp_parameters.cname.clear();
  EXPECT_TRUE(transport->SetRtcpParameters(rtcp_parameters).ok());
  EXPECT_EQ("teST", transport->GetRtcpParameters().cname);

  // Disabling RTCP muxing after enabling shouldn't be allowed, since enabling
  // muxing should have made the RTP transport forget about the RTCP packet
  // transport initially passed into it.
  rtcp_parameters.mux = false;
  EXPECT_EQ(RTCErrorType::INVALID_STATE,
            transport->SetRtcpParameters(rtcp_parameters).type());
}

// When Send or Receive is called on a sender or receiver, the RTCP parameters
// from the RtpTransport underneath the sender should be applied to the created
// media stream. The only relevant parameters (currently) are |cname| and
// |reduced_size|.
TEST_F(RtpTransportTest, SendAndReceiveApplyRtcpParametersToMediaEngine) {
  // First, create video transport with reduced-size RTCP.
  rtc::FakePacketTransport fake_packet_transport1("1");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  rtcp_parameters.reduced_size = true;
  rtcp_parameters.cname = "foo";
  auto rtp_transport_result = ortc_factory_->CreateRtpTransport(
      rtcp_parameters, &fake_packet_transport1, nullptr, nullptr);
  auto video_transport = rtp_transport_result.MoveValue();

  // Create video sender and call Send, expecting parameters to be applied.
  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_VIDEO,
                                                      video_transport.get());
  auto video_sender = sender_result.MoveValue();
  EXPECT_TRUE(video_sender->Send(MakeMinimalVp8Parameters()).ok());
  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  EXPECT_TRUE(fake_video_channel->send_rtcp_parameters().reduced_size);
  ASSERT_EQ(1u, fake_video_channel->send_streams().size());
  const cricket::StreamParams& video_send_stream =
      fake_video_channel->send_streams()[0];
  EXPECT_EQ("foo", video_send_stream.cname);

  // Create video receiver and call Receive, expecting parameters to be applied
  // (minus |cname|, since that's the sent cname, not received).
  auto receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, video_transport.get());
  auto video_receiver = receiver_result.MoveValue();
  EXPECT_TRUE(
      video_receiver->Receive(MakeMinimalVp8ParametersWithSsrc(0xdeadbeef))
          .ok());
  EXPECT_TRUE(fake_video_channel->recv_rtcp_parameters().reduced_size);

  // Create audio transport with non-reduced size RTCP.
  rtc::FakePacketTransport fake_packet_transport2("2");
  rtcp_parameters.reduced_size = false;
  rtcp_parameters.cname = "bar";
  rtp_transport_result = ortc_factory_->CreateRtpTransport(
      rtcp_parameters, &fake_packet_transport2, nullptr, nullptr);
  auto audio_transport = rtp_transport_result.MoveValue();

  // Create audio sender and call Send, expecting parameters to be applied.
  sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                                 audio_transport.get());
  auto audio_sender = sender_result.MoveValue();
  EXPECT_TRUE(audio_sender->Send(MakeMinimalIsacParameters()).ok());

  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  EXPECT_FALSE(fake_voice_channel->send_rtcp_parameters().reduced_size);
  ASSERT_EQ(1u, fake_voice_channel->send_streams().size());
  const cricket::StreamParams& audio_send_stream =
      fake_voice_channel->send_streams()[0];
  EXPECT_EQ("bar", audio_send_stream.cname);

  // Create audio receiver and call Receive, expecting parameters to be applied
  // (minus |cname|, since that's the sent cname, not received).
  receiver_result = ortc_factory_->CreateRtpReceiver(cricket::MEDIA_TYPE_AUDIO,
                                                     audio_transport.get());
  auto audio_receiver = receiver_result.MoveValue();
  EXPECT_TRUE(
      audio_receiver->Receive(MakeMinimalOpusParametersWithSsrc(0xbaadf00d))
          .ok());
  EXPECT_FALSE(fake_voice_channel->recv_rtcp_parameters().reduced_size);
}

// When SetRtcpParameters is called, the modified parameters should be applied
// to the media engine.
// TODO(deadbeef): Once the implementation supports changing the CNAME,
// test that here.
TEST_F(RtpTransportTest, SetRtcpParametersAppliesParametersToMediaEngine) {
  rtc::FakePacketTransport fake_packet_transport("fake");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  rtcp_parameters.reduced_size = false;
  auto rtp_transport_result = ortc_factory_->CreateRtpTransport(
      rtcp_parameters, &fake_packet_transport, nullptr, nullptr);
  auto rtp_transport = rtp_transport_result.MoveValue();

  // Create video sender and call Send, applying an initial set of parameters.
  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_VIDEO,
                                                      rtp_transport.get());
  auto sender = sender_result.MoveValue();
  EXPECT_TRUE(sender->Send(MakeMinimalVp8Parameters()).ok());

  // Modify parameters and expect them to be changed at the media engine level.
  rtcp_parameters.reduced_size = true;
  EXPECT_TRUE(rtp_transport->SetRtcpParameters(rtcp_parameters).ok());

  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  EXPECT_TRUE(fake_video_channel->send_rtcp_parameters().reduced_size);
}

}  // namespace webrtc
