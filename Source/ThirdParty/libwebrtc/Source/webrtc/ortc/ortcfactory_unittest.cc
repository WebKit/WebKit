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

#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/ortc/ortcfactory.h"
#include "webrtc/ortc/testrtpparameters.h"
#include "webrtc/p2p/base/fakepackettransport.h"

namespace webrtc {

// This test uses a virtual network and fake media engine, in order to test the
// OrtcFactory at only an API level. Any end-to-end test should go in
// ortcfactory_integrationtest.cc instead.
class OrtcFactoryTest : public testing::Test {
 public:
  OrtcFactoryTest()
      : virtual_socket_server_(&physical_socket_server_),
        socket_server_scope_(&virtual_socket_server_),
        fake_packet_transport_("fake transport") {
    ortc_factory_ =
        OrtcFactory::Create(nullptr, nullptr, &fake_network_manager_, nullptr,
                            nullptr,
                            std::unique_ptr<cricket::MediaEngineInterface>(
                                new cricket::FakeMediaEngine()))
            .MoveValue();
  }

 protected:
  // Uses a single pre-made FakePacketTransport, so shouldn't be called twice in
  // the same test.
  std::unique_ptr<RtpTransportInterface>
  CreateRtpTransportWithFakePacketTransport() {
    return ortc_factory_
        ->CreateRtpTransport(MakeRtcpMuxParameters(), &fake_packet_transport_,
                             nullptr, nullptr)
        .MoveValue();
  }

  rtc::PhysicalSocketServer physical_socket_server_;
  rtc::VirtualSocketServer virtual_socket_server_;
  rtc::SocketServerScope socket_server_scope_;
  rtc::FakeNetworkManager fake_network_manager_;
  rtc::FakePacketTransport fake_packet_transport_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory_;
};

TEST_F(OrtcFactoryTest, CanCreateMultipleRtpTransportControllers) {
  auto controller_result1 = ortc_factory_->CreateRtpTransportController();
  EXPECT_TRUE(controller_result1.ok());
  auto controller_result2 = ortc_factory_->CreateRtpTransportController();
  EXPECT_TRUE(controller_result1.ok());
}

// Simple test for the successful cases of CreateRtpTransport.
TEST_F(OrtcFactoryTest, CreateRtpTransportWithAndWithoutMux) {
  rtc::FakePacketTransport rtp("rtp");
  rtc::FakePacketTransport rtcp("rtcp");
  // With muxed RTCP.
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  auto result = ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp,
                                                  nullptr, nullptr);
  EXPECT_TRUE(result.ok());
  result.MoveValue().reset();
  // With non-muxed RTCP.
  rtcp_parameters.mux = false;
  result =
      ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp, &rtcp, nullptr);
  EXPECT_TRUE(result.ok());
}

// If no CNAME is provided, one should be generated and returned by
// GetRtpParameters.
TEST_F(OrtcFactoryTest, CreateRtpTransportGeneratesCname) {
  rtc::FakePacketTransport rtp("rtp");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  auto result = ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp,
                                                  nullptr, nullptr);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.value()->GetRtcpParameters().cname.empty());
}

// Extension of the above test; multiple transports created by the same factory
// should use the same generated CNAME.
TEST_F(OrtcFactoryTest, MultipleRtpTransportsUseSameGeneratedCname) {
  rtc::FakePacketTransport packet_transport1("1");
  rtc::FakePacketTransport packet_transport2("2");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  // Sanity check.
  ASSERT_TRUE(rtcp_parameters.cname.empty());
  auto result = ortc_factory_->CreateRtpTransport(
      rtcp_parameters, &packet_transport1, nullptr, nullptr);
  ASSERT_TRUE(result.ok());
  auto rtp_transport1 = result.MoveValue();
  result = ortc_factory_->CreateRtpTransport(
      rtcp_parameters, &packet_transport2, nullptr, nullptr);
  ASSERT_TRUE(result.ok());
  auto rtp_transport2 = result.MoveValue();
  RtcpParameters params1 = rtp_transport1->GetRtcpParameters();
  RtcpParameters params2 = rtp_transport2->GetRtcpParameters();
  EXPECT_FALSE(params1.cname.empty());
  EXPECT_EQ(params1.cname, params2.cname);
}

