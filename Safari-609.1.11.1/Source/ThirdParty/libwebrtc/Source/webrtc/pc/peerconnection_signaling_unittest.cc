/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests that check the PeerConnection's signaling state
// machine, as well as tests that check basic, media-agnostic aspects of SDP.

#include <tuple>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peerconnectionproxy.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "absl/memory/memory.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakertccertificategenerator.h"
#include "rtc_base/gunit.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

namespace webrtc {

using SignalingState = PeerConnectionInterface::SignalingState;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

class PeerConnectionWrapperForSignalingTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  bool initial_offerer() {
    return GetInternalPeerConnection()->initial_offerer();
  }

  PeerConnection* GetInternalPeerConnection() {
    auto* pci =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc());
    return static_cast<PeerConnection*>(pci->internal());
  }
};

class PeerConnectionSignalingBaseTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForSignalingTest> WrapperPtr;

  explicit PeerConnectionSignalingBaseTest(SdpSemantics sdp_semantics)
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
    auto observer = absl::make_unique<MockPeerConnectionObserver>();
    RTCConfiguration modified_config = config;
    modified_config.sdp_semantics = sdp_semantics_;
    auto pc = pc_factory_->CreatePeerConnection(modified_config, nullptr,
                                                nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(pc.get());
    return absl::make_unique<PeerConnectionWrapperForSignalingTest>(
        pc_factory_, pc, std::move(observer));
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

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  const SdpSemantics sdp_semantics_;
};

class PeerConnectionSignalingTest
    : public PeerConnectionSignalingBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  PeerConnectionSignalingTest() : PeerConnectionSignalingBaseTest(GetParam()) {}
};

TEST_P(PeerConnectionSignalingTest, SetLocalOfferTwiceWorks) {
  auto caller = CreatePeerConnection();

  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
}

TEST_P(PeerConnectionSignalingTest, SetRemoteOfferTwiceWorks) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
}

TEST_P(PeerConnectionSignalingTest, FailToSetNullLocalDescription) {
  auto caller = CreatePeerConnection();
  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(nullptr, &error));
  EXPECT_EQ("SessionDescription is NULL.", error);
}

TEST_P(PeerConnectionSignalingTest, FailToSetNullRemoteDescription) {
  auto caller = CreatePeerConnection();
  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(nullptr, &error));
  EXPECT_EQ("SessionDescription is NULL.", error);
}

// The following parameterized test verifies that calls to various signaling
// methods on PeerConnection will succeed/fail depending on what is the
// PeerConnection's signaling state. Note that the test tries many different
// forms of SignalingState::kClosed by arriving at a valid state then calling
// |Close()|. This is intended to catch cases where the PeerConnection signaling
// method ignores the closed flag but may work/not work because of the single
// state the PeerConnection was created in before it was closed.

class PeerConnectionSignalingStateTest
    : public PeerConnectionSignalingBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, SignalingState, bool>> {
 protected:
  PeerConnectionSignalingStateTest()
      : PeerConnectionSignalingBaseTest(std::get<0>(GetParam())),
        state_under_test_(std::make_tuple(std::get<1>(GetParam()),
                                          std::get<2>(GetParam()))) {}

  RTCConfiguration GetConfig() {
    RTCConfiguration config;
    config.certificates.push_back(
        FakeRTCCertificateGenerator::GenerateCertificate());
    return config;
  }

  WrapperPtr CreatePeerConnectionUnderTest() {
    return CreatePeerConnectionInState(state_under_test_);
  }

  WrapperPtr CreatePeerConnectionInState(SignalingState state) {
    return CreatePeerConnectionInState(std::make_tuple(state, false));
  }

  WrapperPtr CreatePeerConnectionInState(
      std::tuple<SignalingState, bool> state_tuple) {
    SignalingState state = std::get<0>(state_tuple);
    bool closed = std::get<1>(state_tuple);

    auto wrapper = CreatePeerConnectionWithAudioVideo(GetConfig());
    switch (state) {
      case SignalingState::kStable: {
        break;
      }
      case SignalingState::kHaveLocalOffer: {
        wrapper->SetLocalDescription(wrapper->CreateOffer());
        break;
      }
      case SignalingState::kHaveLocalPrAnswer: {
        auto caller = CreatePeerConnectionWithAudioVideo(GetConfig());
        wrapper->SetRemoteDescription(caller->CreateOffer());
        auto answer = wrapper->CreateAnswer();
        wrapper->SetLocalDescription(
            CloneSessionDescriptionAsType(answer.get(), SdpType::kPrAnswer));
        break;
      }
      case SignalingState::kHaveRemoteOffer: {
        auto caller = CreatePeerConnectionWithAudioVideo(GetConfig());
        wrapper->SetRemoteDescription(caller->CreateOffer());
        break;
      }
      case SignalingState::kHaveRemotePrAnswer: {
        auto callee = CreatePeerConnectionWithAudioVideo(GetConfig());
        callee->SetRemoteDescription(wrapper->CreateOfferAndSetAsLocal());
        auto answer = callee->CreateAnswer();
        wrapper->SetRemoteDescription(
            CloneSessionDescriptionAsType(answer.get(), SdpType::kPrAnswer));
        break;
      }
      case SignalingState::kClosed: {
        RTC_NOTREACHED() << "Set the second member of the tuple to true to "
                            "achieve a closed state from an existing, valid "
                            "state.";
      }
    }

    RTC_DCHECK_EQ(state, wrapper->pc()->signaling_state());

    if (closed) {
      wrapper->pc()->Close();
      RTC_DCHECK_EQ(SignalingState::kClosed, wrapper->signaling_state());
    }

    return wrapper;
  }

  std::tuple<SignalingState, bool> state_under_test_;
};

