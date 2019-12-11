/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/mediastreaminterface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/base/fakevideocapturer.h"
#include "p2p/base/fakeportallocator.h"
#include "pc/peerconnectionfactory.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/gunit.h"

#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakertccertificategenerator.h"
#include "pc/test/fakevideotrackrenderer.h"

using webrtc::DataChannelInterface;
using webrtc::FakeVideoTrackRenderer;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::VideoTrackSourceInterface;
using webrtc::VideoTrackInterface;

namespace {

static const char kStunIceServer[] = "stun:stun.l.google.com:19302";
static const char kTurnIceServer[] = "turn:test%40hello.com@test.com:1234";
static const char kTurnIceServerWithTransport[] =
    "turn:test@hello.com?transport=tcp";
static const char kSecureTurnIceServer[] = "turns:test@hello.com?transport=tcp";
static const char kSecureTurnIceServerWithoutTransportParam[] =
    "turns:test_no_transport@hello.com:443";
static const char kSecureTurnIceServerWithoutTransportAndPortParam[] =
    "turns:test_no_transport@hello.com";
static const char kTurnIceServerWithNoUsernameInUri[] = "turn:test.com:1234";
static const char kTurnPassword[] = "turnpassword";
static const int kDefaultStunPort = 3478;
static const int kDefaultStunTlsPort = 5349;
static const char kTurnUsername[] = "test";
static const char kStunIceServerWithIPv4Address[] = "stun:1.2.3.4:1234";
static const char kStunIceServerWithIPv4AddressWithoutPort[] = "stun:1.2.3.4";
static const char kStunIceServerWithIPv6Address[] = "stun:[2401:fa00:4::]:1234";
static const char kStunIceServerWithIPv6AddressWithoutPort[] =
    "stun:[2401:fa00:4::]";
static const char kTurnIceServerWithIPv6Address[] =
    "turn:test@[2401:fa00:4::]:1234";

class NullPeerConnectionObserver : public PeerConnectionObserver {
 public:
  virtual ~NullPeerConnectionObserver() = default;
  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override {}
  void OnRemoveStream(
      rtc::scoped_refptr<MediaStreamInterface> stream) override {}
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
  }
};

}  // namespace

class PeerConnectionFactoryTest : public testing::Test {
  void SetUp() {
#ifdef WEBRTC_ANDROID
    webrtc::InitializeAndroidObjects();
#endif
    // Use fake audio device module since we're only testing the interface
    // level, and using a real one could make tests flaky e.g. when run in
    // parallel.
    factory_ = webrtc::CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        rtc::scoped_refptr<webrtc::AudioDeviceModule>(
            FakeAudioCaptureModule::Create()),
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
        nullptr /* audio_processing */);

    ASSERT_TRUE(factory_.get() != NULL);
    port_allocator_.reset(
        new cricket::FakePortAllocator(rtc::Thread::Current(), nullptr));
    raw_port_allocator_ = port_allocator_.get();
  }

 protected:
  void VerifyStunServers(cricket::ServerAddresses stun_servers) {
    EXPECT_EQ(stun_servers, raw_port_allocator_->stun_servers());
  }

  void VerifyTurnServers(std::vector<cricket::RelayServerConfig> turn_servers) {
    EXPECT_EQ(turn_servers.size(), raw_port_allocator_->turn_servers().size());
    for (size_t i = 0; i < turn_servers.size(); ++i) {
      ASSERT_EQ(1u, turn_servers[i].ports.size());
      EXPECT_EQ(1u, raw_port_allocator_->turn_servers()[i].ports.size());
      EXPECT_EQ(
          turn_servers[i].ports[0].address.ToString(),
          raw_port_allocator_->turn_servers()[i].ports[0].address.ToString());
      EXPECT_EQ(turn_servers[i].ports[0].proto,
                raw_port_allocator_->turn_servers()[i].ports[0].proto);
      EXPECT_EQ(turn_servers[i].credentials.username,
                raw_port_allocator_->turn_servers()[i].credentials.username);
      EXPECT_EQ(turn_servers[i].credentials.password,
                raw_port_allocator_->turn_servers()[i].credentials.password);
    }
  }

  void VerifyAudioCodecCapability(const webrtc::RtpCodecCapability& codec) {
    EXPECT_EQ(codec.kind, cricket::MEDIA_TYPE_AUDIO);
    EXPECT_FALSE(codec.name.empty());
    EXPECT_GT(codec.clock_rate, 0);
    EXPECT_GT(codec.num_channels, 0);
  }

  void VerifyVideoCodecCapability(const webrtc::RtpCodecCapability& codec) {
    EXPECT_EQ(codec.kind, cricket::MEDIA_TYPE_VIDEO);
    EXPECT_FALSE(codec.name.empty());
    EXPECT_GT(codec.clock_rate, 0);
  }

  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory_;
  NullPeerConnectionObserver observer_;
  std::unique_ptr<cricket::FakePortAllocator> port_allocator_;
  // Since the PC owns the port allocator after it's been initialized,
  // this should only be used when known to be safe.
  cricket::FakePortAllocator* raw_port_allocator_;
};

