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
#include <utility>  // For std::pair, std::move.

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/ortc/ortcfactoryinterface.h"
#include "ortc/testrtpparameters.h"
#include "p2p/base/udptransport.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakeperiodicvideotracksource.h"
#include "pc/test/fakevideotrackrenderer.h"
#include "pc/videotracksource.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/gunit.h"
#include "rtc_base/system/arch.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/virtualsocketserver.h"

namespace {

const int kDefaultTimeout = 10000;    // 10 seconds.
const int kReceivingDuration = 1000;  // 1 second.

// Default number of audio/video frames to wait for before considering a test a
// success.
const int kDefaultNumFrames = 3;
const rtc::IPAddress kIPv4LocalHostAddress =
    rtc::IPAddress(0x7F000001);  // 127.0.0.1

static const char kTestKeyParams1[] =
    "inline:WVNfX19zZW1jdGwgKskgewkyMjA7fQp9CnVubGVz";
static const char kTestKeyParams2[] =
    "inline:PS1uQCVeeCFCanVmcjkpaywjNWhcYD0mXXtxaVBR";
static const char kTestKeyParams3[] =
    "inline:WVNfX19zZW1jdGwgKskgewkyMjA7fQp9CnVubGVa";
static const char kTestKeyParams4[] =
    "inline:WVNfX19zZW1jdGwgKskgewkyMjA7fQp9CnVubGVb";
static const cricket::CryptoParams kTestCryptoParams1(1,
                                                      "AES_CM_128_HMAC_SHA1_80",
                                                      kTestKeyParams1,
                                                      "");
static const cricket::CryptoParams kTestCryptoParams2(1,
                                                      "AES_CM_128_HMAC_SHA1_80",
                                                      kTestKeyParams2,
                                                      "");
static const cricket::CryptoParams kTestCryptoParams3(1,
                                                      "AES_CM_128_HMAC_SHA1_80",
                                                      kTestKeyParams3,
                                                      "");
static const cricket::CryptoParams kTestCryptoParams4(1,
                                                      "AES_CM_128_HMAC_SHA1_80",
                                                      kTestKeyParams4,
                                                      "");
}  // namespace

namespace webrtc {

// Used to test that things work end-to-end when using the default
// implementations of threads/etc. provided by OrtcFactory, with the exception
// of using a virtual network.
//
// By default, the virtual network manager doesn't enumerate any networks, but
// sockets can still be created in this state.
class OrtcFactoryIntegrationTest : public testing::Test {
 public:
  OrtcFactoryIntegrationTest()
      : network_thread_(&virtual_socket_server_),
        fake_audio_capture_module1_(FakeAudioCaptureModule::Create()),
        fake_audio_capture_module2_(FakeAudioCaptureModule::Create()) {
    // Sockets are bound to the ANY address, so this is needed to tell the
    // virtual network which address to use in this case.
    virtual_socket_server_.SetDefaultRoute(kIPv4LocalHostAddress);
    network_thread_.SetName("TestNetworkThread", this);
    network_thread_.Start();
    // Need to create after network thread is started.
    ortc_factory1_ =
        OrtcFactoryInterface::Create(
            &network_thread_, nullptr, &fake_network_manager_, nullptr,
            fake_audio_capture_module1_, CreateBuiltinAudioEncoderFactory(),
            CreateBuiltinAudioDecoderFactory())
            .MoveValue();
    ortc_factory2_ =
        OrtcFactoryInterface::Create(
            &network_thread_, nullptr, &fake_network_manager_, nullptr,
            fake_audio_capture_module2_, CreateBuiltinAudioEncoderFactory(),
            CreateBuiltinAudioDecoderFactory())
            .MoveValue();
  }

 protected:
  typedef std::pair<std::unique_ptr<UdpTransportInterface>,
                    std::unique_ptr<UdpTransportInterface>>
      UdpTransportPair;
  typedef std::pair<std::unique_ptr<RtpTransportInterface>,
                    std::unique_ptr<RtpTransportInterface>>
      RtpTransportPair;
  typedef std::pair<std::unique_ptr<SrtpTransportInterface>,
                    std::unique_ptr<SrtpTransportInterface>>
      SrtpTransportPair;
  typedef std::pair<std::unique_ptr<RtpTransportControllerInterface>,
                    std::unique_ptr<RtpTransportControllerInterface>>
      RtpTransportControllerPair;

