/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>

#include "api/media_transport_interface.h"
#include "api/test/fake_media_transport.h"
#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakeicetransport.h"
#include "p2p/base/transportfactoryinterface.h"
#include "p2p/base/transportinfo.h"
#include "pc/jseptransportcontroller.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

using cricket::FakeDtlsTransport;
using cricket::Candidate;
using cricket::Candidates;
using webrtc::SdpType;

static const int kTimeout = 100;
static const char kIceUfrag1[] = "u0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIceUfrag2[] = "u0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";
static const char kIceUfrag3[] = "u0003";
static const char kIcePwd3[] = "TESTICEPWD00000000000003";
static const char kAudioMid1[] = "audio1";
static const char kAudioMid2[] = "audio2";
static const char kVideoMid1[] = "video1";
static const char kVideoMid2[] = "video2";
static const char kDataMid1[] = "data1";

namespace webrtc {

namespace {

// Media transport factory requires crypto settings to be present in order to
// create media transport.
void AddCryptoSettings(cricket::SessionDescription* description) {
  for (auto& content : description->contents()) {
    content.media_description()->AddCrypto(cricket::CryptoParams(
        /*t=*/0, std::string(rtc::CS_AES_CM_128_HMAC_SHA1_80),
        "inline:YUJDZGVmZ2hpSktMbW9QUXJzVHVWd3l6MTIzNDU2", ""));
  }
}

}  // namespace

class FakeTransportFactory : public cricket::TransportFactoryInterface {
 public:
  std::unique_ptr<cricket::IceTransportInternal> CreateIceTransport(
      const std::string& transport_name,
      int component) override {
    return absl::make_unique<cricket::FakeIceTransport>(transport_name,
                                                        component);
  }

  std::unique_ptr<cricket::DtlsTransportInternal> CreateDtlsTransport(
      std::unique_ptr<cricket::IceTransportInternal> ice,
      const webrtc::CryptoOptions& crypto_options) override {
    std::unique_ptr<cricket::FakeIceTransport> fake_ice(
        static_cast<cricket::FakeIceTransport*>(ice.release()));
    return absl::make_unique<FakeDtlsTransport>(std::move(fake_ice));
  }
};

class JsepTransportControllerTest : public JsepTransportController::Observer,
                                    public testing::Test,
                                    public sigslot::has_slots<> {
 public:
  JsepTransportControllerTest() : signaling_thread_(rtc::Thread::Current()) {
    fake_transport_factory_ = absl::make_unique<FakeTransportFactory>();
  }

  void CreateJsepTransportController(
      JsepTransportController::Config config,
      rtc::Thread* signaling_thread = rtc::Thread::Current(),
      rtc::Thread* network_thread = rtc::Thread::Current(),
      cricket::PortAllocator* port_allocator = nullptr) {
    config.transport_observer = this;
    // The tests only works with |fake_transport_factory|;
    config.external_transport_factory = fake_transport_factory_.get();
    // TODO(zstein): Provide an AsyncResolverFactory once it is required.
    transport_controller_ = absl::make_unique<JsepTransportController>(
        signaling_thread, network_thread, port_allocator, nullptr, config);
    ConnectTransportControllerSignals();
  }

  void ConnectTransportControllerSignals() {
    transport_controller_->SignalIceConnectionState.connect(
        this, &JsepTransportControllerTest::OnConnectionState);
    transport_controller_->SignalStandardizedIceConnectionState.connect(
        this, &JsepTransportControllerTest::OnStandardizedIceConnectionState);
    transport_controller_->SignalConnectionState.connect(
        this, &JsepTransportControllerTest::OnCombinedConnectionState);
    transport_controller_->SignalIceGatheringState.connect(
        this, &JsepTransportControllerTest::OnGatheringState);
    transport_controller_->SignalIceCandidatesGathered.connect(
        this, &JsepTransportControllerTest::OnCandidatesGathered);
  }

  std::unique_ptr<cricket::SessionDescription>
  CreateSessionDescriptionWithoutBundle() {
    auto description = absl::make_unique<cricket::SessionDescription>();
    AddAudioSection(description.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                    cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                    nullptr);
    AddVideoSection(description.get(), kVideoMid1, kIceUfrag1, kIcePwd1,
                    cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                    nullptr);
    return description;
  }

  std::unique_ptr<cricket::SessionDescription>
  CreateSessionDescriptionWithBundleGroup() {
    auto description = CreateSessionDescriptionWithoutBundle();
    cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
    bundle_group.AddContentName(kAudioMid1);
    bundle_group.AddContentName(kVideoMid1);
    description->AddGroup(bundle_group);

    return description;
  }

  void AddAudioSection(cricket::SessionDescription* description,
                       const std::string& mid,
                       const std::string& ufrag,
                       const std::string& pwd,
                       cricket::IceMode ice_mode,
                       cricket::ConnectionRole conn_role,
                       rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<cricket::AudioContentDescription> audio(
        new cricket::AudioContentDescription());
    // Set RTCP-mux to be true because the default policy is "mux required".
    audio->set_rtcp_mux(true);
    description->AddContent(mid, cricket::MediaProtocolType::kRtp,
                            /*rejected=*/false, audio.release());
    AddTransportInfo(description, mid, ufrag, pwd, ice_mode, conn_role, cert);
  }

  void AddVideoSection(cricket::SessionDescription* description,
                       const std::string& mid,
                       const std::string& ufrag,
                       const std::string& pwd,
                       cricket::IceMode ice_mode,
                       cricket::ConnectionRole conn_role,
                       rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<cricket::VideoContentDescription> video(
        new cricket::VideoContentDescription());
    // Set RTCP-mux to be true because the default policy is "mux required".
    video->set_rtcp_mux(true);
    description->AddContent(mid, cricket::MediaProtocolType::kRtp,
                            /*rejected=*/false, video.release());
    AddTransportInfo(description, mid, ufrag, pwd, ice_mode, conn_role, cert);
  }

  void AddDataSection(cricket::SessionDescription* description,
                      const std::string& mid,
                      cricket::MediaProtocolType protocol_type,
                      const std::string& ufrag,
                      const std::string& pwd,
                      cricket::IceMode ice_mode,
                      cricket::ConnectionRole conn_role,
                      rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<cricket::DataContentDescription> data(
        new cricket::DataContentDescription());
    data->set_rtcp_mux(true);
    description->AddContent(mid, protocol_type,
                            /*rejected=*/false, data.release());
    AddTransportInfo(description, mid, ufrag, pwd, ice_mode, conn_role, cert);
  }

  void AddTransportInfo(cricket::SessionDescription* description,
                        const std::string& mid,
                        const std::string& ufrag,
                        const std::string& pwd,
                        cricket::IceMode ice_mode,
                        cricket::ConnectionRole conn_role,
                        rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<rtc::SSLFingerprint> fingerprint;
    if (cert) {
      fingerprint = rtc::SSLFingerprint::CreateFromCertificate(*cert);
    }

    cricket::TransportDescription transport_desc(std::vector<std::string>(),
                                                 ufrag, pwd, ice_mode,
                                                 conn_role, fingerprint.get());
    description->AddTransportInfo(cricket::TransportInfo(mid, transport_desc));
  }

  cricket::IceConfig CreateIceConfig(
      int receiving_timeout,
      cricket::ContinualGatheringPolicy continual_gathering_policy) {
    cricket::IceConfig config;
    config.receiving_timeout = receiving_timeout;
    config.continual_gathering_policy = continual_gathering_policy;
    return config;
  }

  Candidate CreateCandidate(const std::string& transport_name, int component) {
    Candidate c;
    c.set_transport_name(transport_name);
    c.set_address(rtc::SocketAddress("192.168.1.1", 8000));
    c.set_component(component);
    c.set_protocol(cricket::UDP_PROTOCOL_NAME);
    c.set_priority(1);
    return c;
  }

  void CreateLocalDescriptionAndCompleteConnectionOnNetworkThread() {
    if (!network_thread_->IsCurrent()) {
      network_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
        CreateLocalDescriptionAndCompleteConnectionOnNetworkThread();
      });
      return;
    }