// Verify creation of PeerConnection using internal ADM, video factory and
// internal libjingle threads.
// TODO(henrika): disabling this test since relying on real audio can result in
// flaky tests and focus on details that are out of scope for you might expect
// for a PeerConnectionFactory unit test.
// See https://bugs.chromium.org/p/webrtc/issues/detail?id=7806 for details.
TEST(PeerConnectionFactoryTestInternal, DISABLED_CreatePCUsingInternalModules) {
#ifdef WEBRTC_ANDROID
  webrtc::InitializeAndroidObjects();
#endif

  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory(
          nullptr /* network_thread */, nullptr /* worker_thread */,
          nullptr /* signaling_thread */, nullptr /* default_adm */,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateBuiltinAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
          nullptr /* audio_processing */));

  NullPeerConnectionObserver observer;
  webrtc::PeerConnectionInterface::RTCConfiguration config;

  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(factory->CreatePeerConnection(
      config, nullptr, std::move(cert_generator), &observer));

  EXPECT_TRUE(pc.get() != nullptr);
}

TEST_F(PeerConnectionFactoryTest, CheckRtpSenderAudioCapabilities) {
  webrtc::RtpCapabilities audio_capabilities =
      factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_FALSE(audio_capabilities.codecs.empty());
  for (const auto& codec : audio_capabilities.codecs) {
    VerifyAudioCodecCapability(codec);
  }
  EXPECT_FALSE(audio_capabilities.header_extensions.empty());
  for (const auto& header_extension : audio_capabilities.header_extensions) {
    EXPECT_FALSE(header_extension.uri.empty());
  }
}

TEST_F(PeerConnectionFactoryTest, CheckRtpSenderVideoCapabilities) {
  webrtc::RtpCapabilities video_capabilities =
      factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_FALSE(video_capabilities.codecs.empty());
  for (const auto& codec : video_capabilities.codecs) {
    VerifyVideoCodecCapability(codec);
  }
  EXPECT_FALSE(video_capabilities.header_extensions.empty());
  for (const auto& header_extension : video_capabilities.header_extensions) {
    EXPECT_FALSE(header_extension.uri.empty());
  }
}

TEST_F(PeerConnectionFactoryTest, CheckRtpSenderDataCapabilities) {
  webrtc::RtpCapabilities data_capabilities =
      factory_->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_DATA);
  EXPECT_TRUE(data_capabilities.codecs.empty());
  EXPECT_TRUE(data_capabilities.header_extensions.empty());
}

TEST_F(PeerConnectionFactoryTest, CheckRtpReceiverAudioCapabilities) {
  webrtc::RtpCapabilities audio_capabilities =
      factory_->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_FALSE(audio_capabilities.codecs.empty());
  for (const auto& codec : audio_capabilities.codecs) {
    VerifyAudioCodecCapability(codec);
  }
  EXPECT_FALSE(audio_capabilities.header_extensions.empty());
  for (const auto& header_extension : audio_capabilities.header_extensions) {
    EXPECT_FALSE(header_extension.uri.empty());
  }
}

TEST_F(PeerConnectionFactoryTest, CheckRtpReceiverVideoCapabilities) {
  webrtc::RtpCapabilities video_capabilities =
      factory_->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_FALSE(video_capabilities.codecs.empty());
  for (const auto& codec : video_capabilities.codecs) {
    VerifyVideoCodecCapability(codec);
  }
  EXPECT_FALSE(video_capabilities.header_extensions.empty());
  for (const auto& header_extension : video_capabilities.header_extensions) {
    EXPECT_FALSE(header_extension.uri.empty());
  }
}

