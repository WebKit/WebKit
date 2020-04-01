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

#include <memory>
#include <tuple>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/peer_connection_proxy.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/sdp_utils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/android_test_initializer.h"
#endif
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"

namespace webrtc {

using SignalingState = PeerConnectionInterface::SignalingState;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

namespace {
const int64_t kWaitTimeout = 10000;
}  // namespace

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

class ExecuteFunctionOnCreateSessionDescriptionObserver
    : public CreateSessionDescriptionObserver {
 public:
  ExecuteFunctionOnCreateSessionDescriptionObserver(
      std::function<void(SessionDescriptionInterface*)> function)
      : function_(std::move(function)) {}
  ~ExecuteFunctionOnCreateSessionDescriptionObserver() override {
    RTC_DCHECK(was_called_);
  }

  bool was_called() const { return was_called_; }

  void OnSuccess(SessionDescriptionInterface* desc) override {
    RTC_DCHECK(!was_called_);
    was_called_ = true;
    function_(desc);
  }

  void OnFailure(RTCError error) override { RTC_NOTREACHED(); }

 private:
  bool was_called_ = false;
  std::function<void(SessionDescriptionInterface*)> function_;
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
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    RTCConfiguration modified_config = config;
    modified_config.sdp_semantics = sdp_semantics_;
    auto pc = pc_factory_->CreatePeerConnection(modified_config, nullptr,
                                                nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(pc.get());
    return std::make_unique<PeerConnectionWrapperForSignalingTest>(
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

  int NumberOfDtlsTransports(const WrapperPtr& pc_wrapper) {
    std::set<DtlsTransportInterface*> transports;
    auto transceivers = pc_wrapper->pc()->GetTransceivers();

    for (auto& transceiver : transceivers) {
      if (transceiver->sender()->dtls_transport()) {
        EXPECT_TRUE(transceiver->receiver()->dtls_transport());
        EXPECT_EQ(transceiver->sender()->dtls_transport().get(),
                  transceiver->receiver()->dtls_transport().get());
        transports.insert(transceiver->sender()->dtls_transport().get());
      } else {
        // If one transceiver is missing, they all should be.
        EXPECT_EQ(0UL, transports.size());
      }
    }
    return transports.size();
  }

  bool HasDtlsTransport(const WrapperPtr& pc_wrapper) {
    return NumberOfDtlsTransports(pc_wrapper) > 0;
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

INSTANTIATE_TEST_SUITE_P(PeerConnectionSignalingTest,
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

// Similar to the above test, but by closing the PC first the CreateOffer() will
// fail "early", which triggers a codepath where the PeerConnection is
// reponsible for invoking the observer, instead of the normal codepath where
// the WebRtcSessionDescriptionFactory is responsible for it.
TEST_P(PeerConnectionSignalingTest, CloseCreateOfferAndShutdown) {
  auto caller = CreatePeerConnection();
  rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer =
      new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>();
  caller->pc()->Close();
  caller->pc()->CreateOffer(observer, RTCOfferAnswerOptions());
  caller.reset(nullptr);
  EXPECT_TRUE(observer->called());
}

TEST_P(PeerConnectionSignalingTest, ImplicitCreateOfferAndShutdown) {
  auto caller = CreatePeerConnection();
  auto observer = MockSetSessionDescriptionObserver::Create();
  caller->pc()->SetLocalDescription(observer);
  caller.reset(nullptr);
  EXPECT_FALSE(observer->called());
}

TEST_P(PeerConnectionSignalingTest, CloseBeforeImplicitCreateOfferAndShutdown) {
  auto caller = CreatePeerConnection();
  auto observer = MockSetSessionDescriptionObserver::Create();
  caller->pc()->Close();
  caller->pc()->SetLocalDescription(observer);
  caller.reset(nullptr);
  EXPECT_FALSE(observer->called());
}

TEST_P(PeerConnectionSignalingTest, CloseAfterImplicitCreateOfferAndShutdown) {
  auto caller = CreatePeerConnection();
  auto observer = MockSetSessionDescriptionObserver::Create();
  caller->pc()->SetLocalDescription(observer);
  caller->pc()->Close();
  caller.reset(nullptr);
  EXPECT_FALSE(observer->called());
}

TEST_P(PeerConnectionSignalingTest, SetRemoteDescriptionExecutesImmediately) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  // This offer will cause receivers to be created.
  auto offer = caller->CreateOffer(RTCOfferAnswerOptions());

  // By not waiting for the observer's callback we can verify that the operation
  // executed immediately.
  callee->pc()->SetRemoteDescription(std::move(offer),
                                     new MockSetRemoteDescriptionObserver());
  EXPECT_EQ(2u, callee->pc()->GetReceivers().size());
}

TEST_P(PeerConnectionSignalingTest, CreateOfferBlocksSetRemoteDescription) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  // This offer will cause receivers to be created.
  auto offer = caller->CreateOffer(RTCOfferAnswerOptions());

  EXPECT_EQ(0u, callee->pc()->GetReceivers().size());
  rtc::scoped_refptr<MockCreateSessionDescriptionObserver> offer_observer(
      new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
  // Synchronously invoke CreateOffer() and SetRemoteDescription(). The
  // SetRemoteDescription() operation should be chained to be executed
  // asynchronously, when CreateOffer() completes.
  callee->pc()->CreateOffer(offer_observer, RTCOfferAnswerOptions());
  callee->pc()->SetRemoteDescription(std::move(offer),
                                     new MockSetRemoteDescriptionObserver());
  // CreateOffer() is asynchronous; without message processing this operation
  // should not have completed.
  EXPECT_FALSE(offer_observer->called());
  // Due to chaining, the receivers should not have been created by the offer
  // yet.
  EXPECT_EQ(0u, callee->pc()->GetReceivers().size());
  // EXPECT_TRUE_WAIT causes messages to be processed...
  EXPECT_TRUE_WAIT(offer_observer->called(), kWaitTimeout);
  // Now that the offer has been completed, SetRemoteDescription() will have
  // been executed next in the chain.
  EXPECT_EQ(2u, callee->pc()->GetReceivers().size());
}

TEST_P(PeerConnectionSignalingTest,
       ParameterlessSetLocalDescriptionCreatesOffer) {
  auto caller = CreatePeerConnectionWithAudioVideo();

  auto observer = MockSetSessionDescriptionObserver::Create();
  caller->pc()->SetLocalDescription(observer);

  // The offer is created asynchronously; message processing is needed for it to
  // complete.
  EXPECT_FALSE(observer->called());
  EXPECT_FALSE(caller->pc()->pending_local_description());
  EXPECT_EQ(PeerConnection::kStable, caller->signaling_state());

  // Wait for messages to be processed.
  EXPECT_TRUE_WAIT(observer->called(), kWaitTimeout);
  EXPECT_TRUE(observer->result());
  EXPECT_TRUE(caller->pc()->pending_local_description());
  EXPECT_EQ(SdpType::kOffer,
            caller->pc()->pending_local_description()->GetType());
  EXPECT_EQ(PeerConnection::kHaveLocalOffer, caller->signaling_state());
}

TEST_P(PeerConnectionSignalingTest,
       ParameterlessSetLocalDescriptionCreatesAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  callee->SetRemoteDescription(caller->CreateOffer());
  EXPECT_EQ(PeerConnection::kHaveRemoteOffer, callee->signaling_state());

  auto observer = MockSetSessionDescriptionObserver::Create();
  callee->pc()->SetLocalDescription(observer);

  // The answer is created asynchronously; message processing is needed for it
  // to complete.
  EXPECT_FALSE(observer->called());
  EXPECT_FALSE(callee->pc()->current_local_description());

  // Wait for messages to be processed.
  EXPECT_TRUE_WAIT(observer->called(), kWaitTimeout);
  EXPECT_TRUE(observer->result());
  EXPECT_TRUE(callee->pc()->current_local_description());
  EXPECT_EQ(SdpType::kAnswer,
            callee->pc()->current_local_description()->GetType());
  EXPECT_EQ(PeerConnection::kStable, callee->signaling_state());
}

TEST_P(PeerConnectionSignalingTest,
       ParameterlessSetLocalDescriptionFullExchange) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  // SetLocalDescription(), implicitly creating an offer.
  rtc::scoped_refptr<MockSetSessionDescriptionObserver>
      caller_set_local_description_observer(
          new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  caller->pc()->SetLocalDescription(caller_set_local_description_observer);
  EXPECT_TRUE_WAIT(caller_set_local_description_observer->called(),
                   kWaitTimeout);
  ASSERT_TRUE(caller->pc()->pending_local_description());

  // SetRemoteDescription(offer)
  rtc::scoped_refptr<MockSetSessionDescriptionObserver>
      callee_set_remote_description_observer(
          new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  callee->pc()->SetRemoteDescription(
      callee_set_remote_description_observer.get(),
      CloneSessionDescription(caller->pc()->pending_local_description())
          .release());

  // SetLocalDescription(), implicitly creating an answer.
  rtc::scoped_refptr<MockSetSessionDescriptionObserver>
      callee_set_local_description_observer(
          new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  callee->pc()->SetLocalDescription(callee_set_local_description_observer);
  EXPECT_TRUE_WAIT(callee_set_local_description_observer->called(),
                   kWaitTimeout);
  // Chaining guarantees SetRemoteDescription() happened before
  // SetLocalDescription().
  EXPECT_TRUE(callee_set_remote_description_observer->called());
  EXPECT_TRUE(callee->pc()->current_local_description());

  // SetRemoteDescription(answer)
  rtc::scoped_refptr<MockSetSessionDescriptionObserver>
      caller_set_remote_description_observer(
          new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  caller->pc()->SetRemoteDescription(
      caller_set_remote_description_observer,
      CloneSessionDescription(callee->pc()->current_local_description())
          .release());
  EXPECT_TRUE_WAIT(caller_set_remote_description_observer->called(),
                   kWaitTimeout);

  EXPECT_EQ(PeerConnection::kStable, caller->signaling_state());
  EXPECT_EQ(PeerConnection::kStable, callee->signaling_state());
}

TEST_P(PeerConnectionSignalingTest,
       ParameterlessSetLocalDescriptionCloseBeforeCreatingOffer) {
  auto caller = CreatePeerConnectionWithAudioVideo();

  auto observer = MockSetSessionDescriptionObserver::Create();
  caller->pc()->Close();
  caller->pc()->SetLocalDescription(observer);

  // The operation should fail asynchronously.
  EXPECT_FALSE(observer->called());
  EXPECT_TRUE_WAIT(observer->called(), kWaitTimeout);
  EXPECT_FALSE(observer->result());
  // This did not affect the signaling state.
  EXPECT_EQ(PeerConnection::kClosed, caller->pc()->signaling_state());
  EXPECT_EQ(
      "SetLocalDescription failed to create session description - "
      "SetLocalDescription called when PeerConnection is closed.",
      observer->error());
}

TEST_P(PeerConnectionSignalingTest,
       ParameterlessSetLocalDescriptionCloseWhileCreatingOffer) {
  auto caller = CreatePeerConnectionWithAudioVideo();

  auto observer = MockSetSessionDescriptionObserver::Create();
  caller->pc()->SetLocalDescription(observer);
  caller->pc()->Close();

  // The operation should fail asynchronously.
  EXPECT_FALSE(observer->called());
  EXPECT_TRUE_WAIT(observer->called(), kWaitTimeout);
  EXPECT_FALSE(observer->result());
  // This did not affect the signaling state.
  EXPECT_EQ(PeerConnection::kClosed, caller->pc()->signaling_state());
  EXPECT_EQ(
      "SetLocalDescription failed to create session description - "
      "CreateOffer failed because the session was shut down",
      observer->error());
}

INSTANTIATE_TEST_SUITE_P(PeerConnectionSignalingTest,
                         PeerConnectionSignalingTest,
                         Values(SdpSemantics::kPlanB,
                                SdpSemantics::kUnifiedPlan));

class PeerConnectionSignalingUnifiedPlanTest
    : public PeerConnectionSignalingBaseTest {
 protected:
  PeerConnectionSignalingUnifiedPlanTest()
      : PeerConnectionSignalingBaseTest(SdpSemantics::kUnifiedPlan) {}
};

// We verify that SetLocalDescription() executed immediately by verifying that
// the transceiver mid values got assigned. SLD executing immeditately is not
// unique to Unified Plan, but the transceivers used to verify this are only
// available in Unified Plan.
TEST_F(PeerConnectionSignalingUnifiedPlanTest,
       SetLocalDescriptionExecutesImmediately) {
  auto caller = CreatePeerConnectionWithAudioVideo();

  // This offer will cause transceiver mids to get assigned.
  auto offer = caller->CreateOffer(RTCOfferAnswerOptions());

  // By not waiting for the observer's callback we can verify that the operation
  // executed immediately.
  RTC_DCHECK(!caller->pc()->GetTransceivers()[0]->mid().has_value());
  caller->pc()->SetLocalDescription(
      new rtc::RefCountedObject<MockSetSessionDescriptionObserver>(),
      offer.release());
  EXPECT_TRUE(caller->pc()->GetTransceivers()[0]->mid().has_value());
}

TEST_F(PeerConnectionSignalingUnifiedPlanTest,
       SetLocalDescriptionExecutesImmediatelyInsideCreateOfferCallback) {
  auto caller = CreatePeerConnectionWithAudioVideo();

  // This offer will cause transceiver mids to get assigned.
  auto offer = caller->CreateOffer(RTCOfferAnswerOptions());

  rtc::scoped_refptr<ExecuteFunctionOnCreateSessionDescriptionObserver>
      offer_observer(new rtc::RefCountedObject<
                     ExecuteFunctionOnCreateSessionDescriptionObserver>(
          [pc = caller->pc()](SessionDescriptionInterface* desc) {
            // By not waiting for the observer's callback we can verify that the
            // operation executed immediately.
            RTC_DCHECK(!pc->GetTransceivers()[0]->mid().has_value());
            pc->SetLocalDescription(
                new rtc::RefCountedObject<MockSetSessionDescriptionObserver>(),
                desc);
            EXPECT_TRUE(pc->GetTransceivers()[0]->mid().has_value());
          }));
  caller->pc()->CreateOffer(offer_observer, RTCOfferAnswerOptions());
  EXPECT_TRUE_WAIT(offer_observer->was_called(), kWaitTimeout);
}

// Test that transports are shown in the sender/receiver API after offer/answer.
// This only works in Unified Plan.
TEST_F(PeerConnectionSignalingUnifiedPlanTest,
       DtlsTransportsInstantiateInOfferAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  EXPECT_FALSE(HasDtlsTransport(caller));
  EXPECT_FALSE(HasDtlsTransport(callee));
  auto offer = caller->CreateOffer(RTCOfferAnswerOptions());
  caller->SetLocalDescription(CloneSessionDescription(offer.get()));
  EXPECT_TRUE(HasDtlsTransport(caller));
  callee->SetRemoteDescription(std::move(offer));
  EXPECT_FALSE(HasDtlsTransport(callee));
  auto answer = callee->CreateAnswer(RTCOfferAnswerOptions());
  callee->SetLocalDescription(CloneSessionDescription(answer.get()));
  EXPECT_TRUE(HasDtlsTransport(callee));
  caller->SetRemoteDescription(std::move(answer));
  EXPECT_TRUE(HasDtlsTransport(caller));

  ASSERT_EQ(SignalingState::kStable, caller->signaling_state());
}

TEST_F(PeerConnectionSignalingUnifiedPlanTest, DtlsTransportsMergeWhenBundled) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  EXPECT_FALSE(HasDtlsTransport(caller));
  EXPECT_FALSE(HasDtlsTransport(callee));
  auto offer = caller->CreateOffer(RTCOfferAnswerOptions());
  caller->SetLocalDescription(CloneSessionDescription(offer.get()));
  EXPECT_EQ(2, NumberOfDtlsTransports(caller));
  callee->SetRemoteDescription(std::move(offer));
  auto answer = callee->CreateAnswer(RTCOfferAnswerOptions());
  callee->SetLocalDescription(CloneSessionDescription(answer.get()));
  caller->SetRemoteDescription(std::move(answer));
  EXPECT_EQ(1, NumberOfDtlsTransports(caller));

  ASSERT_EQ(SignalingState::kStable, caller->signaling_state());
}

TEST_F(PeerConnectionSignalingUnifiedPlanTest,
       DtlsTransportsAreSeparateeWhenUnbundled) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  EXPECT_FALSE(HasDtlsTransport(caller));
  EXPECT_FALSE(HasDtlsTransport(callee));
  RTCOfferAnswerOptions unbundle_options;
  unbundle_options.use_rtp_mux = false;
  auto offer = caller->CreateOffer(unbundle_options);
  caller->SetLocalDescription(CloneSessionDescription(offer.get()));
  EXPECT_EQ(2, NumberOfDtlsTransports(caller));
  callee->SetRemoteDescription(std::move(offer));
  auto answer = callee->CreateAnswer(RTCOfferAnswerOptions());
  callee->SetLocalDescription(CloneSessionDescription(answer.get()));
  EXPECT_EQ(2, NumberOfDtlsTransports(callee));
  caller->SetRemoteDescription(std::move(answer));
  EXPECT_EQ(2, NumberOfDtlsTransports(caller));

  ASSERT_EQ(SignalingState::kStable, caller->signaling_state());
}

}  // namespace webrtc