    auto description = CreateSessionDescriptionWithBundleGroup();
    EXPECT_TRUE(transport_controller_
                    ->SetLocalDescription(SdpType::kOffer, description.get())
                    .ok());

    transport_controller_->MaybeStartGathering();
    auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
        transport_controller_->GetDtlsTransport(kAudioMid1));
    auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
        transport_controller_->GetDtlsTransport(kVideoMid1));
    fake_audio_dtls->fake_ice_transport()->SignalCandidateGathered(
        fake_audio_dtls->fake_ice_transport(),
        CreateCandidate(kAudioMid1, /*component=*/1));
    fake_video_dtls->fake_ice_transport()->SignalCandidateGathered(
        fake_video_dtls->fake_ice_transport(),
        CreateCandidate(kVideoMid1, /*component=*/1));
    fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
    fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
    fake_audio_dtls->fake_ice_transport()->SetConnectionCount(2);
    fake_video_dtls->fake_ice_transport()->SetConnectionCount(2);
    fake_audio_dtls->SetReceiving(true);
    fake_video_dtls->SetReceiving(true);
    fake_audio_dtls->SetWritable(true);
    fake_video_dtls->SetWritable(true);
    fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
    fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  }

 protected:
  void OnConnectionState(cricket::IceConnectionState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    connection_state_ = state;
    ++connection_state_signal_count_;
  }

  void OnStandardizedIceConnectionState(
      PeerConnectionInterface::IceConnectionState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    ice_connection_state_ = state;
    ++ice_connection_state_signal_count_;
  }

  void OnCombinedConnectionState(
      PeerConnectionInterface::PeerConnectionState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    combined_connection_state_ = state;
    ++combined_connection_state_signal_count_;
  }

  void OnGatheringState(cricket::IceGatheringState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    gathering_state_ = state;
    ++gathering_state_signal_count_;
  }

  void OnCandidatesGathered(const std::string& transport_name,
                            const Candidates& candidates) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    candidates_[transport_name].insert(candidates_[transport_name].end(),
                                       candidates.begin(), candidates.end());
    ++candidates_signal_count_;
  }

  // JsepTransportController::Observer overrides.
  bool OnTransportChanged(const std::string& mid,
                          RtpTransportInternal* rtp_transport,
                          cricket::DtlsTransportInternal* dtls_transport,
                          MediaTransportInterface* media_transport) override {
    changed_rtp_transport_by_mid_[mid] = rtp_transport;
    changed_dtls_transport_by_mid_[mid] = dtls_transport;
    changed_media_transport_by_mid_[mid] = media_transport;
    return true;
  }

  // Information received from signals from transport controller.
  cricket::IceConnectionState connection_state_ =
      cricket::kIceConnectionConnecting;
  PeerConnectionInterface::IceConnectionState ice_connection_state_ =
      PeerConnectionInterface::kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState combined_connection_state_ =
      PeerConnectionInterface::PeerConnectionState::kNew;
  bool receiving_ = false;
  cricket::IceGatheringState gathering_state_ = cricket::kIceGatheringNew;
  // transport_name => candidates
  std::map<std::string, Candidates> candidates_;
  // Counts of each signal emitted.
  int connection_state_signal_count_ = 0;
  int ice_connection_state_signal_count_ = 0;
  int combined_connection_state_signal_count_ = 0;
  int receiving_signal_count_ = 0;
  int gathering_state_signal_count_ = 0;
  int candidates_signal_count_ = 0;

  // |network_thread_| should be destroyed after |transport_controller_|
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<FakeTransportFactory> fake_transport_factory_;
  rtc::Thread* const signaling_thread_ = nullptr;
  bool signaled_on_non_signaling_thread_ = false;
  // Used to verify the SignalRtpTransportChanged/SignalDtlsTransportChanged are
  // signaled correctly.
  std::map<std::string, RtpTransportInternal*> changed_rtp_transport_by_mid_;
  std::map<std::string, cricket::DtlsTransportInternal*>
      changed_dtls_transport_by_mid_;
  std::map<std::string, MediaTransportInterface*>
      changed_media_transport_by_mid_;

  // Transport controller needs to be destroyed first, because it may issue
  // callbacks that modify the changed_*_by_mid in the destructor.
  std::unique_ptr<JsepTransportController> transport_controller_;
};

