/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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
#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/peerconnectionproxy.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/gunit.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/virtualsocketserver.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using rtc::SocketAddress;
using ::testing::Combine;
using ::testing::Values;

constexpr int kIceCandidatesTimeout = 10000;

class PeerConnectionWrapperForIceTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  // Adds a new ICE candidate to the first transport.
  bool AddIceCandidate(cricket::Candidate* candidate) {
    RTC_DCHECK(pc()->remote_description());
    const auto* desc = pc()->remote_description()->description();
    RTC_DCHECK(desc->contents().size() > 0);
    const auto& first_content = desc->contents()[0];
    candidate->set_transport_name(first_content.name);
    std::unique_ptr<IceCandidateInterface> jsep_candidate =
        CreateIceCandidate(first_content.name, 0, *candidate);
    return pc()->AddIceCandidate(jsep_candidate.get());
  }

  // Returns ICE candidates from the remote session description.
  std::vector<const IceCandidateInterface*>
  GetIceCandidatesFromRemoteDescription() {
    const SessionDescriptionInterface* sdesc = pc()->remote_description();
    RTC_DCHECK(sdesc);
    std::vector<const IceCandidateInterface*> candidates;
    for (size_t mline_index = 0; mline_index < sdesc->number_of_mediasections();
         mline_index++) {
      const auto* candidate_collection = sdesc->candidates(mline_index);
      for (size_t i = 0; i < candidate_collection->count(); i++) {
        candidates.push_back(candidate_collection->at(i));
      }
    }
    return candidates;
  }

  rtc::FakeNetworkManager* network() { return network_; }

  void set_network(rtc::FakeNetworkManager* network) { network_ = network; }

 private:
  rtc::FakeNetworkManager* network_;
};

class PeerConnectionIceBaseTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForIceTest> WrapperPtr;

  explicit PeerConnectionIceBaseTest(SdpSemantics sdp_semantics)
      : vss_(new rtc::VirtualSocketServer()),
        main_(vss_.get()),
        sdp_semantics_(sdp_semantics) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        rtc::scoped_refptr<AudioDeviceModule>(FakeAudioCaptureModule::Create()),
        CreateBuiltinAudioEncoderFactory(), CreateBuiltinAudioDecoderFactory(),
        CreateBuiltinVideoEncoderFactory(), CreateBuiltinVideoDecoderFactory(),
        nullptr /* audio_mixer */, nullptr /* audio_processing */);
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto* fake_network = NewFakeNetwork();
    auto port_allocator =
        absl::make_unique<cricket::BasicPortAllocator>(fake_network);
    port_allocator->set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                              cricket::PORTALLOCATOR_DISABLE_RELAY);
    port_allocator->set_step_delay(cricket::kMinimumStepDelay);
    RTCConfiguration modified_config = config;
    modified_config.sdp_semantics = sdp_semantics_;
    auto observer = absl::make_unique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(
        modified_config, std::move(port_allocator), nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = absl::make_unique<PeerConnectionWrapperForIceTest>(
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

  // Remove all ICE ufrag/pwd lines from the given session description.
  void RemoveIceUfragPwd(SessionDescriptionInterface* sdesc) {
    SetIceUfragPwd(sdesc, "", "");
  }

  // Sets all ICE ufrag/pwds on the given session description.
  void SetIceUfragPwd(SessionDescriptionInterface* sdesc,
                      const std::string& ufrag,
                      const std::string& pwd) {
    auto* desc = sdesc->description();
    for (const auto& content : desc->contents()) {
      auto* transport_info = desc->GetTransportInfoByName(content.name);
      transport_info->description.ice_ufrag = ufrag;
      transport_info->description.ice_pwd = pwd;
    }
  }

  // Set ICE mode on the given session description.
  void SetIceMode(SessionDescriptionInterface* sdesc,
                  const cricket::IceMode ice_mode) {
    auto* desc = sdesc->description();
    for (const auto& content : desc->contents()) {
      auto* transport_info = desc->GetTransportInfoByName(content.name);
      transport_info->description.ice_mode = ice_mode;
    }
  }

  cricket::TransportDescription* GetFirstTransportDescription(
      SessionDescriptionInterface* sdesc) {
    auto* desc = sdesc->description();
    RTC_DCHECK(desc->contents().size() > 0);
    auto* transport_info =
        desc->GetTransportInfoByName(desc->contents()[0].name);
    RTC_DCHECK(transport_info);
    return &transport_info->description;
  }

  const cricket::TransportDescription* GetFirstTransportDescription(
      const SessionDescriptionInterface* sdesc) {
    auto* desc = sdesc->description();
    RTC_DCHECK(desc->contents().size() > 0);
    auto* transport_info =
        desc->GetTransportInfoByName(desc->contents()[0].name);
    RTC_DCHECK(transport_info);
    return &transport_info->description;
  }

  // TODO(qingsi): Rewrite this method in terms of the standard IceTransport
  // after it is implemented.
  cricket::IceRole GetIceRole(const WrapperPtr& pc_wrapper_ptr) {
    auto* pc_proxy =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc_wrapper_ptr->pc());
    PeerConnection* pc = static_cast<PeerConnection*>(pc_proxy->internal());
    for (auto transceiver : pc->GetTransceiversInternal()) {
      if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
        cricket::BaseChannel* channel = transceiver->internal()->channel();
        if (channel) {
          auto dtls_transport = static_cast<cricket::DtlsTransportInternal*>(
              channel->rtp_packet_transport());
          return dtls_transport->ice_transport()->GetIceRole();
        }
      }
    }
    RTC_NOTREACHED();
    return cricket::ICEROLE_UNKNOWN;
  }

  bool AddCandidateToFirstTransport(cricket::Candidate* candidate,
                                    SessionDescriptionInterface* sdesc) {
    auto* desc = sdesc->description();
    RTC_DCHECK(desc->contents().size() > 0);
    const auto& first_content = desc->contents()[0];
    candidate->set_transport_name(first_content.name);
    std::unique_ptr<IceCandidateInterface> jsep_candidate =
        CreateIceCandidate(first_content.name, 0, *candidate);
    return sdesc->AddCandidate(jsep_candidate.get());
  }

  rtc::FakeNetworkManager* NewFakeNetwork() {
    // The PeerConnection's port allocator is tied to the PeerConnection's
    // lifetime and expects the underlying NetworkManager to outlive it. That
    // prevents us from having the PeerConnectionWrapper own the fake network.
    // Therefore, the test fixture will own all the fake networks even though
    // tests should access the fake network through the PeerConnectionWrapper.
    auto* fake_network = new rtc::FakeNetworkManager();
    fake_networks_.emplace_back(fake_network);
    return fake_network;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  std::vector<std::unique_ptr<rtc::FakeNetworkManager>> fake_networks_;
  const SdpSemantics sdp_semantics_;
};

class PeerConnectionIceTest
    : public PeerConnectionIceBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  PeerConnectionIceTest() : PeerConnectionIceBaseTest(GetParam()) {
    webrtc::metrics::Reset();
  }
};

::testing::AssertionResult AssertCandidatesEqual(const char* a_expr,
                                                 const char* b_expr,
                                                 const cricket::Candidate& a,
                                                 const cricket::Candidate& b) {
  rtc::StringBuilder failure_info;
  if (a.component() != b.component()) {
    failure_info << "\ncomponent: " << a.component() << " != " << b.component();
  }
  if (a.protocol() != b.protocol()) {
    failure_info << "\nprotocol: " << a.protocol() << " != " << b.protocol();
  }
  if (a.address() != b.address()) {
    failure_info << "\naddress: " << a.address().ToString()
                 << " != " << b.address().ToString();
  }
  if (a.type() != b.type()) {
    failure_info << "\ntype: " << a.type() << " != " << b.type();
  }
  std::string failure_info_str = failure_info.str();
  if (failure_info_str.empty()) {
    return ::testing::AssertionSuccess();
  } else {
    return ::testing::AssertionFailure()
           << a_expr << " and " << b_expr << " are not equal"
           << failure_info_str;
  }
}

TEST_P(PeerConnectionIceTest, OfferContainsGatheredCandidates) {
  const SocketAddress kLocalAddress("1.1.1.1", 0);

  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(kLocalAddress);

  // Start ICE candidate gathering by setting the local offer.
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kIceCandidatesTimeout);

  auto offer = caller->CreateOffer();
  EXPECT_LT(0u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(caller->observer()->GetCandidatesByMline(0).size(),
            offer->candidates(0)->count());
  EXPECT_LT(0u, caller->observer()->GetCandidatesByMline(1).size());
  EXPECT_EQ(caller->observer()->GetCandidatesByMline(1).size(),
            offer->candidates(1)->count());
}

