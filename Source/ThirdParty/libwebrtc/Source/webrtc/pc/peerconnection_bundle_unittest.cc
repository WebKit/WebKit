/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/peerconnectionproxy.h"
#include "p2p/base/fakeportallocator.h"
#include "p2p/base/teststunserver.h"
#include "p2p/client/basicportallocator.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

namespace webrtc {

using BundlePolicy = PeerConnectionInterface::BundlePolicy;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using RtcpMuxPolicy = PeerConnectionInterface::RtcpMuxPolicy;
using rtc::SocketAddress;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

constexpr int kDefaultTimeout = 10000;

// TODO(steveanton): These tests should be rewritten to use the standard
// RtpSenderInterface/DtlsTransportInterface objects once they're available in
// the API. The RtpSender can be used to determine which transport a given media
// will use: https://www.w3.org/TR/webrtc/#dom-rtcrtpsender-transport

class PeerConnectionWrapperForBundleTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  bool AddIceCandidateToMedia(cricket::Candidate* candidate,
                              cricket::MediaType media_type) {
    auto* desc = pc()->remote_description()->description();
    for (size_t i = 0; i < desc->contents().size(); i++) {
      const auto& content = desc->contents()[i];
      if (content.media_description()->type() == media_type) {
        candidate->set_transport_name(content.name);
        JsepIceCandidate jsep_candidate(content.name, i, *candidate);
        return pc()->AddIceCandidate(&jsep_candidate);
      }
    }
    RTC_NOTREACHED();
    return false;
  }

  rtc::PacketTransportInternal* voice_rtp_transport_channel() {
    return (voice_channel() ? voice_channel()->rtp_dtls_transport() : nullptr);
  }

  rtc::PacketTransportInternal* voice_rtcp_transport_channel() {
    return (voice_channel() ? voice_channel()->rtcp_dtls_transport() : nullptr);
  }

  cricket::VoiceChannel* voice_channel() {
    return GetInternalPeerConnection()->voice_channel();
  }

  rtc::PacketTransportInternal* video_rtp_transport_channel() {
    return (video_channel() ? video_channel()->rtp_dtls_transport() : nullptr);
  }

  rtc::PacketTransportInternal* video_rtcp_transport_channel() {
    return (video_channel() ? video_channel()->rtcp_dtls_transport() : nullptr);
  }

  cricket::VideoChannel* video_channel() {
    return GetInternalPeerConnection()->video_channel();
  }

  PeerConnection* GetInternalPeerConnection() {
    auto* pci =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc());
    return static_cast<PeerConnection*>(pci->internal());
  }

  // Returns true if the stats indicate that an ICE connection is either in
  // progress or established with the given remote address.
  bool HasConnectionWithRemoteAddress(const SocketAddress& address) {
    auto report = GetStats();
    if (!report) {
      return false;
    }
    std::string matching_candidate_id;
    for (auto* ice_candidate_stats :
         report->GetStatsOfType<RTCRemoteIceCandidateStats>()) {
      if (*ice_candidate_stats->ip == address.HostAsURIString() &&
          *ice_candidate_stats->port == address.port()) {
        matching_candidate_id = ice_candidate_stats->id();
        break;
      }
    }
    if (matching_candidate_id.empty()) {
      return false;
    }
    for (auto* pair_stats :
         report->GetStatsOfType<RTCIceCandidatePairStats>()) {
      if (*pair_stats->remote_candidate_id == matching_candidate_id) {
        if (*pair_stats->state == RTCStatsIceCandidatePairState::kInProgress ||
            *pair_stats->state == RTCStatsIceCandidatePairState::kSucceeded) {
          return true;
        }
      }
    }
    return false;
  }

  rtc::FakeNetworkManager* network() { return network_; }

  void set_network(rtc::FakeNetworkManager* network) { network_ = network; }

 private:
  rtc::FakeNetworkManager* network_;
};

class PeerConnectionBundleTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForBundleTest> WrapperPtr;

  PeerConnectionBundleTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        FakeAudioCaptureModule::Create(), CreateBuiltinAudioEncoderFactory(),
        CreateBuiltinAudioDecoderFactory(), nullptr, nullptr);
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto* fake_network = NewFakeNetwork();
    auto port_allocator =
        rtc::MakeUnique<cricket::BasicPortAllocator>(fake_network);
    port_allocator->set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                              cricket::PORTALLOCATOR_DISABLE_RELAY);
    port_allocator->set_step_delay(cricket::kMinimumStepDelay);
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(
        config, std::move(port_allocator), nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = rtc::MakeUnique<PeerConnectionWrapperForBundleTest>(
        pc_factory_, pc, std::move(observer));
    wrapper->set_network(fake_network);
    return wrapper;
  }

  // Accepts the same arguments as CreatePeerConnection and adds default audio
  // and video tracks.
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithAudioVideo(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->AddAudioTrack("a");
    wrapper->AddVideoTrack("v");
    return wrapper;
  }

  cricket::Candidate CreateLocalUdpCandidate(
      const rtc::SocketAddress& address) {
    cricket::Candidate candidate;
    candidate.set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
    candidate.set_protocol(cricket::UDP_PROTOCOL_NAME);
    candidate.set_address(address);
    candidate.set_type(cricket::LOCAL_PORT_TYPE);
    return candidate;
  }

  rtc::FakeNetworkManager* NewFakeNetwork() {
    // The PeerConnection's port allocator is tied to the PeerConnection's
    // lifetime and expects the underlying NetworkManager to outlive it. If
    // PeerConnectionWrapper owned the NetworkManager, it would be destroyed
    // before the PeerConnection (since subclass members are destroyed before
    // base class members). Therefore, the test fixture will own all the fake
    // networks even though tests should access the fake network through the
    // PeerConnectionWrapper.
    auto* fake_network = new rtc::FakeNetworkManager();
    fake_networks_.emplace_back(fake_network);
    return fake_network;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  std::vector<std::unique_ptr<rtc::FakeNetworkManager>> fake_networks_;
};

SdpContentMutator RemoveRtcpMux() {
  return [](cricket::ContentInfo* content, cricket::TransportInfo* transport) {
    content->media_description()->set_rtcp_mux(false);
  };
}

std::vector<int> GetCandidateComponents(
    const std::vector<IceCandidateInterface*> candidates) {
  std::vector<int> components;
  for (auto* candidate : candidates) {
    components.push_back(candidate->candidate().component());
  }
  return components;
}

// Test that there are 2 local UDP candidates (1 RTP and 1 RTCP candidate) for
// each media section when disabling bundling and disabling RTCP multiplexing.
TEST_F(PeerConnectionBundleTest,
       TwoCandidatesForEachTransportWhenNoBundleNoRtcpMux) {
  const SocketAddress kCallerAddress("1.1.1.1", 0);
  const SocketAddress kCalleeAddress("2.2.2.2", 0);

  RTCConfiguration config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(kCallerAddress);
  auto callee = CreatePeerConnectionWithAudioVideo(config);
  callee->network()->AddInterface(kCalleeAddress);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  auto answer = callee->CreateAnswer(options_no_bundle);
  SdpContentsForEach(RemoveRtcpMux(), answer->description());
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  // Check that caller has separate RTP and RTCP candidates for each media.
  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeout);
  EXPECT_THAT(
      GetCandidateComponents(caller->observer()->GetCandidatesByMline(0)),
      UnorderedElementsAre(cricket::ICE_CANDIDATE_COMPONENT_RTP,
                           cricket::ICE_CANDIDATE_COMPONENT_RTCP));
  EXPECT_THAT(
      GetCandidateComponents(caller->observer()->GetCandidatesByMline(1)),
      UnorderedElementsAre(cricket::ICE_CANDIDATE_COMPONENT_RTP,
                           cricket::ICE_CANDIDATE_COMPONENT_RTCP));

  // Check that callee has separate RTP and RTCP candidates for each media.
  EXPECT_TRUE_WAIT(callee->IsIceGatheringDone(), kDefaultTimeout);
  EXPECT_THAT(
      GetCandidateComponents(callee->observer()->GetCandidatesByMline(0)),
      UnorderedElementsAre(cricket::ICE_CANDIDATE_COMPONENT_RTP,
                           cricket::ICE_CANDIDATE_COMPONENT_RTCP));
  EXPECT_THAT(
      GetCandidateComponents(callee->observer()->GetCandidatesByMline(1)),
      UnorderedElementsAre(cricket::ICE_CANDIDATE_COMPONENT_RTP,
                           cricket::ICE_CANDIDATE_COMPONENT_RTCP));
}