TEST_F(PeerConnectionFactoryTest, CheckRtpReceiverDataCapabilities) {
  webrtc::RtpCapabilities data_capabilities =
      factory_->GetRtpReceiverCapabilities(cricket::MEDIA_TYPE_DATA);
  EXPECT_TRUE(data_capabilities.codecs.empty());
  EXPECT_TRUE(data_capabilities.header_extensions.empty());
}

// This test verifies creation of PeerConnection with valid STUN and TURN
// configuration. Also verifies the URL's parsed correctly as expected.
TEST_F(PeerConnectionFactoryTest, CreatePCUsingIceServers) {
  PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kStunIceServer;
  config.servers.push_back(ice_server);
  ice_server.uri = kTurnIceServer;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  ice_server.uri = kTurnIceServerWithTransport;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(config, std::move(port_allocator_),
                                     std::move(cert_generator), &observer_));
  ASSERT_TRUE(pc.get() != NULL);
  cricket::ServerAddresses stun_servers;
  rtc::SocketAddress stun1("stun.l.google.com", 19302);
  stun_servers.insert(stun1);
  VerifyStunServers(stun_servers);
  std::vector<cricket::RelayServerConfig> turn_servers;
  cricket::RelayServerConfig turn1("test.com", 1234, "test@hello.com",
                                   kTurnPassword, cricket::PROTO_UDP);
  turn_servers.push_back(turn1);
  cricket::RelayServerConfig turn2("hello.com", kDefaultStunPort, "test",
                                   kTurnPassword, cricket::PROTO_TCP);
  turn_servers.push_back(turn2);
  VerifyTurnServers(turn_servers);
}

// This test verifies creation of PeerConnection with valid STUN and TURN
// configuration. Also verifies the list of URL's parsed correctly as expected.
TEST_F(PeerConnectionFactoryTest, CreatePCUsingIceServersUrls) {
  PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back(kStunIceServer);
  ice_server.urls.push_back(kTurnIceServer);
  ice_server.urls.push_back(kTurnIceServerWithTransport);
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(config, std::move(port_allocator_),
                                     std::move(cert_generator), &observer_));
  ASSERT_TRUE(pc.get() != NULL);
  cricket::ServerAddresses stun_servers;
  rtc::SocketAddress stun1("stun.l.google.com", 19302);
  stun_servers.insert(stun1);
  VerifyStunServers(stun_servers);
  std::vector<cricket::RelayServerConfig> turn_servers;
  cricket::RelayServerConfig turn1("test.com", 1234, "test@hello.com",
                                   kTurnPassword, cricket::PROTO_UDP);
  turn_servers.push_back(turn1);
  cricket::RelayServerConfig turn2("hello.com", kDefaultStunPort, "test",
                                   kTurnPassword, cricket::PROTO_TCP);
  turn_servers.push_back(turn2);
  VerifyTurnServers(turn_servers);
}

TEST_F(PeerConnectionFactoryTest, CreatePCUsingNoUsernameInUri) {
  PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kStunIceServer;
  config.servers.push_back(ice_server);
  ice_server.uri = kTurnIceServerWithNoUsernameInUri;
  ice_server.username = kTurnUsername;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(config, std::move(port_allocator_),
                                     std::move(cert_generator), &observer_));
  ASSERT_TRUE(pc.get() != NULL);
  std::vector<cricket::RelayServerConfig> turn_servers;
  cricket::RelayServerConfig turn("test.com", 1234, kTurnUsername,
                                  kTurnPassword, cricket::PROTO_UDP);
  turn_servers.push_back(turn);
  VerifyTurnServers(turn_servers);
}

// This test verifies the PeerConnection created properly with TURN url which
// has transport parameter in it.
TEST_F(PeerConnectionFactoryTest, CreatePCUsingTurnUrlWithTransportParam) {
  PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kTurnIceServerWithTransport;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(config, std::move(port_allocator_),
                                     std::move(cert_generator), &observer_));
  ASSERT_TRUE(pc.get() != NULL);
  std::vector<cricket::RelayServerConfig> turn_servers;
  cricket::RelayServerConfig turn("hello.com", kDefaultStunPort, "test",
                                  kTurnPassword, cricket::PROTO_TCP);
  turn_servers.push_back(turn);
  VerifyTurnServers(turn_servers);
}