  // Helper function that creates one UDP transport each for |ortc_factory1_|
  // and |ortc_factory2_|, and connects them.
  UdpTransportPair CreateAndConnectUdpTransportPair() {
    auto transport1 = ortc_factory1_->CreateUdpTransport(AF_INET).MoveValue();
    auto transport2 = ortc_factory2_->CreateUdpTransport(AF_INET).MoveValue();
    transport1->SetRemoteAddress(
        rtc::SocketAddress(virtual_socket_server_.GetDefaultRoute(AF_INET),
                           transport2->GetLocalAddress().port()));
    transport2->SetRemoteAddress(
        rtc::SocketAddress(virtual_socket_server_.GetDefaultRoute(AF_INET),
                           transport1->GetLocalAddress().port()));
    return {std::move(transport1), std::move(transport2)};
  }

  // Creates one transport controller each for |ortc_factory1_| and
  // |ortc_factory2_|.
  RtpTransportControllerPair CreateRtpTransportControllerPair() {
    return {ortc_factory1_->CreateRtpTransportController().MoveValue(),
            ortc_factory2_->CreateRtpTransportController().MoveValue()};
  }

  // Helper function that creates a pair of RtpTransports between
  // |ortc_factory1_| and |ortc_factory2_|. Expected to be called with the
  // result of CreateAndConnectUdpTransportPair. |rtcp_udp_transports| can be
  // empty if RTCP muxing is used. |transport_controllers| can be empty if
  // these transports are being created using a default transport controller.
  RtpTransportPair CreateRtpTransportPair(
      const RtpTransportParameters& parameters,
      const UdpTransportPair& rtp_udp_transports,
      const UdpTransportPair& rtcp_udp_transports,
      const RtpTransportControllerPair& transport_controllers) {
    auto transport_result1 = ortc_factory1_->CreateRtpTransport(
        parameters, rtp_udp_transports.first.get(),
        rtcp_udp_transports.first.get(), transport_controllers.first.get());
    auto transport_result2 = ortc_factory2_->CreateRtpTransport(
        parameters, rtp_udp_transports.second.get(),
        rtcp_udp_transports.second.get(), transport_controllers.second.get());
    return {transport_result1.MoveValue(), transport_result2.MoveValue()};
  }

  SrtpTransportPair CreateSrtpTransportPair(
      const RtpTransportParameters& parameters,
      const UdpTransportPair& rtp_udp_transports,
      const UdpTransportPair& rtcp_udp_transports,
      const RtpTransportControllerPair& transport_controllers) {
    auto transport_result1 = ortc_factory1_->CreateSrtpTransport(
        parameters, rtp_udp_transports.first.get(),
        rtcp_udp_transports.first.get(), transport_controllers.first.get());
    auto transport_result2 = ortc_factory2_->CreateSrtpTransport(
        parameters, rtp_udp_transports.second.get(),
        rtcp_udp_transports.second.get(), transport_controllers.second.get());
    return {transport_result1.MoveValue(), transport_result2.MoveValue()};
  }

  // For convenience when |rtcp_udp_transports| and |transport_controllers|
  // aren't needed.
  RtpTransportPair CreateRtpTransportPair(
      const RtpTransportParameters& parameters,
      const UdpTransportPair& rtp_udp_transports) {
    return CreateRtpTransportPair(parameters, rtp_udp_transports,
                                  UdpTransportPair(),
                                  RtpTransportControllerPair());
  }

  SrtpTransportPair CreateSrtpTransportPairAndSetKeys(
      const RtpTransportParameters& parameters,
      const UdpTransportPair& rtp_udp_transports) {
    SrtpTransportPair srtp_transports = CreateSrtpTransportPair(
        parameters, rtp_udp_transports, UdpTransportPair(),
        RtpTransportControllerPair());
    EXPECT_TRUE(srtp_transports.first->SetSrtpSendKey(kTestCryptoParams1).ok());
    EXPECT_TRUE(
        srtp_transports.first->SetSrtpReceiveKey(kTestCryptoParams2).ok());
    EXPECT_TRUE(
        srtp_transports.second->SetSrtpSendKey(kTestCryptoParams2).ok());
    EXPECT_TRUE(
        srtp_transports.second->SetSrtpReceiveKey(kTestCryptoParams1).ok());
    return srtp_transports;
  }

  SrtpTransportPair CreateSrtpTransportPairAndSetMismatchingKeys(
      const RtpTransportParameters& parameters,
      const UdpTransportPair& rtp_udp_transports) {
    SrtpTransportPair srtp_transports = CreateSrtpTransportPair(
        parameters, rtp_udp_transports, UdpTransportPair(),
        RtpTransportControllerPair());
    EXPECT_TRUE(srtp_transports.first->SetSrtpSendKey(kTestCryptoParams1).ok());
    EXPECT_TRUE(
        srtp_transports.first->SetSrtpReceiveKey(kTestCryptoParams2).ok());
    EXPECT_TRUE(
        srtp_transports.second->SetSrtpSendKey(kTestCryptoParams1).ok());
    EXPECT_TRUE(
        srtp_transports.second->SetSrtpReceiveKey(kTestCryptoParams2).ok());
    return srtp_transports;
  }