TEST_P(PeerConnectionSignalingStateTest, CreateOffer) {
  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() != SignalingState::kClosed) {
    EXPECT_TRUE(wrapper->CreateOffer());
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->CreateOffer(RTCOfferAnswerOptions(), &error));
    EXPECT_PRED_FORMAT2(AssertStartsWith, error,
                        "CreateOffer called when PeerConnection is closed.");
  }
}

TEST_P(PeerConnectionSignalingStateTest, CreateAnswer) {
  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kHaveLocalPrAnswer ||
      wrapper->signaling_state() == SignalingState::kHaveRemoteOffer) {
    EXPECT_TRUE(wrapper->CreateAnswer());
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->CreateAnswer(RTCOfferAnswerOptions(), &error));
    EXPECT_EQ(error,
              "PeerConnection cannot create an answer in a state other than "
              "have-remote-offer or have-local-pranswer.");
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetLocalOffer) {
  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kStable ||
      wrapper->signaling_state() == SignalingState::kHaveLocalOffer) {
    // Need to call CreateOffer on the PeerConnection under test, otherwise when
    // setting the local offer it will want to verify the DTLS fingerprint
    // against the locally generated certificate, but without a call to
    // CreateOffer the certificate will never be generated.
    EXPECT_TRUE(wrapper->SetLocalDescription(wrapper->CreateOffer()));
  } else {
    auto wrapper_for_offer =
        CreatePeerConnectionInState(SignalingState::kHaveLocalOffer);
    auto offer =
        CloneSessionDescription(wrapper_for_offer->pc()->local_description());

    std::string error;
    ASSERT_FALSE(wrapper->SetLocalDescription(std::move(offer), &error));
    EXPECT_PRED_FORMAT2(
        AssertStartsWith, error,
        "Failed to set local offer sdp: Called in wrong state:");
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetLocalPrAnswer) {
  auto wrapper_for_pranswer =
      CreatePeerConnectionInState(SignalingState::kHaveLocalPrAnswer);
  auto pranswer =
      CloneSessionDescription(wrapper_for_pranswer->pc()->local_description());

  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kHaveLocalPrAnswer ||
      wrapper->signaling_state() == SignalingState::kHaveRemoteOffer) {
    EXPECT_TRUE(wrapper->SetLocalDescription(std::move(pranswer)));
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->SetLocalDescription(std::move(pranswer), &error));
    EXPECT_PRED_FORMAT2(
        AssertStartsWith, error,
        "Failed to set local pranswer sdp: Called in wrong state:");
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetLocalAnswer) {
  auto wrapper_for_answer =
      CreatePeerConnectionInState(SignalingState::kHaveRemoteOffer);
  auto answer = wrapper_for_answer->CreateAnswer();

  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kHaveLocalPrAnswer ||
      wrapper->signaling_state() == SignalingState::kHaveRemoteOffer) {
    EXPECT_TRUE(wrapper->SetLocalDescription(std::move(answer)));
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->SetLocalDescription(std::move(answer), &error));
    EXPECT_PRED_FORMAT2(
        AssertStartsWith, error,
        "Failed to set local answer sdp: Called in wrong state:");
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetRemoteOffer) {
  auto wrapper_for_offer =
      CreatePeerConnectionInState(SignalingState::kHaveRemoteOffer);
  auto offer =
      CloneSessionDescription(wrapper_for_offer->pc()->remote_description());

  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kStable ||
      wrapper->signaling_state() == SignalingState::kHaveRemoteOffer) {
    EXPECT_TRUE(wrapper->SetRemoteDescription(std::move(offer)));
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->SetRemoteDescription(std::move(offer), &error));
    EXPECT_PRED_FORMAT2(
        AssertStartsWith, error,
        "Failed to set remote offer sdp: Called in wrong state:");
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetRemotePrAnswer) {
  auto wrapper_for_pranswer =
      CreatePeerConnectionInState(SignalingState::kHaveRemotePrAnswer);
  auto pranswer =
      CloneSessionDescription(wrapper_for_pranswer->pc()->remote_description());

  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kHaveLocalOffer ||
      wrapper->signaling_state() == SignalingState::kHaveRemotePrAnswer) {
    EXPECT_TRUE(wrapper->SetRemoteDescription(std::move(pranswer)));
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->SetRemoteDescription(std::move(pranswer), &error));
    EXPECT_PRED_FORMAT2(
        AssertStartsWith, error,
        "Failed to set remote pranswer sdp: Called in wrong state:");
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetRemoteAnswer) {
  auto wrapper_for_answer =
      CreatePeerConnectionInState(SignalingState::kHaveRemoteOffer);
  auto answer = wrapper_for_answer->CreateAnswer();

  auto wrapper = CreatePeerConnectionUnderTest();
  if (wrapper->signaling_state() == SignalingState::kHaveLocalOffer ||
      wrapper->signaling_state() == SignalingState::kHaveRemotePrAnswer) {
    EXPECT_TRUE(wrapper->SetRemoteDescription(std::move(answer)));
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->SetRemoteDescription(std::move(answer), &error));
    EXPECT_PRED_FORMAT2(
        AssertStartsWith, error,
        "Failed to set remote answer sdp: Called in wrong state:");
  }
}