// Test that there is 1 local UDP candidate for both RTP and RTCP for each media
// section when disabling bundle but enabling RTCP multiplexing.
TEST_F(PeerConnectionBundleTest,
       OneCandidateForEachTransportWhenNoBundleButRtcpMux) {
  const SocketAddress kCallerAddress("1.1.1.1", 0);

  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(kCallerAddress);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswer(options_no_bundle)));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeout);

  EXPECT_EQ(1u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(1u, caller->observer()->GetCandidatesByMline(1).size());
}

// Test that there is 1 local UDP candidate in only the first media section when
// bundling and enabling RTCP multiplexing.
TEST_F(PeerConnectionBundleTest,
       OneCandidateOnlyOnFirstTransportWhenBundleAndRtcpMux) {
  const SocketAddress kCallerAddress("1.1.1.1", 0);

  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(kCallerAddress);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(caller->SetRemoteDescription(callee->CreateAnswer()));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeout);

  EXPECT_EQ(1u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(0u, caller->observer()->GetCandidatesByMline(1).size());
}

// The following parameterized test verifies that an offer/answer with varying
// bundle policies and either bundle in the answer or not will produce the
// expected RTP transports for audio and video. In particular, for bundling we
// care about whether they are separate transports or the same.

enum class BundleIncluded { kBundleInAnswer, kBundleNotInAnswer };
std::ostream& operator<<(std::ostream& out, BundleIncluded value) {
  switch (value) {
    case BundleIncluded::kBundleInAnswer:
      return out << "bundle in answer";
    case BundleIncluded::kBundleNotInAnswer:
      return out << "bundle not in answer";
  }
  return out << "unknown";
}

class PeerConnectionBundleMatrixTest
    : public PeerConnectionBundleTest,
      public ::testing::WithParamInterface<
          std::tuple<BundlePolicy, BundleIncluded, bool, bool>> {
 protected:
  PeerConnectionBundleMatrixTest() {
    bundle_policy_ = std::get<0>(GetParam());
    bundle_included_ = std::get<1>(GetParam());
    expected_same_before_ = std::get<2>(GetParam());
    expected_same_after_ = std::get<3>(GetParam());
  }

  PeerConnectionInterface::BundlePolicy bundle_policy_;
  BundleIncluded bundle_included_;
  bool expected_same_before_;
  bool expected_same_after_;
};

TEST_P(PeerConnectionBundleMatrixTest,
       VerifyTransportsBeforeAndAfterSettingRemoteAnswer) {
  RTCConfiguration config;
  config.bundle_policy = bundle_policy_;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  bool equal_before = (caller->voice_rtp_transport_channel() ==
                       caller->video_rtp_transport_channel());
  EXPECT_EQ(expected_same_before_, equal_before);

  RTCOfferAnswerOptions options;
  options.use_rtp_mux = (bundle_included_ == BundleIncluded::kBundleInAnswer);
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal(options)));
  bool equal_after = (caller->voice_rtp_transport_channel() ==
                      caller->video_rtp_transport_channel());
  EXPECT_EQ(expected_same_after_, equal_after);
}