  // Ends up using fake audio capture module, which was passed into OrtcFactory
  // on creation.
  rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateLocalAudioTrack(
      const std::string& id,
      OrtcFactoryInterface* ortc_factory) {
    // Disable echo cancellation to make test more efficient.
    cricket::AudioOptions options;
    options.echo_cancellation.emplace(true);
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        ortc_factory->CreateAudioSource(options);
    return ortc_factory->CreateAudioTrack(id, source);
  }

  // Stores created video source in |fake_video_track_sources_|.
  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackAndFakeSource(const std::string& id,
                                     OrtcFactoryInterface* ortc_factory) {
    FakePeriodicVideoSource::Config config;
    config.timestamp_offset_ms = rtc::TimeMillis();
    fake_video_track_sources_.emplace_back(
        new rtc::RefCountedObject<FakePeriodicVideoTrackSource>(
            config, false /* remote */));
    return rtc::scoped_refptr<VideoTrackInterface>(
        ortc_factory->CreateVideoTrack(id, fake_video_track_sources_.back()));
  }

  // Helper function used to test two way RTP senders and receivers with basic
  // configurations.
  // If |expect_success| is true, waits for kDefaultTimeout for
  // kDefaultNumFrames frames to be received by all RtpReceivers.
  // If |expect_success| is false, simply waits for |kReceivingDuration|, and
  // stores the number of received frames in |received_audio_frame1_| etc.
  void BasicTwoWayRtpSendersAndReceiversTest(RtpTransportPair srtp_transports,
                                             bool expect_success) {
    received_audio_frames1_ = 0;
    received_audio_frames2_ = 0;
    rendered_video_frames1_ = 0;
    rendered_video_frames2_ = 0;
    // Create all the senders and receivers (four per endpoint).
    auto audio_sender_result1 = ortc_factory1_->CreateRtpSender(
        cricket::MEDIA_TYPE_AUDIO, srtp_transports.first.get());
    auto video_sender_result1 = ortc_factory1_->CreateRtpSender(
        cricket::MEDIA_TYPE_VIDEO, srtp_transports.first.get());
    auto audio_receiver_result1 = ortc_factory1_->CreateRtpReceiver(
        cricket::MEDIA_TYPE_AUDIO, srtp_transports.first.get());
    auto video_receiver_result1 = ortc_factory1_->CreateRtpReceiver(
        cricket::MEDIA_TYPE_VIDEO, srtp_transports.first.get());
    ASSERT_TRUE(audio_sender_result1.ok());
    ASSERT_TRUE(video_sender_result1.ok());
    ASSERT_TRUE(audio_receiver_result1.ok());
    ASSERT_TRUE(video_receiver_result1.ok());
    auto audio_sender1 = audio_sender_result1.MoveValue();
    auto video_sender1 = video_sender_result1.MoveValue();
    auto audio_receiver1 = audio_receiver_result1.MoveValue();
    auto video_receiver1 = video_receiver_result1.MoveValue();

    auto audio_sender_result2 = ortc_factory2_->CreateRtpSender(
        cricket::MEDIA_TYPE_AUDIO, srtp_transports.second.get());
    auto video_sender_result2 = ortc_factory2_->CreateRtpSender(
        cricket::MEDIA_TYPE_VIDEO, srtp_transports.second.get());
    auto audio_receiver_result2 = ortc_factory2_->CreateRtpReceiver(
        cricket::MEDIA_TYPE_AUDIO, srtp_transports.second.get());
    auto video_receiver_result2 = ortc_factory2_->CreateRtpReceiver(
        cricket::MEDIA_TYPE_VIDEO, srtp_transports.second.get());
    ASSERT_TRUE(audio_sender_result2.ok());
    ASSERT_TRUE(video_sender_result2.ok());
    ASSERT_TRUE(audio_receiver_result2.ok());
    ASSERT_TRUE(video_receiver_result2.ok());
    auto audio_sender2 = audio_sender_result2.MoveValue();
    auto video_sender2 = video_sender_result2.MoveValue();
    auto audio_receiver2 = audio_receiver_result2.MoveValue();
    auto video_receiver2 = video_receiver_result2.MoveValue();

    // Add fake tracks.
    RTCError error = audio_sender1->SetTrack(
        CreateLocalAudioTrack("audio", ortc_factory1_.get()));
    EXPECT_TRUE(error.ok());
    error = video_sender1->SetTrack(
        CreateLocalVideoTrackAndFakeSource("video", ortc_factory1_.get()));
    EXPECT_TRUE(error.ok());
    error = audio_sender2->SetTrack(
        CreateLocalAudioTrack("audio", ortc_factory2_.get()));
    EXPECT_TRUE(error.ok());
    error = video_sender2->SetTrack(
        CreateLocalVideoTrackAndFakeSource("video", ortc_factory2_.get()));
    EXPECT_TRUE(error.ok());

    // "sent_X_parameters1" are the parameters that endpoint 1 sends with and
    // endpoint 2 receives with.
    RtpParameters sent_opus_parameters1 =
        MakeMinimalOpusParametersWithSsrc(0xdeadbeef);
    RtpParameters sent_vp8_parameters1 =
        MakeMinimalVp8ParametersWithSsrc(0xbaadfeed);
    RtpParameters sent_opus_parameters2 =
        MakeMinimalOpusParametersWithSsrc(0x13333337);
    RtpParameters sent_vp8_parameters2 =
        MakeMinimalVp8ParametersWithSsrc(0x12345678);

    // Configure the senders' and receivers' parameters.
    EXPECT_TRUE(audio_receiver1->Receive(sent_opus_parameters2).ok());
    EXPECT_TRUE(video_receiver1->Receive(sent_vp8_parameters2).ok());
    EXPECT_TRUE(audio_receiver2->Receive(sent_opus_parameters1).ok());
    EXPECT_TRUE(video_receiver2->Receive(sent_vp8_parameters1).ok());
    EXPECT_TRUE(audio_sender1->Send(sent_opus_parameters1).ok());
    EXPECT_TRUE(video_sender1->Send(sent_vp8_parameters1).ok());
    EXPECT_TRUE(audio_sender2->Send(sent_opus_parameters2).ok());
    EXPECT_TRUE(video_sender2->Send(sent_vp8_parameters2).ok());

    FakeVideoTrackRenderer fake_video_renderer1(
        static_cast<VideoTrackInterface*>(video_receiver1->GetTrack().get()));
    FakeVideoTrackRenderer fake_video_renderer2(
        static_cast<VideoTrackInterface*>(video_receiver2->GetTrack().get()));

    if (expect_success) {
      EXPECT_TRUE_WAIT(
          fake_audio_capture_module1_->frames_received() > kDefaultNumFrames &&
              fake_video_renderer1.num_rendered_frames() > kDefaultNumFrames &&
              fake_audio_capture_module2_->frames_received() >
                  kDefaultNumFrames &&
              fake_video_renderer2.num_rendered_frames() > kDefaultNumFrames,
          kDefaultTimeout)
          << "Audio capture module 1 received "
          << fake_audio_capture_module1_->frames_received()
          << " frames, Video renderer 1 rendered "
          << fake_video_renderer1.num_rendered_frames()
          << " frames, Audio capture module 2 received "
          << fake_audio_capture_module2_->frames_received()
          << " frames, Video renderer 2 rendered "
          << fake_video_renderer2.num_rendered_frames() << " frames.";
    } else {
      WAIT(false, kReceivingDuration);
      rendered_video_frames1_ = fake_video_renderer1.num_rendered_frames();
      rendered_video_frames2_ = fake_video_renderer2.num_rendered_frames();
      received_audio_frames1_ = fake_audio_capture_module1_->frames_received();
      received_audio_frames2_ = fake_audio_capture_module2_->frames_received();
    }
  }