TEST_F(OrtcFactoryTest, CreateRtpTransportWithNoPacketTransport) {
  auto result = ortc_factory_->CreateRtpTransport(MakeRtcpMuxParameters(),
                                                  nullptr, nullptr, nullptr);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

// If the |mux| member of the RtcpParameters is false, both an RTP and RTCP
// packet transport are needed.
TEST_F(OrtcFactoryTest, CreateRtpTransportWithMissingRtcpTransport) {
  rtc::FakePacketTransport rtp("rtp");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = false;
  auto result = ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp,
                                                  nullptr, nullptr);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

// If the |mux| member of the RtcpParameters is true, only an RTP packet
// transport is necessary. So, passing in an RTCP transport is most likely
// an accident, and thus should be treated as an error.
TEST_F(OrtcFactoryTest, CreateRtpTransportWithExtraneousRtcpTransport) {
  rtc::FakePacketTransport rtp("rtp");
  rtc::FakePacketTransport rtcp("rtcp");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  auto result =
      ortc_factory_->CreateRtpTransport(rtcp_parameters, &rtp, &rtcp, nullptr);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

// Basic test that CreateUdpTransport works with AF_INET and AF_INET6.
TEST_F(OrtcFactoryTest, CreateUdpTransport) {
  auto result = ortc_factory_->CreateUdpTransport(AF_INET);
  EXPECT_TRUE(result.ok());
  result = ortc_factory_->CreateUdpTransport(AF_INET6);
  EXPECT_TRUE(result.ok());
}

// Test CreateUdpPort with the |min_port| and |max_port| arguments.
TEST_F(OrtcFactoryTest, CreateUdpTransportWithPortRange) {
  auto socket_result1 = ortc_factory_->CreateUdpTransport(AF_INET, 2000, 2002);
  ASSERT_TRUE(socket_result1.ok());
  EXPECT_EQ(2000, socket_result1.value()->GetLocalAddress().port());
  auto socket_result2 = ortc_factory_->CreateUdpTransport(AF_INET, 2000, 2002);
  ASSERT_TRUE(socket_result2.ok());
  EXPECT_EQ(2001, socket_result2.value()->GetLocalAddress().port());
  auto socket_result3 = ortc_factory_->CreateUdpTransport(AF_INET, 2000, 2002);
  ASSERT_TRUE(socket_result3.ok());
  EXPECT_EQ(2002, socket_result3.value()->GetLocalAddress().port());

  // All sockets in the range have been exhausted, so the next call should
  // fail.
  auto failed_result = ortc_factory_->CreateUdpTransport(AF_INET, 2000, 2002);
  EXPECT_EQ(RTCErrorType::RESOURCE_EXHAUSTED, failed_result.error().type());

  // If one socket is destroyed, that port should be freed up again.
  socket_result2.MoveValue().reset();
  auto socket_result4 = ortc_factory_->CreateUdpTransport(AF_INET, 2000, 2002);
  ASSERT_TRUE(socket_result4.ok());
  EXPECT_EQ(2001, socket_result4.value()->GetLocalAddress().port());
}

// Basic test that CreateUdpTransport works with AF_INET and AF_INET6.
TEST_F(OrtcFactoryTest, CreateUdpTransportWithInvalidAddressFamily) {
  auto result = ortc_factory_->CreateUdpTransport(12345);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

TEST_F(OrtcFactoryTest, CreateUdpTransportWithInvalidPortRange) {
  auto result = ortc_factory_->CreateUdpTransport(AF_INET, 3000, 2000);
  EXPECT_EQ(RTCErrorType::INVALID_RANGE, result.error().type());
}

// Just sanity check that each "GetCapabilities" method returns some codecs.
TEST_F(OrtcFactoryTest, GetSenderAndReceiverCapabilities) {
  RtpCapabilities audio_send_caps =
      ortc_factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_GT(audio_send_caps.codecs.size(), 0u);
  RtpCapabilities video_send_caps =
      ortc_factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_GT(video_send_caps.codecs.size(), 0u);
  RtpCapabilities audio_receive_caps =
      ortc_factory_->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_GT(audio_receive_caps.codecs.size(), 0u);
  RtpCapabilities video_receive_caps =
      ortc_factory_->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_GT(video_receive_caps.codecs.size(), 0u);
}

// Calling CreateRtpSender with a null track should fail, since that makes it
// impossible to know whether to create an audio or video sender. The
// application should be using the method that takes a cricket::MediaType
// instead.
TEST_F(OrtcFactoryTest, CreateSenderWithNullTrack) {
  auto rtp_transport = CreateRtpTransportWithFakePacketTransport();
  auto result = ortc_factory_->CreateRtpSender(nullptr, rtp_transport.get());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

// Calling CreateRtpSender or CreateRtpReceiver with MEDIA_TYPE_DATA should
// fail.
TEST_F(OrtcFactoryTest, CreateSenderOrReceieverWithInvalidKind) {
  auto rtp_transport = CreateRtpTransportWithFakePacketTransport();
  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_DATA,
                                                      rtp_transport.get());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, sender_result.error().type());
  auto receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_DATA, rtp_transport.get());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, receiver_result.error().type());
}

TEST_F(OrtcFactoryTest, CreateSendersOrReceieversWithNullTransport) {
  auto sender_result =
      ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO, nullptr);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, sender_result.error().type());
  auto receiver_result =
      ortc_factory_->CreateRtpReceiver(cricket::MEDIA_TYPE_AUDIO, nullptr);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, receiver_result.error().type());
}

}  // namespace webrtc