TEST_P(PeerConnectionIceTest, AnswerContainsGatheredCandidates) {
  const SocketAddress kCallerAddress("1.1.1.1", 0);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(kCallerAddress);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  EXPECT_TRUE_WAIT(callee->IsIceGatheringDone(), kIceCandidatesTimeout);

  auto* answer = callee->pc()->local_description();
  EXPECT_LT(0u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(callee->observer()->GetCandidatesByMline(0).size(),
            answer->candidates(0)->count());
  EXPECT_LT(0u, caller->observer()->GetCandidatesByMline(1).size());
  EXPECT_EQ(callee->observer()->GetCandidatesByMline(1).size(),
            answer->candidates(1)->count());
}

TEST_P(PeerConnectionIceTest,
       CanSetRemoteSessionDescriptionWithRemoteCandidates) {
  const SocketAddress kCallerAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOfferAndSetAsLocal();
  cricket::Candidate candidate = CreateLocalUdpCandidate(kCallerAddress);
  AddCandidateToFirstTransport(&candidate, offer.get());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto remote_candidates = callee->GetIceCandidatesFromRemoteDescription();
  ASSERT_EQ(1u, remote_candidates.size());
  EXPECT_PRED_FORMAT2(AssertCandidatesEqual, candidate,
                      remote_candidates[0]->candidate());
}

TEST_P(PeerConnectionIceTest, SetLocalDescriptionFailsIfNoIceCredentials) {
  auto caller = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  RemoveIceUfragPwd(offer.get());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}

TEST_P(PeerConnectionIceTest, SetRemoteDescriptionFailsIfNoIceCredentials) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOfferAndSetAsLocal();
  RemoveIceUfragPwd(offer.get());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}

// Test that doing an offer/answer exchange with no transport (i.e., no data
// channel or media) results in the ICE connection state staying at New.
TEST_P(PeerConnectionIceTest,
       OfferAnswerWithNoTransportsDoesNotChangeIceConnectionState) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  EXPECT_EQ(PeerConnectionInterface::kIceConnectionNew,
            caller->pc()->ice_connection_state());
  EXPECT_EQ(PeerConnectionInterface::kIceConnectionNew,
            callee->pc()->ice_connection_state());
}

// The following group tests that ICE candidates are not generated before
// SetLocalDescription is called on a PeerConnection.

TEST_P(PeerConnectionIceTest, NoIceCandidatesBeforeSetLocalDescription) {
  const SocketAddress kLocalAddress("1.1.1.1", 0);

  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(kLocalAddress);

  // Pump for 1 second and verify that no candidates are generated.
  rtc::Thread::Current()->ProcessMessages(1000);

  EXPECT_EQ(0u, caller->observer()->candidates_.size());
}
TEST_P(PeerConnectionIceTest,
       NoIceCandidatesBeforeAnswerSetAsLocalDescription) {
  const SocketAddress kCallerAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(kCallerAddress);

  auto offer = caller->CreateOfferAndSetAsLocal();
  cricket::Candidate candidate = CreateLocalUdpCandidate(kCallerAddress);
  AddCandidateToFirstTransport(&candidate, offer.get());
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  // Pump for 1 second and verify that no candidates are generated.
  rtc::Thread::Current()->ProcessMessages(1000);

  EXPECT_EQ(0u, callee->observer()->candidates_.size());
}

TEST_P(PeerConnectionIceTest, CannotAddCandidateWhenRemoteDescriptionNotSet) {
  const SocketAddress kCalleeAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  cricket::Candidate candidate = CreateLocalUdpCandidate(kCalleeAddress);
  std::unique_ptr<IceCandidateInterface> jsep_candidate =
      CreateIceCandidate(cricket::CN_AUDIO, 0, candidate);

  EXPECT_FALSE(caller->pc()->AddIceCandidate(jsep_candidate.get()));

  caller->CreateOfferAndSetAsLocal();

  EXPECT_FALSE(caller->pc()->AddIceCandidate(jsep_candidate.get()));
  EXPECT_EQ(
      2, webrtc::metrics::NumSamples("WebRTC.PeerConnection.AddIceCandidate"));
  EXPECT_EQ(
      2, webrtc::metrics::NumEvents("WebRTC.PeerConnection.AddIceCandidate",
                                    kAddIceCandidateFailNoRemoteDescription));
}