  rtc::VirtualSocketServer virtual_socket_server_;
  rtc::Thread network_thread_;
  rtc::FakeNetworkManager fake_network_manager_;
  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module1_;
  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module2_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory1_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory2_;
  std::vector<rtc::scoped_refptr<VideoTrackSource>> fake_video_track_sources_;
  int received_audio_frames1_ = 0;
  int received_audio_frames2_ = 0;
  int rendered_video_frames1_ = 0;
  int rendered_video_frames2_ = 0;
};

// Disable for TSan v2, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=7366 for details.
#if !defined(THREAD_SANITIZER)

// Very basic end-to-end test with a single pair of audio RTP sender and
// receiver.
//
// Uses muxed RTCP, and minimal parameters with a hard-coded config that's
// known to work.
TEST_F(OrtcFactoryIntegrationTest, BasicOneWayAudioRtpSenderAndReceiver) {
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto rtp_transports =
      CreateRtpTransportPair(MakeRtcpMuxParameters(), udp_transports);

  auto sender_result = ortc_factory1_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transports.first.get());
  auto receiver_result = ortc_factory2_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, rtp_transports.second.get());
  ASSERT_TRUE(sender_result.ok());
  ASSERT_TRUE(receiver_result.ok());
  auto sender = sender_result.MoveValue();
  auto receiver = receiver_result.MoveValue();

  RTCError error =
      sender->SetTrack(CreateLocalAudioTrack("audio", ortc_factory1_.get()));
  EXPECT_TRUE(error.ok());

  RtpParameters opus_parameters = MakeMinimalOpusParameters();
  EXPECT_TRUE(receiver->Receive(opus_parameters).ok());
  EXPECT_TRUE(sender->Send(opus_parameters).ok());
  // Sender and receiver are connected and configured; audio frames should be
  // able to flow at this point.
  EXPECT_TRUE_WAIT(
      fake_audio_capture_module2_->frames_received() > kDefaultNumFrames,
      kDefaultTimeout);
}