TEST_F(JsepTransportControllerTest, GetRtpTransport) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  auto audio_rtp_transport = transport_controller_->GetRtpTransport(kAudioMid1);
  auto video_rtp_transport = transport_controller_->GetRtpTransport(kVideoMid1);
  EXPECT_NE(nullptr, audio_rtp_transport);
  EXPECT_NE(nullptr, video_rtp_transport);
  EXPECT_NE(audio_rtp_transport, video_rtp_transport);
  // Return nullptr for non-existing ones.
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kAudioMid2));
}

TEST_F(JsepTransportControllerTest, GetDtlsTransport) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_NE(nullptr, transport_controller_->GetRtcpDtlsTransport(kAudioMid1));
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_NE(nullptr, transport_controller_->GetRtcpDtlsTransport(kVideoMid1));
  // Return nullptr for non-existing ones.
  EXPECT_EQ(nullptr, transport_controller_->GetDtlsTransport(kVideoMid2));
  EXPECT_EQ(nullptr, transport_controller_->GetRtcpDtlsTransport(kVideoMid2));
}

TEST_F(JsepTransportControllerTest, GetDtlsTransportWithRtcpMux) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetRtcpDtlsTransport(kAudioMid1));
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetRtcpDtlsTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetMediaTransport(kAudioMid1));
}

TEST_F(JsepTransportControllerTest, GetMediaTransportInCaller) {
  FakeMediaTransportFactory fake_media_transport_factory;
  JsepTransportController::Config config;

  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  config.media_transport_factory = &fake_media_transport_factory;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description.get());

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  FakeMediaTransport* media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kAudioMid1));

  ASSERT_NE(nullptr, media_transport);

  // After SetLocalDescription, media transport should be created as caller.
  EXPECT_TRUE(media_transport->is_caller());
  EXPECT_TRUE(media_transport->pre_shared_key().has_value());

  // Return nullptr for non-existing mids.
  EXPECT_EQ(nullptr, transport_controller_->GetMediaTransport(kVideoMid2));
}

TEST_F(JsepTransportControllerTest, GetMediaTransportInCallee) {
  FakeMediaTransportFactory fake_media_transport_factory;
  JsepTransportController::Config config;

  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  config.media_transport_factory = &fake_media_transport_factory;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description.get());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, description.get())
                  .ok());

  FakeMediaTransport* media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kAudioMid1));

  ASSERT_NE(nullptr, media_transport);

  // After SetRemoteDescription, media transport should be created as callee.
  EXPECT_FALSE(media_transport->is_caller());
  EXPECT_TRUE(media_transport->pre_shared_key().has_value());

  // Return nullptr for non-existing mids.
  EXPECT_EQ(nullptr, transport_controller_->GetMediaTransport(kVideoMid2));
}