INSTANTIATE_TEST_CASE_P(PeerConnectionSignalingTest,
                        PeerConnectionSignalingStateTest,
                        Combine(Values(SdpSemantics::kPlanB,
                                       SdpSemantics::kUnifiedPlan),
                                Values(SignalingState::kStable,
                                       SignalingState::kHaveLocalOffer,
                                       SignalingState::kHaveLocalPrAnswer,
                                       SignalingState::kHaveRemoteOffer,
                                       SignalingState::kHaveRemotePrAnswer),
                                Bool()));

// Test that CreateAnswer fails if a round of offer/answer has been done and
// the PeerConnection is in the stable state.
TEST_P(PeerConnectionSignalingTest, CreateAnswerFailsIfStable) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_EQ(SignalingState::kStable, caller->signaling_state());
  EXPECT_FALSE(caller->CreateAnswer());

  ASSERT_EQ(SignalingState::kStable, callee->signaling_state());
  EXPECT_FALSE(callee->CreateAnswer());
}

// According to https://tools.ietf.org/html/rfc3264#section-8, the session id
// stays the same but the version must be incremented if a later, different
// session description is generated. These two tests verify that is the case for
// both offers and answers.
TEST_P(PeerConnectionSignalingTest,
       SessionVersionIncrementedInSubsequentDifferentOffer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto original_offer = caller->CreateOfferAndSetAsLocal();
  const std::string original_id = original_offer->session_id();
  const std::string original_version = original_offer->session_version();

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(original_offer)));
  ASSERT_TRUE(caller->SetRemoteDescription(callee->CreateAnswer()));

  // Add track to get a different offer.
  caller->AddAudioTrack("a");

  auto later_offer = caller->CreateOffer();

  EXPECT_EQ(original_id, later_offer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(original_version),
            rtc::FromString<uint64_t>(later_offer->session_version()));
}
TEST_P(PeerConnectionSignalingTest,
       SessionVersionIncrementedInSubsequentDifferentAnswer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto original_answer = callee->CreateAnswer();
  const std::string original_id = original_answer->session_id();
  const std::string original_version = original_answer->session_version();

  // Add track to get a different answer.
  callee->AddAudioTrack("a");

  auto later_answer = callee->CreateAnswer();

  EXPECT_EQ(original_id, later_answer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(original_version),
            rtc::FromString<uint64_t>(later_answer->session_version()));
}

TEST_P(PeerConnectionSignalingTest, InitiatorFlagSetOnCallerAndNotOnCallee) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  EXPECT_FALSE(caller->initial_offerer());
  EXPECT_FALSE(callee->initial_offerer());

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_TRUE(caller->initial_offerer());
  EXPECT_FALSE(callee->initial_offerer());

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_TRUE(caller->initial_offerer());
  EXPECT_FALSE(callee->initial_offerer());
}

// Test creating a PeerConnection, request multiple offers, destroy the
// PeerConnection and make sure we get success/failure callbacks for all of the
// requests.
// Background: crbug.com/507307
TEST_P(PeerConnectionSignalingTest, CreateOffersAndShutdown) {
  auto caller = CreatePeerConnection();

  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;

  rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observers[100];
  for (auto& observer : observers) {
    observer =
        new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>();
    caller->pc()->CreateOffer(observer, options);
  }

  // Destroy the PeerConnection.
  caller.reset(nullptr);

  for (auto& observer : observers) {
    // We expect to have received a notification now even if the PeerConnection
    // was terminated. The offer creation may or may not have succeeded, but we
    // must have received a notification.
    EXPECT_TRUE(observer->called());
  }
}

INSTANTIATE_TEST_CASE_P(PeerConnectionSignalingTest,
                        PeerConnectionSignalingTest,
                        Values(SdpSemantics::kPlanB,
                               SdpSemantics::kUnifiedPlan));

}  // namespace webrtc