// Very basic end-to-end test with a single pair of video RTP sender and
// receiver.
//
// Uses muxed RTCP, and minimal parameters with a hard-coded config that's
// known to work.
TEST_F(OrtcFactoryIntegrationTest, BasicOneWayVideoRtpSenderAndReceiver) {
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto rtp_transports =
      CreateRtpTransportPair(MakeRtcpMuxParameters(), udp_transports);

  auto sender_result = ortc_factory1_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transports.first.get());
  auto receiver_result = ortc_factory2_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, rtp_transports.second.get());
  ASSERT_TRUE(sender_result.ok());
  ASSERT_TRUE(receiver_result.ok());
  auto sender = sender_result.MoveValue();
  auto receiver = receiver_result.MoveValue();

  RTCError error = sender->SetTrack(
      CreateLocalVideoTrackAndFakeSource("video", ortc_factory1_.get()));
  EXPECT_TRUE(error.ok());

  RtpParameters vp8_parameters = MakeMinimalVp8Parameters();
  EXPECT_TRUE(receiver->Receive(vp8_parameters).ok());
  EXPECT_TRUE(sender->Send(vp8_parameters).ok());
  FakeVideoTrackRenderer fake_renderer(
      static_cast<VideoTrackInterface*>(receiver->GetTrack().get()));
  // Sender and receiver are connected and configured; video frames should be
  // able to flow at this point.
  EXPECT_TRUE_WAIT(fake_renderer.num_rendered_frames() > kDefaultNumFrames,
                   kDefaultTimeout);
}

// Test that if the track is changed while sending, the sender seamlessly
// transitions to sending it and frames are received end-to-end.
//
// Only doing this for video, since given that audio is sourced from a single
// fake audio capture module, the audio track is just a dummy object.
// TODO(deadbeef): Change this when possible.
TEST_F(OrtcFactoryIntegrationTest, SetTrackWhileSending) {
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto rtp_transports =
      CreateRtpTransportPair(MakeRtcpMuxParameters(), udp_transports);

  auto sender_result = ortc_factory1_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transports.first.get());
  auto receiver_result = ortc_factory2_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, rtp_transports.second.get());
  ASSERT_TRUE(sender_result.ok());
  ASSERT_TRUE(receiver_result.ok());
  auto sender = sender_result.MoveValue();
  auto receiver = receiver_result.MoveValue();

  RTCError error = sender->SetTrack(
      CreateLocalVideoTrackAndFakeSource("video_1", ortc_factory1_.get()));
  EXPECT_TRUE(error.ok());
  RtpParameters vp8_parameters = MakeMinimalVp8Parameters();
  EXPECT_TRUE(receiver->Receive(vp8_parameters).ok());
  EXPECT_TRUE(sender->Send(vp8_parameters).ok());
  FakeVideoTrackRenderer fake_renderer(
      static_cast<VideoTrackInterface*>(receiver->GetTrack().get()));
  // Expect for some initial number of frames to be received.
  EXPECT_TRUE_WAIT(fake_renderer.num_rendered_frames() > kDefaultNumFrames,
                   kDefaultTimeout);
  // Destroy old source, set a new track, and verify new frames are received
  // from the new track. The VideoTrackSource is reference counted and may live
  // a little longer, so tell it that its source is going away now.
  fake_video_track_sources_[0] = nullptr;
  int prev_num_frames = fake_renderer.num_rendered_frames();
  error = sender->SetTrack(
      CreateLocalVideoTrackAndFakeSource("video_2", ortc_factory1_.get()));
  EXPECT_TRUE(error.ok());
  EXPECT_TRUE_WAIT(
      fake_renderer.num_rendered_frames() > kDefaultNumFrames + prev_num_frames,
      kDefaultTimeout);
}