TEST_P(PeerConnectionIceTest, CannotAddCandidateWhenPeerConnectionClosed) {
  const SocketAddress kCalleeAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  cricket::Candidate candidate = CreateLocalUdpCandidate(kCalleeAddress);
  auto* audio_content = cricket::GetFirstAudioContent(
      caller->pc()->local_description()->description());
  std::unique_ptr<IceCandidateInterface> jsep_candidate =
      CreateIceCandidate(audio_content->name, 0, candidate);

  caller->pc()->Close();

  EXPECT_FALSE(caller->pc()->AddIceCandidate(jsep_candidate.get()));
}

TEST_P(PeerConnectionIceTest, DuplicateIceCandidateIgnoredWhenAdded) {
  const SocketAddress kCalleeAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  cricket::Candidate candidate = CreateLocalUdpCandidate(kCalleeAddress);
  caller->AddIceCandidate(&candidate);
  EXPECT_TRUE(caller->AddIceCandidate(&candidate));
  EXPECT_EQ(1u, caller->GetIceCandidatesFromRemoteDescription().size());
}

TEST_P(PeerConnectionIceTest,
       CannotRemoveIceCandidatesWhenPeerConnectionClosed) {
  const SocketAddress kCalleeAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  cricket::Candidate candidate = CreateLocalUdpCandidate(kCalleeAddress);
  auto* audio_content = cricket::GetFirstAudioContent(
      caller->pc()->local_description()->description());
  std::unique_ptr<IceCandidateInterface> ice_candidate =
      CreateIceCandidate(audio_content->name, 0, candidate);

  ASSERT_TRUE(caller->pc()->AddIceCandidate(ice_candidate.get()));

  caller->pc()->Close();

  EXPECT_FALSE(caller->pc()->RemoveIceCandidates({candidate}));
}

TEST_P(PeerConnectionIceTest,
       AddRemoveCandidateWithEmptyTransportDoesNotCrash) {
  const SocketAddress kCalleeAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // |candidate.transport_name()| is empty.
  cricket::Candidate candidate = CreateLocalUdpCandidate(kCalleeAddress);
  auto* audio_content = cricket::GetFirstAudioContent(
      caller->pc()->local_description()->description());
  std::unique_ptr<IceCandidateInterface> ice_candidate =
      CreateIceCandidate(audio_content->name, 0, candidate);
  EXPECT_TRUE(caller->pc()->AddIceCandidate(ice_candidate.get()));
  EXPECT_TRUE(caller->pc()->RemoveIceCandidates({candidate}));
}

TEST_P(PeerConnectionIceTest, RemoveCandidateRemovesFromRemoteDescription) {
  const SocketAddress kCalleeAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  cricket::Candidate candidate = CreateLocalUdpCandidate(kCalleeAddress);
  ASSERT_TRUE(caller->AddIceCandidate(&candidate));
  EXPECT_TRUE(caller->pc()->RemoveIceCandidates({candidate}));
  EXPECT_EQ(0u, caller->GetIceCandidatesFromRemoteDescription().size());
}

// Test that if a candidate is added via AddIceCandidate and via an updated
// remote description, then both candidates appear in the stored remote
// description.
TEST_P(PeerConnectionIceTest,
       CandidateInSubsequentOfferIsAddedToRemoteDescription) {
  const SocketAddress kCallerAddress1("1.1.1.1", 1111);
  const SocketAddress kCallerAddress2("2.2.2.2", 2222);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // Add one candidate via |AddIceCandidate|.
  cricket::Candidate candidate1 = CreateLocalUdpCandidate(kCallerAddress1);
  ASSERT_TRUE(callee->AddIceCandidate(&candidate1));

  // Add the second candidate via a reoffer.
  auto offer = caller->CreateOffer();
  cricket::Candidate candidate2 = CreateLocalUdpCandidate(kCallerAddress2);
  AddCandidateToFirstTransport(&candidate2, offer.get());

  // Expect both candidates to appear in the callee's remote description.
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  EXPECT_EQ(2u, callee->GetIceCandidatesFromRemoteDescription().size());
}