TEST_F(JsepTransportControllerTest, GetMediaTransportIsNotSetIfNoSdes) {
  FakeMediaTransportFactory fake_media_transport_factory;
  JsepTransportController::Config config;

  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  config.media_transport_factory = &fake_media_transport_factory;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, description.get())
                  .ok());

  EXPECT_EQ(nullptr, transport_controller_->GetMediaTransport(kAudioMid1));

  // Even if we set local description with crypto now (after the remote offer
  // was set), media transport won't be provided.
  auto description2 = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description2.get());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, description2.get())
                  .ok());

  EXPECT_EQ(nullptr, transport_controller_->GetMediaTransport(kAudioMid1));
}

TEST_F(JsepTransportControllerTest,
       AfterSettingAnswerTheSameMediaTransportIsReturned) {
  FakeMediaTransportFactory fake_media_transport_factory;
  JsepTransportController::Config config;

  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  config.media_transport_factory = &fake_media_transport_factory;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description.get());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, description.get())
                  .ok());

  FakeMediaTransport* media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kAudioMid1));
  EXPECT_NE(nullptr, media_transport);
  EXPECT_TRUE(media_transport->pre_shared_key().has_value());

  // Even if we set local description with crypto now (after the remote offer
  // was set), media transport won't be provided.
  auto description2 = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description2.get());

  RTCError result = transport_controller_->SetLocalDescription(
      SdpType::kAnswer, description2.get());
  EXPECT_TRUE(result.ok()) << result.message();

  // Media transport did not change.
  EXPECT_EQ(media_transport,
            transport_controller_->GetMediaTransport(kAudioMid1));
}

TEST_F(JsepTransportControllerTest, SetIceConfig) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  transport_controller_->SetIceConfig(
      CreateIceConfig(kTimeout, cricket::GATHER_CONTINUALLY));
  FakeDtlsTransport* fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  ASSERT_NE(nullptr, fake_audio_dtls);
  EXPECT_EQ(kTimeout,
            fake_audio_dtls->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(fake_audio_dtls->fake_ice_transport()->gather_continually());

  // Test that value stored in controller is applied to new transports.
  AddAudioSection(description.get(), kAudioMid2, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid2));
  ASSERT_NE(nullptr, fake_audio_dtls);
  EXPECT_EQ(kTimeout,
            fake_audio_dtls->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(fake_audio_dtls->fake_ice_transport()->gather_continually());
}

// Tests the getter and setter of the ICE restart flag.
TEST_F(JsepTransportControllerTest, NeedIceRestart) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, description.get())
                  .ok());

  // Initially NeedsIceRestart should return false.
  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kAudioMid1));
  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kVideoMid1));
  // Set the needs-ice-restart flag and verify NeedsIceRestart starts returning
  // true.
  transport_controller_->SetNeedsIceRestartFlag();
  EXPECT_TRUE(transport_controller_->NeedsIceRestart(kAudioMid1));
  EXPECT_TRUE(transport_controller_->NeedsIceRestart(kVideoMid1));
  // For a nonexistent transport, false should be returned.
  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kVideoMid2));

  // Reset the ice_ufrag/ice_pwd for audio.
  auto audio_transport_info = description->GetTransportInfoByName(kAudioMid1);
  audio_transport_info->description.ice_ufrag = kIceUfrag2;
  audio_transport_info->description.ice_pwd = kIcePwd2;
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  // Because the ICE is only restarted for audio, NeedsIceRestart is expected to
  // return false for audio and true for video.
  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kAudioMid1));
  EXPECT_TRUE(transport_controller_->NeedsIceRestart(kVideoMid1));
}

TEST_F(JsepTransportControllerTest, MaybeStartGathering) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  // After setting the local description, we should be able to start gathering
  // candidates.
  transport_controller_->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, AddRemoveRemoteCandidates) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  transport_controller_->SetLocalDescription(SdpType::kOffer,
                                             description.get());
  transport_controller_->SetRemoteDescription(SdpType::kAnswer,
                                              description.get());
  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  ASSERT_NE(nullptr, fake_audio_dtls);
  Candidates candidates;
  candidates.push_back(
      CreateCandidate(kAudioMid1, cricket::ICE_CANDIDATE_COMPONENT_RTP));
  EXPECT_TRUE(
      transport_controller_->AddRemoteCandidates(kAudioMid1, candidates).ok());
  EXPECT_EQ(1U,
            fake_audio_dtls->fake_ice_transport()->remote_candidates().size());

  EXPECT_TRUE(transport_controller_->RemoveRemoteCandidates(candidates).ok());
  EXPECT_EQ(0U,
            fake_audio_dtls->fake_ice_transport()->remote_candidates().size());
}