// TODO(webrtc:7915, webrtc:9184): Tests below are disabled for iOS 64 on debug
// builds because of flakiness.
#if !(defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_64_BITS) && !defined(NDEBUG))
#define MAYBE_BasicTwoWayAudioVideoRtpSendersAndReceivers \
  BasicTwoWayAudioVideoRtpSendersAndReceivers
#define MAYBE_BasicTwoWayAudioVideoSrtpSendersAndReceivers \
  BasicTwoWayAudioVideoSrtpSendersAndReceivers
#define MAYBE_SrtpSendersAndReceiversWithMismatchingKeys \
  SrtpSendersAndReceiversWithMismatchingKeys
#define MAYBE_OneSideSrtpSenderAndReceiver OneSideSrtpSenderAndReceiver
#define MAYBE_FullTwoWayAudioVideoSrtpSendersAndReceivers \
  FullTwoWayAudioVideoSrtpSendersAndReceivers
#else
#define MAYBE_BasicTwoWayAudioVideoRtpSendersAndReceivers \
  DISABLED_BasicTwoWayAudioVideoRtpSendersAndReceivers
#define MAYBE_BasicTwoWayAudioVideoSrtpSendersAndReceivers \
  DISABLED_BasicTwoWayAudioVideoSrtpSendersAndReceivers
#define MAYBE_SrtpSendersAndReceiversWithMismatchingKeys \
  DISABLED_SrtpSendersAndReceiversWithMismatchingKeys
#define MAYBE_OneSideSrtpSenderAndReceiver DISABLED_OneSideSrtpSenderAndReceiver
#define MAYBE_FullTwoWayAudioVideoSrtpSendersAndReceivers \
  DISABLED_FullTwoWayAudioVideoSrtpSendersAndReceivers
#endif

// End-to-end test with two pairs of RTP senders and receivers, for audio and
// video.
//
// Uses muxed RTCP, and minimal parameters with hard-coded configs that are
// known to work.
TEST_F(OrtcFactoryIntegrationTest,
       MAYBE_BasicTwoWayAudioVideoRtpSendersAndReceivers) {
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto rtp_transports =
      CreateRtpTransportPair(MakeRtcpMuxParameters(), udp_transports);
  bool expect_success = true;
  BasicTwoWayRtpSendersAndReceiversTest(std::move(rtp_transports),
                                        expect_success);
}

TEST_F(OrtcFactoryIntegrationTest,
       MAYBE_BasicTwoWayAudioVideoSrtpSendersAndReceivers) {
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto srtp_transports = CreateSrtpTransportPairAndSetKeys(
      MakeRtcpMuxParameters(), udp_transports);
  bool expect_success = true;
  BasicTwoWayRtpSendersAndReceiversTest(std::move(srtp_transports),
                                        expect_success);
}

// Tests that the packets cannot be decoded if the keys are mismatched.
// TODO(webrtc:9184): Disabled because this test is flaky.
TEST_F(OrtcFactoryIntegrationTest,
       MAYBE_SrtpSendersAndReceiversWithMismatchingKeys) {
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto srtp_transports = CreateSrtpTransportPairAndSetMismatchingKeys(
      MakeRtcpMuxParameters(), udp_transports);
  bool expect_success = false;
  BasicTwoWayRtpSendersAndReceiversTest(std::move(srtp_transports),
                                        expect_success);
  // No frames are expected to be decoded.
  EXPECT_TRUE(received_audio_frames1_ == 0 && received_audio_frames2_ == 0 &&
              rendered_video_frames1_ == 0 && rendered_video_frames2_ == 0);
}

// Tests that the frames cannot be decoded if only one side uses SRTP.
TEST_F(OrtcFactoryIntegrationTest, MAYBE_OneSideSrtpSenderAndReceiver) {
  auto rtcp_parameters = MakeRtcpMuxParameters();
  auto udp_transports = CreateAndConnectUdpTransportPair();
  auto rtcp_udp_transports = UdpTransportPair();
  auto transport_controllers = RtpTransportControllerPair();
  auto transport_result1 = ortc_factory1_->CreateRtpTransport(
      rtcp_parameters, udp_transports.first.get(),
      rtcp_udp_transports.first.get(), transport_controllers.first.get());
  auto transport_result2 = ortc_factory2_->CreateSrtpTransport(
      rtcp_parameters, udp_transports.second.get(),
      rtcp_udp_transports.second.get(), transport_controllers.second.get());

  auto rtp_transport = transport_result1.MoveValue();
  auto srtp_transport = transport_result2.MoveValue();
  EXPECT_TRUE(srtp_transport->SetSrtpSendKey(kTestCryptoParams1).ok());
  EXPECT_TRUE(srtp_transport->SetSrtpReceiveKey(kTestCryptoParams2).ok());
  bool expect_success = false;
  BasicTwoWayRtpSendersAndReceiversTest(
      {std::move(rtp_transport), std::move(srtp_transport)}, expect_success);

  // The SRTP side is not expected to decode any audio or video frames.
  // The RTP side is not expected to decode any video frames while it is
  // possible that the encrypted audio frames can be accidentally decoded which
  // is why received_audio_frames1_ is not validated.
  EXPECT_TRUE(received_audio_frames2_ == 0 && rendered_video_frames1_ == 0 &&
              rendered_video_frames2_ == 0);
}