// The follow test verifies that SetLocal/RemoteDescription fails when an offer
// has either ICE ufrag/pwd too short or too long and succeeds otherwise.
// The standard (https://tools.ietf.org/html/rfc5245#section-15.4) says that
// pwd must be 22-256 characters and ufrag must be 4-256 characters.
TEST_P(PeerConnectionIceTest, VerifyUfragPwdLength) {
  auto set_local_description_with_ufrag_pwd_length = [this](int ufrag_len,
                                                            int pwd_len) {
    auto pc = CreatePeerConnectionWithAudioVideo();
    auto offer = pc->CreateOffer();
    SetIceUfragPwd(offer.get(), std::string(ufrag_len, 'x'),
                   std::string(pwd_len, 'x'));
    return pc->SetLocalDescription(std::move(offer));
  };

  auto set_remote_description_with_ufrag_pwd_length = [this](int ufrag_len,
                                                             int pwd_len) {
    auto pc = CreatePeerConnectionWithAudioVideo();
    auto offer = pc->CreateOffer();
    SetIceUfragPwd(offer.get(), std::string(ufrag_len, 'x'),
                   std::string(pwd_len, 'x'));
    return pc->SetRemoteDescription(std::move(offer));
  };

  EXPECT_FALSE(set_local_description_with_ufrag_pwd_length(3, 22));
  EXPECT_FALSE(set_remote_description_with_ufrag_pwd_length(3, 22));
  EXPECT_FALSE(set_local_description_with_ufrag_pwd_length(257, 22));
  EXPECT_FALSE(set_remote_description_with_ufrag_pwd_length(257, 22));
  EXPECT_FALSE(set_local_description_with_ufrag_pwd_length(4, 21));
  EXPECT_FALSE(set_remote_description_with_ufrag_pwd_length(4, 21));
  EXPECT_FALSE(set_local_description_with_ufrag_pwd_length(4, 257));
  EXPECT_FALSE(set_remote_description_with_ufrag_pwd_length(4, 257));
  EXPECT_TRUE(set_local_description_with_ufrag_pwd_length(4, 22));
  EXPECT_TRUE(set_remote_description_with_ufrag_pwd_length(4, 22));
  EXPECT_TRUE(set_local_description_with_ufrag_pwd_length(256, 256));
  EXPECT_TRUE(set_remote_description_with_ufrag_pwd_length(256, 256));
}

::testing::AssertionResult AssertIpInCandidates(
    const char* address_expr,
    const char* candidates_expr,
    const SocketAddress& address,
    const std::vector<IceCandidateInterface*> candidates) {
  rtc::StringBuilder candidate_hosts;
  for (const auto* candidate : candidates) {
    const auto& candidate_ip = candidate->candidate().address().ipaddr();
    if (candidate_ip == address.ipaddr()) {
      return ::testing::AssertionSuccess();
    }
    candidate_hosts << "\n" << candidate_ip.ToString();
  }
  return ::testing::AssertionFailure()
         << address_expr << " (host " << address.HostAsURIString()
         << ") not in " << candidates_expr
         << " which have the following address hosts:" << candidate_hosts.str();
}

TEST_P(PeerConnectionIceTest, CandidatesGeneratedForEachLocalInterface) {
  const SocketAddress kLocalAddress1("1.1.1.1", 0);
  const SocketAddress kLocalAddress2("2.2.2.2", 0);

  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(kLocalAddress1);
  caller->network()->AddInterface(kLocalAddress2);

  caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kIceCandidatesTimeout);

  auto candidates = caller->observer()->GetCandidatesByMline(0);
  EXPECT_PRED_FORMAT2(AssertIpInCandidates, kLocalAddress1, candidates);
  EXPECT_PRED_FORMAT2(AssertIpInCandidates, kLocalAddress2, candidates);
}

TEST_P(PeerConnectionIceTest, TrickledSingleCandidateAddedToRemoteDescription) {
  const SocketAddress kCallerAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  cricket::Candidate candidate = CreateLocalUdpCandidate(kCallerAddress);
  callee->AddIceCandidate(&candidate);
  auto candidates = callee->GetIceCandidatesFromRemoteDescription();
  ASSERT_EQ(1u, candidates.size());
  EXPECT_PRED_FORMAT2(AssertCandidatesEqual, candidate,
                      candidates[0]->candidate());
}