TEST_F(PeerConnectionFactoryTest, CreatePCUsingSecureTurnUrl) {
  PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kSecureTurnIceServer;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  ice_server.uri = kSecureTurnIceServerWithoutTransportParam;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  ice_server.uri = kSecureTurnIceServerWithoutTransportAndPortParam;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(config, std::move(port_allocator_),
                                     std::move(cert_generator), &observer_));
  ASSERT_TRUE(pc.get() != NULL);
  std::vector<cricket::RelayServerConfig> turn_servers;
  cricket::RelayServerConfig turn1("hello.com", kDefaultStunTlsPort, "test",
                                   kTurnPassword, cricket::PROTO_TLS);
  turn_servers.push_back(turn1);
  // TURNS with transport param should be default to tcp.
  cricket::RelayServerConfig turn2("hello.com", 443, "test_no_transport",
                                   kTurnPassword, cricket::PROTO_TLS);
  turn_servers.push_back(turn2);
  cricket::RelayServerConfig turn3("hello.com", kDefaultStunTlsPort,
                                   "test_no_transport", kTurnPassword,
                                   cricket::PROTO_TLS);
  turn_servers.push_back(turn3);
  VerifyTurnServers(turn_servers);
}

TEST_F(PeerConnectionFactoryTest, CreatePCUsingIPLiteralAddress) {
  PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.uri = kStunIceServerWithIPv4Address;
  config.servers.push_back(ice_server);
  ice_server.uri = kStunIceServerWithIPv4AddressWithoutPort;
  config.servers.push_back(ice_server);
  ice_server.uri = kStunIceServerWithIPv6Address;
  config.servers.push_back(ice_server);
  ice_server.uri = kStunIceServerWithIPv6AddressWithoutPort;
  config.servers.push_back(ice_server);
  ice_server.uri = kTurnIceServerWithIPv6Address;
  ice_server.password = kTurnPassword;
  config.servers.push_back(ice_server);
  std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
      new FakeRTCCertificateGenerator());
  rtc::scoped_refptr<PeerConnectionInterface> pc(
      factory_->CreatePeerConnection(config, std::move(port_allocator_),
                                     std::move(cert_generator), &observer_));
  ASSERT_TRUE(pc.get() != NULL);
  cricket::ServerAddresses stun_servers;
  rtc::SocketAddress stun1("1.2.3.4", 1234);
  stun_servers.insert(stun1);
  rtc::SocketAddress stun2("1.2.3.4", 3478);
  stun_servers.insert(stun2);  // Default port
  rtc::SocketAddress stun3("2401:fa00:4::", 1234);
  stun_servers.insert(stun3);
  rtc::SocketAddress stun4("2401:fa00:4::", 3478);
  stun_servers.insert(stun4);  // Default port
  VerifyStunServers(stun_servers);

  std::vector<cricket::RelayServerConfig> turn_servers;
  cricket::RelayServerConfig turn1("2401:fa00:4::", 1234, "test", kTurnPassword,
                                   cricket::PROTO_UDP);
  turn_servers.push_back(turn1);
  VerifyTurnServers(turn_servers);
}

// This test verifies the captured stream is rendered locally using a
// local video track.
TEST_F(PeerConnectionFactoryTest, LocalRendering) {
  cricket::FakeVideoCapturerWithTaskQueue* capturer =
      new cricket::FakeVideoCapturerWithTaskQueue();
  // The source takes ownership of |capturer|, but we keep a raw pointer to
  // inject fake frames.
  rtc::scoped_refptr<VideoTrackSourceInterface> source(
      factory_->CreateVideoSource(
          std::unique_ptr<cricket::VideoCapturer>(capturer), NULL));
  ASSERT_TRUE(source.get() != NULL);
  rtc::scoped_refptr<VideoTrackInterface> track(
      factory_->CreateVideoTrack("testlabel", source));
  ASSERT_TRUE(track.get() != NULL);
  FakeVideoTrackRenderer local_renderer(track);

  EXPECT_EQ(0, local_renderer.num_rendered_frames());
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_EQ(1, local_renderer.num_rendered_frames());
  EXPECT_FALSE(local_renderer.black_frame());

  track->set_enabled(false);
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_EQ(2, local_renderer.num_rendered_frames());
  EXPECT_TRUE(local_renderer.black_frame());

  track->set_enabled(true);
  EXPECT_TRUE(capturer->CaptureFrame());
  EXPECT_EQ(3, local_renderer.num_rendered_frames());
  EXPECT_FALSE(local_renderer.black_frame());
}