TEST_F(JsepTransportControllerTest, SetAndGetLocalCertificate) {
  CreateJsepTransportController(JsepTransportController::Config());

  rtc::scoped_refptr<rtc::RTCCertificate> certificate1 =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> returned_certificate;

  auto description = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(description.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  certificate1);

  // Apply the local certificate.
  EXPECT_TRUE(transport_controller_->SetLocalCertificate(certificate1));
  // Apply the local description.
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  returned_certificate = transport_controller_->GetLocalCertificate(kAudioMid1);
  EXPECT_TRUE(returned_certificate);
  EXPECT_EQ(certificate1->identity()->certificate().ToPEMString(),
            returned_certificate->identity()->certificate().ToPEMString());

  // Should fail if called for a nonexistant transport.
  EXPECT_EQ(nullptr, transport_controller_->GetLocalCertificate(kVideoMid1));

  // Shouldn't be able to change the identity once set.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2 =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("session2", rtc::KT_DEFAULT)));
  EXPECT_FALSE(transport_controller_->SetLocalCertificate(certificate2));
}

TEST_F(JsepTransportControllerTest, GetRemoteSSLCertChain) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  rtc::FakeSSLCertificate fake_certificate("fake_data");

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->SetRemoteSSLCertificate(&fake_certificate);
  std::unique_ptr<rtc::SSLCertChain> returned_cert_chain =
      transport_controller_->GetRemoteSSLCertChain(kAudioMid1);
  ASSERT_TRUE(returned_cert_chain);
  ASSERT_EQ(1u, returned_cert_chain->GetSize());
  EXPECT_EQ(fake_certificate.ToPEMString(),
            returned_cert_chain->Get(0).ToPEMString());

  // Should fail if called for a nonexistant transport.
  EXPECT_FALSE(transport_controller_->GetRemoteSSLCertChain(kAudioMid2));
}

TEST_F(JsepTransportControllerTest, GetDtlsRole) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto offer_certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("offer", rtc::KT_DEFAULT)));
  auto answer_certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("answer", rtc::KT_DEFAULT)));
  transport_controller_->SetLocalCertificate(offer_certificate);

  auto offer_desc = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(offer_desc.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  offer_certificate);
  auto answer_desc = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(answer_desc.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  answer_certificate);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, offer_desc.get())
                  .ok());

  absl::optional<rtc::SSLRole> role =
      transport_controller_->GetDtlsRole(kAudioMid1);
  // The DTLS role is not decided yet.
  EXPECT_FALSE(role);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, answer_desc.get())
                  .ok());
  role = transport_controller_->GetDtlsRole(kAudioMid1);

  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);
}

TEST_F(JsepTransportControllerTest, GetStats) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  cricket::TransportStats stats;
  EXPECT_TRUE(transport_controller_->GetStats(kAudioMid1, &stats));
  EXPECT_EQ(kAudioMid1, stats.transport_name);
  EXPECT_EQ(1u, stats.channel_stats.size());
  // Return false for non-existing transport.
  EXPECT_FALSE(transport_controller_->GetStats(kAudioMid2, &stats));
}

TEST_F(JsepTransportControllerTest, SignalConnectionStateFailed) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_ice = static_cast<cricket::FakeIceTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1)->ice_transport());
  fake_ice->SetCandidatesGatheringComplete();
  fake_ice->SetConnectionCount(1);
  // The connection stats will be failed if there is no active connection.
  fake_ice->SetConnectionCount(0);
  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(1, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kFailed,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(1, combined_connection_state_signal_count_);
}

TEST_F(JsepTransportControllerTest,
       SignalConnectionStateConnectedNoMediaTransport) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));

  // First, have one transport connect, and another fail, to ensure that
  // the first transport connecting didn't trigger a "connected" state signal.
  // We should only get a signal when all are connected.
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetWritable(true);
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  // Decrease the number of the connection to trigger the signal.
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(0);
  fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();

  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(1, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kFailed,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(1, combined_connection_state_signal_count_);

  fake_audio_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  fake_video_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  // Set the connection count to be 2 and the cricket::FakeIceTransport will set
  // the transport state to be STATE_CONNECTING.
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(2);
  fake_video_dtls->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(2, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnected,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(2, combined_connection_state_signal_count_);
}

TEST_F(JsepTransportControllerTest,
       SignalConnectionStateConnectedWithMediaTransport) {
  FakeMediaTransportFactory fake_media_transport_factory;
  JsepTransportController::Config config;
  config.media_transport_factory = &fake_media_transport_factory;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description.get());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));
  fake_audio_dtls->SetWritable(true);
  fake_video_dtls->SetWritable(true);
  // Decreasing connection count from 2 to 1 triggers connection state event.
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(2);
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(2);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  fake_video_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);

  // Still not connected, because we are waiting for media transport.
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnecting, connection_state_,
                 kTimeout);

  FakeMediaTransport* media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kAudioMid1));

  media_transport->SetState(webrtc::MediaTransportState::kWritable);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnecting, connection_state_,
                 kTimeout);

  // Still waiting for the second media transport.
  media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kVideoMid1));
  media_transport->SetState(webrtc::MediaTransportState::kWritable);

  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
}