// The max-bundle policy means we should anticipate bundling being negotiated,
// and multiplex audio/video from the start.
// For all other policies, bundling should only be enabled if negotiated by the
// answer.
INSTANTIATE_TEST_CASE_P(
    PeerConnectionBundleTest,
    PeerConnectionBundleMatrixTest,
    Values(std::make_tuple(BundlePolicy::kBundlePolicyBalanced,
                           BundleIncluded::kBundleInAnswer,
                           false,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyBalanced,
                           BundleIncluded::kBundleNotInAnswer,
                           false,
                           false),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxBundle,
                           BundleIncluded::kBundleInAnswer,
                           true,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxBundle,
                           BundleIncluded::kBundleNotInAnswer,
                           true,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxCompat,
                           BundleIncluded::kBundleInAnswer,
                           false,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxCompat,
                           BundleIncluded::kBundleNotInAnswer,
                           false,
                           false)));

// Test that the audio/video transports on the callee side are the same before
// and after setting a local answer when max BUNDLE is enabled and an offer with
// BUNDLE is received.
TEST_F(PeerConnectionBundleTest,
       TransportsSameForMaxBundleWithBundleInRemoteOffer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  RTCOfferAnswerOptions options_with_bundle;
  options_with_bundle.use_rtp_mux = true;
  ASSERT_TRUE(callee->SetRemoteDescription(
      caller->CreateOfferAndSetAsLocal(options_with_bundle)));

  EXPECT_EQ(callee->voice_rtp_transport_channel(),
            callee->video_rtp_transport_channel());

  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  EXPECT_EQ(callee->voice_rtp_transport_channel(),
            callee->video_rtp_transport_channel());
}

TEST_F(PeerConnectionBundleTest,
       FailToSetRemoteOfferWithNoBundleWhenBundlePolicyMaxBundle) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  EXPECT_FALSE(callee->SetRemoteDescription(
      caller->CreateOfferAndSetAsLocal(options_no_bundle)));
}

// Test that if the media section which has the bundled transport is rejected,
// then the peers still connect and the bundled transport switches to the other
// media section.
// Note: This is currently failing because of the following bug:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6280
TEST_F(PeerConnectionBundleTest,
       DISABLED_SuccessfullyNegotiateMaxBundleIfBundleTransportMediaRejected) {
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnection();
  callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal(options)));

  EXPECT_FALSE(caller->voice_rtp_transport_channel());
  EXPECT_TRUE(caller->video_rtp_transport_channel());
}

// When requiring RTCP multiplexing, the PeerConnection never makes RTCP
// transport channels.
TEST_F(PeerConnectionBundleTest, NeverCreateRtcpTransportWithRtcpMuxRequired) {
  RTCConfiguration config;
  config.rtcp_mux_policy = RtcpMuxPolicy::kRtcpMuxPolicyRequire;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_FALSE(caller->voice_rtcp_transport_channel());
  EXPECT_FALSE(caller->video_rtcp_transport_channel());

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->voice_rtcp_transport_channel());
  EXPECT_FALSE(caller->video_rtcp_transport_channel());
}

// When negotiating RTCP multiplexing, the PeerConnection makes RTCP transport
// channels when the offer is sent, but will destroy them once the remote answer
// is set.
TEST_F(PeerConnectionBundleTest,
       CreateRtcpTransportOnlyBeforeAnswerWithRtcpMuxNegotiate) {
  RTCConfiguration config;
  config.rtcp_mux_policy = RtcpMuxPolicy::kRtcpMuxPolicyNegotiate;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_TRUE(caller->voice_rtcp_transport_channel());
  EXPECT_TRUE(caller->video_rtcp_transport_channel());

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->voice_rtcp_transport_channel());
  EXPECT_FALSE(caller->video_rtcp_transport_channel());
}