TEST_P(PeerConnectionIceTest, TwoTrickledCandidatesAddedToRemoteDescription) {
  const SocketAddress kCalleeAddress1("1.1.1.1", 1111);
  const SocketAddress kCalleeAddress2("2.2.2.2", 2222);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  cricket::Candidate candidate1 = CreateLocalUdpCandidate(kCalleeAddress1);
  caller->AddIceCandidate(&candidate1);

  cricket::Candidate candidate2 = CreateLocalUdpCandidate(kCalleeAddress2);
  caller->AddIceCandidate(&candidate2);

  auto candidates = caller->GetIceCandidatesFromRemoteDescription();
  ASSERT_EQ(2u, candidates.size());
  EXPECT_PRED_FORMAT2(AssertCandidatesEqual, candidate1,
                      candidates[0]->candidate());
  EXPECT_PRED_FORMAT2(AssertCandidatesEqual, candidate2,
                      candidates[1]->candidate());
}

TEST_P(PeerConnectionIceTest, LocalDescriptionUpdatedWhenContinualGathering) {
  const SocketAddress kLocalAddress("1.1.1.1", 0);

  RTCConfiguration config;
  config.continual_gathering_policy =
      PeerConnectionInterface::GATHER_CONTINUALLY;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(kLocalAddress);

  // Start ICE candidate gathering by setting the local offer.
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  // Since we're using continual gathering, we won't get "gathering done".
  EXPECT_TRUE_WAIT(
      caller->pc()->local_description()->candidates(0)->count() > 0,
      kIceCandidatesTimeout);
}

// Test that when continual gathering is enabled, and a network interface goes
// down, the candidate is signaled as removed and removed from the local
// description.
TEST_P(PeerConnectionIceTest,
       LocalCandidatesRemovedWhenNetworkDownIfGatheringContinually) {
  const SocketAddress kLocalAddress("1.1.1.1", 0);

  RTCConfiguration config;
  config.continual_gathering_policy =
      PeerConnectionInterface::GATHER_CONTINUALLY;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(kLocalAddress);

  // Start ICE candidate gathering by setting the local offer.
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_TRUE_WAIT(
      caller->pc()->local_description()->candidates(0)->count() > 0,
      kIceCandidatesTimeout);

  // Remove the only network interface, causing the PeerConnection to signal
  // the removal of all candidates derived from this interface.
  caller->network()->RemoveInterface(kLocalAddress);

  EXPECT_EQ_WAIT(0u, caller->pc()->local_description()->candidates(0)->count(),
                 kIceCandidatesTimeout);
  EXPECT_LT(0, caller->observer()->num_candidates_removed_);
}

TEST_P(PeerConnectionIceTest,
       LocalCandidatesNotRemovedWhenNetworkDownIfGatheringOnce) {
  const SocketAddress kLocalAddress("1.1.1.1", 0);

  RTCConfiguration config;
  config.continual_gathering_policy = PeerConnectionInterface::GATHER_ONCE;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(kLocalAddress);

  // Start ICE candidate gathering by setting the local offer.
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kIceCandidatesTimeout);

  caller->network()->RemoveInterface(kLocalAddress);

  // Verify that the local candidates are not removed;
  rtc::Thread::Current()->ProcessMessages(1000);
  EXPECT_EQ(0, caller->observer()->num_candidates_removed_);
}

// The following group tests that when an offer includes a new ufrag or pwd
// (indicating an ICE restart) the old candidates are removed and new candidates
// added to the remote description.