TEST_F(JsepTransportControllerTest,
       SignalConnectionStateFailedWhenMediaTransportClosed) {
  FakeMediaTransportFactory fake_media_transport_factory;
  JsepTransportController::Config config;
  config.media_transport_factory = &fake_media_transport_factory;
  CreateJsepTransportController(config);
  auto description = CreateSessionDescriptionWithoutBundle();
  AddCryptoSettings(description.get());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));
  fake_audio_dtls->SetWritable(true);
  fake_video_dtls->SetWritable(true);
  // Decreasing connection count from 2 to 1 triggers connection state event.
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(2);
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(2);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  fake_video_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);

  FakeMediaTransport* media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kAudioMid1));

  media_transport->SetState(webrtc::MediaTransportState::kWritable);

  media_transport = static_cast<FakeMediaTransport*>(
      transport_controller_->GetMediaTransport(kVideoMid1));

  media_transport->SetState(webrtc::MediaTransportState::kWritable);

  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);

  media_transport->SetState(webrtc::MediaTransportState::kClosed);
  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
}

TEST_F(JsepTransportControllerTest, SignalConnectionStateComplete) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));

  // First, have one transport connect, and another fail, to ensure that
  // the first transport connecting didn't trigger a "connected" state signal.
  // We should only get a signal when all are connected.
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetWritable(true);
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  // Decrease the number of the connection to trigger the signal.
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(0);
  fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();

  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(1, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kFailed,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(1, combined_connection_state_signal_count_);

  fake_audio_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  fake_video_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  // Set the connection count to be 1 and the cricket::FakeIceTransport will set
  // the transport state to be STATE_COMPLETED.
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_video_dtls->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(2, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnected,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(2, combined_connection_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalIceGatheringStateGathering) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();
  // Should be in the gathering state as soon as any transport starts gathering.
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalIceGatheringStateComplete) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));

  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  // Have one transport finish gathering, to make sure gathering
  // completion wasn't signalled if only one transport finished gathering.
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  EXPECT_EQ(1, gathering_state_signal_count_);

  fake_video_dtls->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);
}

// Test that when the last transport that hasn't finished connecting and/or
// gathering is destroyed, the aggregate state jumps to "completed". This can
// happen if, for example, we have an audio and video transport, the audio
// transport completes, then we start bundling video on the audio transport.
TEST_F(JsepTransportControllerTest,
       SignalingWhenLastIncompleteTransportDestroyed) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_NE(fake_audio_dtls, fake_video_dtls);

  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  // Let the audio transport complete.
  fake_audio_dtls->SetWritable(true);
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetDtlsState(cricket::DTLS_TRANSPORT_CONNECTED);
  EXPECT_EQ(1, gathering_state_signal_count_);

  // Set the remote description and enable the bundle.
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, description.get())
                  .ok());
  // The BUNDLE should be enabled, the incomplete video transport should be
  // deleted and the states shoud be updated.
  fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_EQ(fake_audio_dtls, fake_video_dtls);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(PeerConnectionInterface::kIceConnectionCompleted,
            ice_connection_state_);
  EXPECT_EQ(PeerConnectionInterface::PeerConnectionState::kConnected,
            combined_connection_state_);
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalCandidatesGathered) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get())
                  .ok());
  transport_controller_->MaybeStartGathering();

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->fake_ice_transport()->SignalCandidateGathered(
      fake_audio_dtls->fake_ice_transport(), CreateCandidate(kAudioMid1, 1));
  EXPECT_EQ_WAIT(1, candidates_signal_count_, kTimeout);
  EXPECT_EQ(1u, candidates_[kAudioMid1].size());
}

TEST_F(JsepTransportControllerTest, IceSignalingOccursOnSignalingThread) {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->Start();
  CreateJsepTransportController(JsepTransportController::Config(),
                                signaling_thread_, network_thread_.get(),
                                /*PortAllocator=*/nullptr);
  CreateLocalDescriptionAndCompleteConnectionOnNetworkThread();

  // connecting --> connected --> completed
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  // new --> gathering --> complete
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  EXPECT_EQ_WAIT(1u, candidates_[kAudioMid1].size(), kTimeout);
  EXPECT_EQ_WAIT(1u, candidates_[kVideoMid1].size(), kTimeout);
  EXPECT_EQ(2, candidates_signal_count_);

  EXPECT_TRUE(!signaled_on_non_signaling_thread_);
}

// Older versions of Chrome expect the ICE role to be re-determined when an
// ICE restart occurs, and also don't perform conflict resolution correctly,
// so for now we can't safely stop doing this.
// See: https://bugs.chromium.org/p/chromium/issues/detail?id=628676
// TODO(deadbeef): Remove this when these old versions of Chrome reach a low
// enough population.
TEST_F(JsepTransportControllerTest, IceRoleRedeterminedOnIceRestartByDefault) {
  CreateJsepTransportController(JsepTransportController::Config());
  // Let the |transport_controller_| be the controlled side initially.
  auto remote_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  auto local_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, remote_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer.get())
                  .ok());

  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED,
            fake_dtls->fake_ice_transport()->GetIceRole());

  // New offer will trigger the ICE restart.
  auto restart_local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(restart_local_offer.get(), kAudioMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, restart_local_offer.get())
          .ok());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
}

