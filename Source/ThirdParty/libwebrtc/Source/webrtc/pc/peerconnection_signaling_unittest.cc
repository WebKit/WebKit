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
#include "api/peerconnectionproxy.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakertccertificategenerator.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
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

class PeerConnectionSignalingTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForSignalingTest> WrapperPtr;

  PeerConnectionSignalingTest()
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
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(config, nullptr, nullptr,
                                                observer.get());
    if (!pc) {
      return nullptr;
    }

    return rtc::MakeUnique<PeerConnectionWrapperForSignalingTest>(
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
};

TEST_F(PeerConnectionSignalingTest, SetLocalOfferTwiceWorks) {
  auto caller = CreatePeerConnection();

  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
}

TEST_F(PeerConnectionSignalingTest, SetRemoteOfferTwiceWorks) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
}

TEST_F(PeerConnectionSignalingTest, FailToSetNullLocalDescription) {
  auto caller = CreatePeerConnection();
  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(nullptr, &error));
  EXPECT_EQ("SessionDescription is NULL.", error);
}

TEST_F(PeerConnectionSignalingTest, FailToSetNullRemoteDescription) {
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
    : public PeerConnectionSignalingTest,
      public ::testing::WithParamInterface<std::tuple<SignalingState, bool>> {
 protected:
  RTCConfiguration GetConfig() {
    RTCConfiguration config;
    config.certificates.push_back(
        FakeRTCCertificateGenerator::GenerateCertificate());
    return config;
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
};

::testing::AssertionResult AssertStartsWith(const char* str_expr,
                                            const char* prefix_expr,
                                            const std::string& str,
                                            const std::string& prefix) {
  if (rtc::starts_with(str.c_str(), prefix.c_str())) {
    return ::testing::AssertionSuccess();
  } else {
    return ::testing::AssertionFailure()
           << str_expr << "\nwhich is\n\"" << str << "\"\ndoes not start with\n"
           << prefix_expr << "\nwhich is\n\"" << prefix << "\"";
  }
}

TEST_P(PeerConnectionSignalingStateTest, CreateOffer) {
  auto wrapper = CreatePeerConnectionInState(GetParam());
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
  auto wrapper = CreatePeerConnectionInState(GetParam());
  if (wrapper->signaling_state() == SignalingState::kHaveLocalPrAnswer ||
      wrapper->signaling_state() == SignalingState::kHaveRemoteOffer) {
    EXPECT_TRUE(wrapper->CreateAnswer());
  } else {
    std::string error;
    ASSERT_FALSE(wrapper->CreateAnswer(RTCOfferAnswerOptions(), &error));
    if (wrapper->signaling_state() == SignalingState::kClosed) {
      EXPECT_PRED_FORMAT2(AssertStartsWith, error,
                          "CreateAnswer called when PeerConnection is closed.");
    } else if (wrapper->signaling_state() ==
               SignalingState::kHaveRemotePrAnswer) {
      EXPECT_PRED_FORMAT2(AssertStartsWith, error,
                          "CreateAnswer called without remote offer.");
    } else {
      EXPECT_PRED_FORMAT2(
          AssertStartsWith, error,
          "CreateAnswer can't be called before SetRemoteDescription.");
    }
  }
}

TEST_P(PeerConnectionSignalingStateTest, SetLocalOffer) {
  auto wrapper = CreatePeerConnectionInState(GetParam());
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

  auto wrapper = CreatePeerConnectionInState(GetParam());
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

  auto wrapper = CreatePeerConnectionInState(GetParam());
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

  auto wrapper = CreatePeerConnectionInState(GetParam());
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

  auto wrapper = CreatePeerConnectionInState(GetParam());
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

  auto wrapper = CreatePeerConnectionInState(GetParam());
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
                        Combine(Values(SignalingState::kStable,
                                       SignalingState::kHaveLocalOffer,
                                       SignalingState::kHaveLocalPrAnswer,
                                       SignalingState::kHaveRemoteOffer,
                                       SignalingState::kHaveRemotePrAnswer),
                                Bool()));

TEST_F(PeerConnectionSignalingTest,
       CreateAnswerSucceedsIfStableAndRemoteDescriptionIsOffer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_EQ(SignalingState::kStable, callee->signaling_state());
  EXPECT_TRUE(callee->CreateAnswer());
}

TEST_F(PeerConnectionSignalingTest,
       CreateAnswerFailsIfStableButRemoteDescriptionIsAnswer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_EQ(SignalingState::kStable, caller->signaling_state());
  std::string error;
  ASSERT_FALSE(caller->CreateAnswer(RTCOfferAnswerOptions(), &error));
  EXPECT_EQ("CreateAnswer called without remote offer.", error);
}

// According to https://tools.ietf.org/html/rfc3264#section-8, the session id
// stays the same but the version must be incremented if a later, different
// session description is generated. These two tests verify that is the case for
// both offers and answers.
TEST_F(PeerConnectionSignalingTest,
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
TEST_F(PeerConnectionSignalingTest,
       SessionVersionIncrementedInSubsequentDifferentAnswer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto original_answer = callee->CreateAnswerAndSetAsLocal();
  const std::string original_id = original_answer->session_id();
  const std::string original_version = original_answer->session_version();

  // Add track to get a different answer.
  callee->AddAudioTrack("a");

  auto later_answer = callee->CreateAnswer();

  EXPECT_EQ(original_id, later_answer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(original_version),
            rtc::FromString<uint64_t>(later_answer->session_version()));
}

TEST_F(PeerConnectionSignalingTest, InitiatorFlagSetOnCallerAndNotOnCallee) {
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
TEST_F(PeerConnectionSignalingTest, CreateOffersAndShutdown) {
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

}  // namespace webrtc