// End-to-end test with two pairs of RTP senders and receivers, for audio and
// video. Unlike the test above, this attempts to make the parameters as
// complex as possible. The senders and receivers use the SRTP transport with
// different keys.
//
// Uses non-muxed RTCP, with separate audio/video transports, and a full set of
// parameters, as would normally be used in a PeerConnection.
//
// TODO(deadbeef): Update this test as more audio/video features become
// supported.
TEST_F(OrtcFactoryIntegrationTest,
       MAYBE_FullTwoWayAudioVideoSrtpSendersAndReceivers) {
  // We want four pairs of UDP transports for this test, for audio/video and
  // RTP/RTCP.
  auto audio_rtp_udp_transports = CreateAndConnectUdpTransportPair();
  auto audio_rtcp_udp_transports = CreateAndConnectUdpTransportPair();
  auto video_rtp_udp_transports = CreateAndConnectUdpTransportPair();
  auto video_rtcp_udp_transports = CreateAndConnectUdpTransportPair();

  // Since we have multiple RTP transports on each side, we need an RTP
  // transport controller.
  auto transport_controllers = CreateRtpTransportControllerPair();

  RtpTransportParameters audio_rtp_transport_parameters;
  audio_rtp_transport_parameters.rtcp.mux = false;
  auto audio_srtp_transports = CreateSrtpTransportPair(
      audio_rtp_transport_parameters, audio_rtp_udp_transports,
      audio_rtcp_udp_transports, transport_controllers);

  RtpTransportParameters video_rtp_transport_parameters;
  video_rtp_transport_parameters.rtcp.mux = false;
  video_rtp_transport_parameters.rtcp.reduced_size = true;
  auto video_srtp_transports = CreateSrtpTransportPair(
      video_rtp_transport_parameters, video_rtp_udp_transports,
      video_rtcp_udp_transports, transport_controllers);

  // Set keys for SRTP transports.
  audio_srtp_transports.first->SetSrtpSendKey(kTestCryptoParams1);
  audio_srtp_transports.first->SetSrtpReceiveKey(kTestCryptoParams2);
  video_srtp_transports.first->SetSrtpSendKey(kTestCryptoParams3);
  video_srtp_transports.first->SetSrtpReceiveKey(kTestCryptoParams4);

  audio_srtp_transports.second->SetSrtpSendKey(kTestCryptoParams2);
  audio_srtp_transports.second->SetSrtpReceiveKey(kTestCryptoParams1);
  video_srtp_transports.second->SetSrtpSendKey(kTestCryptoParams4);
  video_srtp_transports.second->SetSrtpReceiveKey(kTestCryptoParams3);

  // Create all the senders and receivers (four per endpoint).
  auto audio_sender_result1 = ortc_factory1_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, audio_srtp_transports.first.get());
  auto video_sender_result1 = ortc_factory1_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, video_srtp_transports.first.get());
  auto audio_receiver_result1 = ortc_factory1_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, audio_srtp_transports.first.get());
  auto video_receiver_result1 = ortc_factory1_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, video_srtp_transports.first.get());
  ASSERT_TRUE(audio_sender_result1.ok());
  ASSERT_TRUE(video_sender_result1.ok());
  ASSERT_TRUE(audio_receiver_result1.ok());
  ASSERT_TRUE(video_receiver_result1.ok());
  auto audio_sender1 = audio_sender_result1.MoveValue();
  auto video_sender1 = video_sender_result1.MoveValue();
  auto audio_receiver1 = audio_receiver_result1.MoveValue();
  auto video_receiver1 = video_receiver_result1.MoveValue();

  auto audio_sender_result2 = ortc_factory2_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, audio_srtp_transports.second.get());
  auto video_sender_result2 = ortc_factory2_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, video_srtp_transports.second.get());
  auto audio_receiver_result2 = ortc_factory2_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, audio_srtp_transports.second.get());
  auto video_receiver_result2 = ortc_factory2_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, video_srtp_transports.second.get());
  ASSERT_TRUE(audio_sender_result2.ok());
  ASSERT_TRUE(video_sender_result2.ok());
  ASSERT_TRUE(audio_receiver_result2.ok());
  ASSERT_TRUE(video_receiver_result2.ok());
  auto audio_sender2 = audio_sender_result2.MoveValue();
  auto video_sender2 = video_sender_result2.MoveValue();
  auto audio_receiver2 = audio_receiver_result2.MoveValue();
  auto video_receiver2 = video_receiver_result2.MoveValue();

  RTCError error = audio_sender1->SetTrack(
      CreateLocalAudioTrack("audio", ortc_factory1_.get()));
  EXPECT_TRUE(error.ok());
  error = video_sender1->SetTrack(
      CreateLocalVideoTrackAndFakeSource("video", ortc_factory1_.get()));
  EXPECT_TRUE(error.ok());
  error = audio_sender2->SetTrack(
      CreateLocalAudioTrack("audio", ortc_factory2_.get()));
  EXPECT_TRUE(error.ok());
  error = video_sender2->SetTrack(
      CreateLocalVideoTrackAndFakeSource("video", ortc_factory2_.get()));
  EXPECT_TRUE(error.ok());

  // Use different codecs in different directions for extra challenge.
  RtpParameters opus_send_parameters = MakeFullOpusParameters();
  RtpParameters isac_send_parameters = MakeFullIsacParameters();
  RtpParameters vp8_send_parameters = MakeFullVp8Parameters();
  RtpParameters vp9_send_parameters = MakeFullVp9Parameters();

  // Remove "payload_type" from receive parameters. Receiver will need to
  // discern the payload type from packets received.
  RtpParameters opus_receive_parameters = opus_send_parameters;
  RtpParameters isac_receive_parameters = isac_send_parameters;
  RtpParameters vp8_receive_parameters = vp8_send_parameters;
  RtpParameters vp9_receive_parameters = vp9_send_parameters;
  opus_receive_parameters.encodings[0].codec_payload_type.reset();
  isac_receive_parameters.encodings[0].codec_payload_type.reset();
  vp8_receive_parameters.encodings[0].codec_payload_type.reset();
  vp9_receive_parameters.encodings[0].codec_payload_type.reset();

  // Configure the senders' and receivers' parameters.
  //
  // Note: Intentionally, the top codec in the receive parameters does not
  // match the codec sent by the other side. If "Receive" is called with a list
  // of codecs, the receiver should be prepared to receive any of them, not
  // just the one on top.
  EXPECT_TRUE(audio_receiver1->Receive(opus_receive_parameters).ok());
  EXPECT_TRUE(video_receiver1->Receive(vp8_receive_parameters).ok());
  EXPECT_TRUE(audio_receiver2->Receive(isac_receive_parameters).ok());
  EXPECT_TRUE(video_receiver2->Receive(vp9_receive_parameters).ok());
  EXPECT_TRUE(audio_sender1->Send(opus_send_parameters).ok());
  EXPECT_TRUE(video_sender1->Send(vp8_send_parameters).ok());
  EXPECT_TRUE(audio_sender2->Send(isac_send_parameters).ok());
  EXPECT_TRUE(video_sender2->Send(vp9_send_parameters).ok());

  FakeVideoTrackRenderer fake_video_renderer1(
      static_cast<VideoTrackInterface*>(video_receiver1->GetTrack().get()));
  FakeVideoTrackRenderer fake_video_renderer2(
      static_cast<VideoTrackInterface*>(video_receiver2->GetTrack().get()));

  // Senders and receivers are connected and configured; audio and video frames
  // should be able to flow at this point.
  EXPECT_TRUE_WAIT(
      fake_audio_capture_module1_->frames_received() > kDefaultNumFrames &&
          fake_video_renderer1.num_rendered_frames() > kDefaultNumFrames &&
          fake_audio_capture_module2_->frames_received() > kDefaultNumFrames &&
          fake_video_renderer2.num_rendered_frames() > kDefaultNumFrames,
      kDefaultTimeout);
}

// TODO(deadbeef): End-to-end test for multiple senders/receivers of the same
// media type, once that's supported. Currently, it is not because the
// BaseChannel model relies on there being a single VoiceChannel and
// VideoChannel, and these only support a single set of codecs/etc. per
// send/receive direction.

// TODO(deadbeef): End-to-end test for simulcast, once that's supported by this
// API.

#endif  // if !defined(THREAD_SANITIZER)

}  // namespace webrtc