// Test that if the TransportController was created with the
// |redetermine_role_on_ice_restart| parameter set to false, the role is *not*
// redetermined on an ICE restart.
TEST_F(JsepTransportControllerTest, IceRoleNotRedetermined) {
  JsepTransportController::Config config;
  config.redetermine_role_on_ice_restart = false;

  CreateJsepTransportController(config);
  // Let the |transport_controller_| be the controlled side initially.
  auto remote_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  auto local_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, remote_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer.get())
                  .ok());

  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED,
            fake_dtls->fake_ice_transport()->GetIceRole());

  // New offer will trigger the ICE restart.
  auto restart_local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(restart_local_offer.get(), kAudioMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, restart_local_offer.get())
          .ok());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED,
            fake_dtls->fake_ice_transport()->GetIceRole());
}

// Tests ICE-Lite mode in remote answer.
TEST_F(JsepTransportControllerTest, SetIceRoleWhenIceLiteInRemoteAnswer) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_FULL,
            fake_dtls->fake_ice_transport()->remote_ice_mode());

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_LITE, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_LITE,
            fake_dtls->fake_ice_transport()->remote_ice_mode());
}

// Tests that the ICE role remains "controlling" if a subsequent offer that
// does an ICE restart is received from an ICE lite endpoint. Regression test
// for: https://crbug.com/710760
TEST_F(JsepTransportControllerTest,
       IceRoleIsControllingAfterIceRestartFromIceLiteEndpoint) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto remote_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_LITE, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  auto local_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  // Initial Offer/Answer exchange. If the remote offerer is ICE-Lite, then the
  // local side is the controlling.
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, remote_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer.get())
                  .ok());
  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());

  // In the subsequence remote offer triggers an ICE restart.
  auto remote_offer2 = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer2.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_LITE, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  auto local_answer2 = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer2.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, remote_offer2.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer2.get())
                  .ok());
  fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  // The local side is still the controlling role since the remote side is using
  // ICE-Lite.
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
}

// Tests that the SDP has more than one audio/video m= sections.
TEST_F(JsepTransportControllerTest, MultipleMediaSectionsOfSameTypeWithBundle) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kAudioMid2);
  bundle_group.AddContentName(kVideoMid1);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddAudioSection(remote_answer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());
  // Verify that all the sections are bundled on kAudio1.
  auto transport1 = transport_controller_->GetRtpTransport(kAudioMid1);
  auto transport2 = transport_controller_->GetRtpTransport(kAudioMid2);
  auto transport3 = transport_controller_->GetRtpTransport(kVideoMid1);
  auto transport4 = transport_controller_->GetRtpTransport(kDataMid1);
  EXPECT_EQ(transport1, transport2);
  EXPECT_EQ(transport1, transport3);
  EXPECT_EQ(transport1, transport4);

  // Verify the OnRtpTransport/DtlsTransportChanged signals are fired correctly.
  auto it = changed_rtp_transport_by_mid_.find(kAudioMid2);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  it = changed_rtp_transport_by_mid_.find(kAudioMid2);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  it = changed_rtp_transport_by_mid_.find(kVideoMid1);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  // Verify the DtlsTransport for the SCTP data channel is reset correctly.
  auto it2 = changed_dtls_transport_by_mid_.find(kDataMid1);
  ASSERT_TRUE(it2 != changed_dtls_transport_by_mid_.end());
  EXPECT_EQ(transport1->rtp_packet_transport(), it2->second);
}

// Tests that only a subset of all the m= sections are bundled.
TEST_F(JsepTransportControllerTest, BundleSubsetOfMediaSections) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);

  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddAudioSection(remote_answer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());

  // Verifiy that only |kAudio1| and |kVideo1| are bundled.
  auto transport1 = transport_controller_->GetRtpTransport(kAudioMid1);
  auto transport2 = transport_controller_->GetRtpTransport(kAudioMid2);
  auto transport3 = transport_controller_->GetRtpTransport(kVideoMid1);
  EXPECT_NE(transport1, transport2);
  EXPECT_EQ(transport1, transport3);

  auto it = changed_rtp_transport_by_mid_.find(kVideoMid1);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  it = changed_rtp_transport_by_mid_.find(kAudioMid2);
  EXPECT_TRUE(transport2 == it->second);
}

// Tests that the initial offer/answer only have data section and audio/video
// sections are added in the subsequent offer.
TEST_F(JsepTransportControllerTest, BundleOnDataSectionInSubsequentOffer) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);
  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);
  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());
  auto data_transport = transport_controller_->GetRtpTransport(kDataMid1);

  // Add audio/video sections in subsequent offer.
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  // Reset the bundle group and do another offer/answer exchange.
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);
  local_offer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  remote_answer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());

  auto audio_transport = transport_controller_->GetRtpTransport(kAudioMid1);
  auto video_transport = transport_controller_->GetRtpTransport(kVideoMid1);
  EXPECT_EQ(data_transport, audio_transport);
  EXPECT_EQ(data_transport, video_transport);
}