TEST_P(PeerConnectionIceTest, IceRestartOfferClearsExistingCandidate) {
  const SocketAddress kCallerAddress("1.1.1.1", 1111);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  cricket::Candidate candidate = CreateLocalUdpCandidate(kCallerAddress);
  AddCandidateToFirstTransport(&candidate, offer.get());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  RTCOfferAnswerOptions options;
  options.ice_restart = true;
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer(options)));

  EXPECT_EQ(0u, callee->GetIceCandidatesFromRemoteDescription().size());
}
TEST_P(PeerConnectionIceTest,
       IceRestartOfferCandidateReplacesExistingCandidate) {
  const SocketAddress kFirstCallerAddress("1.1.1.1", 1111);
  const SocketAddress kRestartedCallerAddress("2.2.2.2", 2222);

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  cricket::Candidate old_candidate =
      CreateLocalUdpCandidate(kFirstCallerAddress);
  AddCandidateToFirstTransport(&old_candidate, offer.get());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  RTCOfferAnswerOptions options;
  options.ice_restart = true;
  auto restart_offer = caller->CreateOffer(options);
  cricket::Candidate new_candidate =
      CreateLocalUdpCandidate(kRestartedCallerAddress);
  AddCandidateToFirstTransport(&new_candidate, restart_offer.get());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(restart_offer)));

  auto remote_candidates = callee->GetIceCandidatesFromRemoteDescription();
  ASSERT_EQ(1u, remote_candidates.size());
  EXPECT_PRED_FORMAT2(AssertCandidatesEqual, new_candidate,
                      remote_candidates[0]->candidate());
}

// Test that if there is not an ICE restart (i.e., nothing changes), then the
// answer to a later offer should have the same ufrag/pwd as the first answer.
TEST_P(PeerConnectionIceTest, LaterAnswerHasSameIceCredentialsIfNoIceRestart) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // Re-offer.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  auto* answer_transport_desc = GetFirstTransportDescription(answer.get());
  auto* local_transport_desc =
      GetFirstTransportDescription(callee->pc()->local_description());

  EXPECT_EQ(answer_transport_desc->ice_ufrag, local_transport_desc->ice_ufrag);
  EXPECT_EQ(answer_transport_desc->ice_pwd, local_transport_desc->ice_pwd);
}

// The following parameterized test verifies that if an offer is sent with a
// modified ICE ufrag and/or ICE pwd, then the answer should identify that the
// other side has initiated an ICE restart and generate a new ufrag and pwd.
// RFC 5245 says: "If the offer contained a change in the a=ice-ufrag or
// a=ice-pwd attributes compared to the previous SDP from the peer, it
// indicates that ICE is restarting for this media stream."

class PeerConnectionIceUfragPwdAnswerTest
    : public PeerConnectionIceBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, std::tuple<bool, bool>>> {
 protected:
  PeerConnectionIceUfragPwdAnswerTest()
      : PeerConnectionIceBaseTest(std::get<0>(GetParam())) {
    auto param = std::get<1>(GetParam());
    offer_new_ufrag_ = std::get<0>(param);
    offer_new_pwd_ = std::get<1>(param);
  }

  bool offer_new_ufrag_;
  bool offer_new_pwd_;
};

TEST_P(PeerConnectionIceUfragPwdAnswerTest, TestIncludedInAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto offer = caller->CreateOffer();
  auto* offer_transport_desc = GetFirstTransportDescription(offer.get());
  if (offer_new_ufrag_) {
    offer_transport_desc->ice_ufrag += "_new";
  }
  if (offer_new_pwd_) {
    offer_transport_desc->ice_pwd += "_new";
  }

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  auto* answer_transport_desc = GetFirstTransportDescription(answer.get());
  auto* local_transport_desc =
      GetFirstTransportDescription(callee->pc()->local_description());

  EXPECT_NE(answer_transport_desc->ice_ufrag, local_transport_desc->ice_ufrag);
  EXPECT_NE(answer_transport_desc->ice_pwd, local_transport_desc->ice_pwd);
}

INSTANTIATE_TEST_CASE_P(
    PeerConnectionIceTest,
    PeerConnectionIceUfragPwdAnswerTest,
    Combine(Values(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
            Values(std::make_pair(true, true),      // Both changed.
                   std::make_pair(true, false),     // Only ufrag changed.
                   std::make_pair(false, true))));  // Only pwd changed.

// Test that if an ICE restart is offered on one media section, then the answer
// will only change ICE ufrag/pwd for that section and keep the other sections
// the same.
// Note that this only works if we have disabled BUNDLE, otherwise all media
// sections will share the same transport.
TEST_P(PeerConnectionIceTest,
       CreateAnswerHasNewUfragPwdForOnlyMediaSectionWhichRestarted) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  RTCOfferAnswerOptions disable_bundle_options;
  disable_bundle_options.use_rtp_mux = false;

  auto offer = caller->CreateOffer(disable_bundle_options);

  // Signal ICE restart on the first media section.
  auto* offer_transport_desc = GetFirstTransportDescription(offer.get());
  offer_transport_desc->ice_ufrag += "_new";
  offer_transport_desc->ice_pwd += "_new";

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer(disable_bundle_options);
  const auto& answer_transports = answer->description()->transport_infos();
  const auto& local_transports =
      callee->pc()->local_description()->description()->transport_infos();

  EXPECT_NE(answer_transports[0].description.ice_ufrag,
            local_transports[0].description.ice_ufrag);
  EXPECT_NE(answer_transports[0].description.ice_pwd,
            local_transports[0].description.ice_pwd);
  EXPECT_EQ(answer_transports[1].description.ice_ufrag,
            local_transports[1].description.ice_ufrag);
  EXPECT_EQ(answer_transports[1].description.ice_pwd,
            local_transports[1].description.ice_pwd);
}