TEST_F(PeerConnectionBundleTest, FailToSetDescriptionWithBundleAndNoRtcpMux) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  auto offer = caller->CreateOffer(options);
  SdpContentsForEach(RemoveRtcpMux(), offer->description());

  std::string error;
  EXPECT_FALSE(caller->SetLocalDescription(CloneSessionDescription(offer.get()),
                                           &error));
  EXPECT_EQ(
      "Failed to set local offer sdp: rtcp-mux must be enabled when BUNDLE is "
      "enabled.",
      error);

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer), &error));
  EXPECT_EQ(
      "Failed to set remote offer sdp: rtcp-mux must be enabled when BUNDLE is "
      "enabled.",
      error);
}

// Test that candidates sent to the "video" transport do not get pushed down to
// the "audio" transport channel when bundling.
TEST_F(PeerConnectionBundleTest,
       IgnoreCandidatesForUnusedTransportWhenBundling) {
  const SocketAddress kAudioAddress1("1.1.1.1", 1111);
  const SocketAddress kAudioAddress2("2.2.2.2", 2222);
  const SocketAddress kVideoAddress("3.3.3.3", 3333);
  const SocketAddress kCallerAddress("4.4.4.4", 0);
  const SocketAddress kCalleeAddress("5.5.5.5", 0);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  caller->network()->AddInterface(kCallerAddress);
  callee->network()->AddInterface(kCalleeAddress);

  RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // The way the *_WAIT checks work is they only wait if the condition fails,
  // which does not help in the case where state is not changing. This is
  // problematic in this test since we want to verify that adding a video
  // candidate does _not_ change state. So we interleave candidates and assume
  // that messages are executed in the order they were posted.

  cricket::Candidate audio_candidate1 = CreateLocalUdpCandidate(kAudioAddress1);
  ASSERT_TRUE(caller->AddIceCandidateToMedia(&audio_candidate1,
                                             cricket::MEDIA_TYPE_AUDIO));

  cricket::Candidate video_candidate = CreateLocalUdpCandidate(kVideoAddress);
  ASSERT_TRUE(caller->AddIceCandidateToMedia(&video_candidate,
                                             cricket::MEDIA_TYPE_VIDEO));

  cricket::Candidate audio_candidate2 = CreateLocalUdpCandidate(kAudioAddress2);
  ASSERT_TRUE(caller->AddIceCandidateToMedia(&audio_candidate2,
                                             cricket::MEDIA_TYPE_AUDIO));

  EXPECT_TRUE_WAIT(caller->HasConnectionWithRemoteAddress(kAudioAddress1),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller->HasConnectionWithRemoteAddress(kAudioAddress2),
                   kDefaultTimeout);
  EXPECT_FALSE(caller->HasConnectionWithRemoteAddress(kVideoAddress));
}

// Test that the transport used by both audio and video is the transport
// associated with the first MID in the answer BUNDLE group, even if it's in a
// different order from the offer.
TEST_F(PeerConnectionBundleTest, BundleOnFirstMidInAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto* old_video_transport = caller->video_rtp_transport_channel();

  auto answer = callee->CreateAnswer();
  auto* old_bundle_group =
      answer->description()->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  ASSERT_THAT(old_bundle_group->content_names(),
              ElementsAre(cricket::CN_AUDIO, cricket::CN_VIDEO));
  answer->description()->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);

  cricket::ContentGroup new_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group.AddContentName(cricket::CN_VIDEO);
  new_bundle_group.AddContentName(cricket::CN_AUDIO);
  answer->description()->AddGroup(new_bundle_group);

  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  EXPECT_EQ(old_video_transport, caller->video_rtp_transport_channel());
  EXPECT_EQ(caller->voice_rtp_transport_channel(),
            caller->video_rtp_transport_channel());
}

}  // namespace webrtc