TEST_F(JsepTransportControllerTest, VideoDataRejectedInAnswer) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);
  // Reject video and data section.
  remote_answer->contents()[1].rejected = true;
  remote_answer->contents()[2].rejected = true;

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());

  // Verify the RtpTransport/DtlsTransport is destroyed correctly.
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetDtlsTransport(kDataMid1));
  // Verify the signals are fired correctly.
  auto it = changed_rtp_transport_by_mid_.find(kVideoMid1);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(nullptr, it->second);
  auto it2 = changed_dtls_transport_by_mid_.find(kDataMid1);
  ASSERT_TRUE(it2 != changed_dtls_transport_by_mid_.end());
  EXPECT_EQ(nullptr, it2->second);
}

// Tests that changing the bundled MID in subsequent offer/answer exchange is
// not supported.
// TODO(bugs.webrtc.org/6704): Change this test to expect success once issue is
// fixed
TEST_F(JsepTransportControllerTest, ChangeBundledMidNotSupported) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);

  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());
  EXPECT_EQ(transport_controller_->GetRtpTransport(kAudioMid1),
            transport_controller_->GetRtpTransport(kVideoMid1));

  // Reorder the bundle group.
  EXPECT_TRUE(bundle_group.RemoveContentName(kAudioMid1));
  bundle_group.AddContentName(kAudioMid1);
  // The answerer uses the new bundle group and now the bundle mid is changed to
  // |kVideo1|.
  remote_answer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  remote_answer->AddGroup(bundle_group);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                   .ok());
}
// Test that rejecting only the first m= section of a BUNDLE group is treated as
// an error, but rejecting all of them works as expected.
TEST_F(JsepTransportControllerTest, RejectFirstContentInBundleGroup) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);
  // Reject audio content in answer.
  remote_answer->contents()[0].rejected = true;

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                   .ok());

  // Reject all the contents.
  remote_answer->contents()[1].rejected = true;
  remote_answer->contents()[2].rejected = true;
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kAudioMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetDtlsTransport(kDataMid1));
}

// Tests that applying non-RTCP-mux offer would fail when kRtcpMuxPolicyRequire
// is used.
TEST_F(JsepTransportControllerTest, ApplyNonRtcpMuxOfferWhenMuxingRequired) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  CreateJsepTransportController(config);
  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  local_offer->contents()[0].media_description()->set_rtcp_mux(false);
  // Applying a non-RTCP-mux offer is expected to fail.
  EXPECT_FALSE(transport_controller_
                   ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                   .ok());
}

// Tests that applying non-RTCP-mux answer would fail when kRtcpMuxPolicyRequire
// is used.
TEST_F(JsepTransportControllerTest, ApplyNonRtcpMuxAnswerWhenMuxingRequired) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  CreateJsepTransportController(config);
  auto local_offer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());

  auto remote_answer = absl::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  // Applying a non-RTCP-mux answer is expected to fail.
  remote_answer->contents()[0].media_description()->set_rtcp_mux(false);
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                   .ok());
}

// This tests that the BUNDLE group in answer should be a subset of the offered
// group.
TEST_F(JsepTransportControllerTest,
       AddContentToBundleGroupInAnswerNotSupported) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = CreateSessionDescriptionWithoutBundle();
  auto remote_answer = CreateSessionDescriptionWithoutBundle();

  cricket::ContentGroup offer_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group.AddContentName(kAudioMid1);
  local_offer->AddGroup(offer_bundle_group);

  cricket::ContentGroup answer_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group.AddContentName(kAudioMid1);
  answer_bundle_group.AddContentName(kVideoMid1);
  remote_answer->AddGroup(answer_bundle_group);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                   .ok());
}

// This tests that the BUNDLE group with non-existing MID should be rejectd.
TEST_F(JsepTransportControllerTest, RejectBundleGroupWithNonExistingMid) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = CreateSessionDescriptionWithoutBundle();
  auto remote_answer = CreateSessionDescriptionWithoutBundle();

  cricket::ContentGroup invalid_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  // The BUNDLE group is invalid because there is no data section in the
  // description.
  invalid_bundle_group.AddContentName(kDataMid1);
  local_offer->AddGroup(invalid_bundle_group);
  remote_answer->AddGroup(invalid_bundle_group);

  EXPECT_FALSE(transport_controller_
                   ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                   .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                   .ok());
}

// This tests that an answer shouldn't be able to remove an m= section from an
// established group without rejecting it.
TEST_F(JsepTransportControllerTest, RemoveContentFromBundleGroup) {
  CreateJsepTransportController(JsepTransportController::Config());

  auto local_offer = CreateSessionDescriptionWithBundleGroup();
  auto remote_answer = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, remote_answer.get())
                  .ok());

  // Do an re-offer/answer.
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get())
                  .ok());
  auto new_answer = CreateSessionDescriptionWithoutBundle();
  cricket::ContentGroup new_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  //  The answer removes video from the BUNDLE group without rejecting it is
  //  invalid.
  new_bundle_group.AddContentName(kAudioMid1);
  new_answer->AddGroup(new_bundle_group);

  // Applying invalid answer is expected to fail.
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, new_answer.get())
                   .ok());

  // Rejected the video content.
  auto video_content = new_answer->GetContentByName(kVideoMid1);
  ASSERT_TRUE(video_content);
  video_content->rejected = true;
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, new_answer.get())
                  .ok());
}

}  // namespace webrtc