// Test that when the initial offerer (caller) uses the lite implementation of
// ICE and the callee uses the full implementation, the caller takes the
// CONTROLLED role and the callee takes the CONTROLLING role. This is specified
// in RFC5245 Section 5.1.1.
TEST_P(PeerConnectionIceTest,
       OfferFromLiteIceControlledAndAnswerFromFullIceControlling) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  SetIceMode(offer.get(), cricket::IceMode::ICEMODE_LITE);
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  SetIceMode(answer.get(), cricket::IceMode::ICEMODE_FULL);
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, GetIceRole(caller));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, GetIceRole(callee));
}

// Test that when the caller and the callee both use the lite implementation of
// ICE, the initial offerer (caller) takes the CONTROLLING role and the callee
// takes the CONTROLLED role. This is specified in RFC5245 Section 5.1.1.
TEST_P(PeerConnectionIceTest,
       OfferFromLiteIceControllingAndAnswerFromLiteIceControlled) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  SetIceMode(offer.get(), cricket::IceMode::ICEMODE_LITE);
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  SetIceMode(answer.get(), cricket::IceMode::ICEMODE_LITE);
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, GetIceRole(caller));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, GetIceRole(callee));
}

INSTANTIATE_TEST_CASE_P(PeerConnectionIceTest,
                        PeerConnectionIceTest,
                        Values(SdpSemantics::kPlanB,
                               SdpSemantics::kUnifiedPlan));

class PeerConnectionIceConfigTest : public testing::Test {
 protected:
  void SetUp() override {
    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        FakeAudioCaptureModule::Create(), CreateBuiltinAudioEncoderFactory(),
        CreateBuiltinAudioDecoderFactory(), CreateBuiltinVideoEncoderFactory(),
        CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
        nullptr /* audio_processing */);
  }
  void CreatePeerConnection(const RTCConfiguration& config) {
    std::unique_ptr<cricket::FakePortAllocator> port_allocator(
        new cricket::FakePortAllocator(rtc::Thread::Current(), nullptr));
    port_allocator_ = port_allocator.get();
    rtc::scoped_refptr<PeerConnectionInterface> pc(
        pc_factory_->CreatePeerConnection(config, std::move(port_allocator),
                                          nullptr /* cert_generator */,
                                          &observer_));
    EXPECT_TRUE(pc.get());
    pc_ = std::move(pc.get());
  }

  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_ = nullptr;
  rtc::scoped_refptr<PeerConnectionInterface> pc_ = nullptr;
  cricket::FakePortAllocator* port_allocator_ = nullptr;

  MockPeerConnectionObserver observer_;
};

TEST_F(PeerConnectionIceConfigTest, SetStunCandidateKeepaliveInterval) {
  RTCConfiguration config;
  config.stun_candidate_keepalive_interval = 123;
  config.ice_candidate_pool_size = 1;
  CreatePeerConnection(config);
  ASSERT_NE(port_allocator_, nullptr);
  absl::optional<int> actual_stun_keepalive_interval =
      port_allocator_->stun_candidate_keepalive_interval();
  EXPECT_EQ(actual_stun_keepalive_interval.value_or(-1), 123);
  config.stun_candidate_keepalive_interval = 321;
  RTCError error;
  pc_->SetConfiguration(config, &error);
  actual_stun_keepalive_interval =
      port_allocator_->stun_candidate_keepalive_interval();
  EXPECT_EQ(actual_stun_keepalive_interval.value_or(-1), 321);
}

}  // namespace webrtc
