/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests that check the interaction between the
// PeerConnection and the underlying media engine, as well as tests that check
// the media-related aspects of SDP.

#include <tuple>

#include "api/call/callfactoryinterface.h"
#include "api/test/fake_media_transport.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "media/base/fakemediaengine.h"
#include "p2p/base/fakeportallocator.h"
#include "pc/mediasession.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/rtpmediautils.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "absl/memory/memory.h"
#include "pc/test/fakertccertificategenerator.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

namespace webrtc {

using cricket::FakeMediaEngine;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using ::testing::ElementsAre;

class PeerConnectionWrapperForMediaTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  FakeMediaEngine* media_engine() { return media_engine_; }
  void set_media_engine(FakeMediaEngine* media_engine) {
    media_engine_ = media_engine;
  }

 private:
  FakeMediaEngine* media_engine_;
};

class PeerConnectionMediaBaseTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForMediaTest> WrapperPtr;

  explicit PeerConnectionMediaBaseTest(SdpSemantics sdp_semantics)
      : vss_(new rtc::VirtualSocketServer()),
        main_(vss_.get()),
        sdp_semantics_(sdp_semantics) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  // Creates PeerConnectionFactory and PeerConnection for given configuration.
  // Note that PeerConnectionFactory is created with MediaTransportFactory,
  // because some tests pass config.use_media_transport = true.
  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto media_engine = absl::make_unique<FakeMediaEngine>();
    auto* media_engine_ptr = media_engine.get();

    PeerConnectionFactoryDependencies factory_dependencies;

    factory_dependencies.network_thread = rtc::Thread::Current();
    factory_dependencies.worker_thread = rtc::Thread::Current();
    factory_dependencies.signaling_thread = rtc::Thread::Current();
    factory_dependencies.media_engine = std::move(media_engine);
    factory_dependencies.call_factory = CreateCallFactory();
    factory_dependencies.event_log_factory = CreateRtcEventLogFactory();
    factory_dependencies.media_transport_factory =
        absl::make_unique<FakeMediaTransportFactory>();

    auto pc_factory =
        CreateModularPeerConnectionFactory(std::move(factory_dependencies));

    auto fake_port_allocator = absl::make_unique<cricket::FakePortAllocator>(
        rtc::Thread::Current(), nullptr);
    auto observer = absl::make_unique<MockPeerConnectionObserver>();
    auto modified_config = config;
    modified_config.sdp_semantics = sdp_semantics_;
    auto pc = pc_factory->CreatePeerConnection(modified_config,
                                               std::move(fake_port_allocator),
                                               nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(pc.get());
    auto wrapper = absl::make_unique<PeerConnectionWrapperForMediaTest>(
        pc_factory, pc, std::move(observer));
    wrapper->set_media_engine(media_engine_ptr);
    return wrapper;
  }

  // Accepts the same arguments as CreatePeerConnection and adds default audio
  // track (but no video).
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithAudio(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->AddAudioTrack("a");
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

  RtpTransceiverDirection GetMediaContentDirection(
      const SessionDescriptionInterface* sdesc,
      cricket::MediaType media_type) {
    auto* content =
        cricket::GetFirstMediaContent(sdesc->description(), media_type);
    RTC_DCHECK(content);
    return content->media_description()->direction();
  }

  bool IsUnifiedPlan() const {
    return sdp_semantics_ == SdpSemantics::kUnifiedPlan;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  const SdpSemantics sdp_semantics_;
};

class PeerConnectionMediaTest
    : public PeerConnectionMediaBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  PeerConnectionMediaTest() : PeerConnectionMediaBaseTest(GetParam()) {}
};

class PeerConnectionMediaTestUnifiedPlan : public PeerConnectionMediaBaseTest {
 protected:
  PeerConnectionMediaTestUnifiedPlan()
      : PeerConnectionMediaBaseTest(SdpSemantics::kUnifiedPlan) {}
};

class PeerConnectionMediaTestPlanB : public PeerConnectionMediaBaseTest {
 protected:
  PeerConnectionMediaTestPlanB()
      : PeerConnectionMediaBaseTest(SdpSemantics::kPlanB) {}
};

TEST_P(PeerConnectionMediaTest,
       FailToSetRemoteDescriptionIfCreateMediaChannelFails) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();
  callee->media_engine()->set_fail_create_channel(true);

  std::string error;
  ASSERT_FALSE(callee->SetRemoteDescription(caller->CreateOffer(), &error));
  EXPECT_PRED_FORMAT2(AssertStartsWith, error,
                      "Failed to set remote offer sdp: Failed to create");
}

TEST_P(PeerConnectionMediaTest,
       FailToSetLocalDescriptionIfCreateMediaChannelFails) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->media_engine()->set_fail_create_channel(true);

  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(caller->CreateOffer(), &error));
  EXPECT_PRED_FORMAT2(AssertStartsWith, error,
                      "Failed to set local offer sdp: Failed to create");
}

std::vector<std::string> GetIds(
    const std::vector<cricket::StreamParams>& streams) {
  std::vector<std::string> ids;
  for (const auto& stream : streams) {
    ids.push_back(stream.id);
  }
  return ids;
}

// Test that exchanging an offer and answer with each side having an audio and
// video stream creates the appropriate send/recv streams in the underlying
// media engine on both sides.
TEST_P(PeerConnectionMediaTest, AudioVideoOfferAnswerCreateSendRecvStreams) {
  const std::string kCallerAudioId = "caller_a";
  const std::string kCallerVideoId = "caller_v";
  const std::string kCalleeAudioId = "callee_a";
  const std::string kCalleeVideoId = "callee_v";

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack(kCallerAudioId);
  caller->AddVideoTrack(kCallerVideoId);

  auto callee = CreatePeerConnection();
  callee->AddAudioTrack(kCalleeAudioId);
  callee->AddVideoTrack(kCalleeVideoId);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto* caller_voice = caller->media_engine()->GetVoiceChannel(0);
  EXPECT_THAT(GetIds(caller_voice->recv_streams()),
              ElementsAre(kCalleeAudioId));
  EXPECT_THAT(GetIds(caller_voice->send_streams()),
              ElementsAre(kCallerAudioId));

  auto* caller_video = caller->media_engine()->GetVideoChannel(0);
  EXPECT_THAT(GetIds(caller_video->recv_streams()),
              ElementsAre(kCalleeVideoId));
  EXPECT_THAT(GetIds(caller_video->send_streams()),
              ElementsAre(kCallerVideoId));

  auto* callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_THAT(GetIds(callee_voice->recv_streams()),
              ElementsAre(kCallerAudioId));
  EXPECT_THAT(GetIds(callee_voice->send_streams()),
              ElementsAre(kCalleeAudioId));

  auto* callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_THAT(GetIds(callee_video->recv_streams()),
              ElementsAre(kCallerVideoId));
  EXPECT_THAT(GetIds(callee_video->send_streams()),
              ElementsAre(kCalleeVideoId));
}

// Test that stopping the caller transceivers causes the media channels on the
// callee to be destroyed after calling SetRemoteDescription on the generated
// offer.
// See next test for equivalent behavior with Plan B semantics.
TEST_F(PeerConnectionMediaTestUnifiedPlan,
       StoppedRemoteTransceiversRemovesMediaChannels) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Stop both audio and video transceivers on the caller.
  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  transceivers[0]->Stop();
  transceivers[1]->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_FALSE(callee->media_engine()->GetVoiceChannel(0));
  ASSERT_FALSE(callee->media_engine()->GetVideoChannel(0));
}

// Test that removing streams from a subsequent offer causes the receive streams
// on the callee to be removed.
// See previous test for equivalent behavior with Unified Plan semantics.
TEST_F(PeerConnectionMediaTestPlanB, EmptyRemoteOfferRemovesRecvStreams) {
  auto caller = CreatePeerConnection();
  auto caller_audio_track = caller->AddAudioTrack("a");
  auto caller_video_track = caller->AddVideoTrack("v");
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Remove both tracks from caller.
  caller->pc()->RemoveTrack(caller_audio_track);
  caller->pc()->RemoveTrack(caller_video_track);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  EXPECT_EQ(0u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());
  EXPECT_EQ(0u, callee_video->recv_streams().size());
}

// Test enabling of simulcast with Plan B semantics.
// This test creating an offer.
TEST_F(PeerConnectionMediaTestPlanB, SimulcastOffer) {
  auto caller = CreatePeerConnection();
  auto caller_video_track = caller->AddVideoTrack("v");
  RTCOfferAnswerOptions options;
  options.num_simulcast_layers = 3;
  auto offer = caller->CreateOffer(options);
  auto* description = cricket::GetFirstMediaContent(
      offer->description(),
      cricket::MEDIA_TYPE_VIDEO)->media_description();
  ASSERT_EQ(1u, description->streams().size());
  ASSERT_TRUE(description->streams()[0].get_ssrc_group("SIM"));
  EXPECT_EQ(3u, description->streams()[0].get_ssrc_group("SIM")->ssrcs.size());

  // Check that it actually creates simulcast aswell.
  caller->SetLocalDescription(std::move(offer));
  auto senders = caller->pc()->GetSenders();
  ASSERT_EQ(1u, senders.size());
  EXPECT_EQ(cricket::MediaType::MEDIA_TYPE_VIDEO, senders[0]->media_type());
  EXPECT_EQ(3u, senders[0]->GetParameters().encodings.size());
}

// Test enabling of simulcast with Plan B semantics.
// This test creating an answer.
TEST_F(PeerConnectionMediaTestPlanB, SimulcastAnswer) {
  auto caller = CreatePeerConnection();
  caller->AddVideoTrack("v0");
  auto offer = caller->CreateOffer();
  auto callee = CreatePeerConnection();
  auto callee_video_track = callee->AddVideoTrack("v1");
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  RTCOfferAnswerOptions options;
  options.num_simulcast_layers = 3;
  auto answer = callee->CreateAnswer(options);
  auto* description = cricket::GetFirstMediaContent(
      answer->description(),
      cricket::MEDIA_TYPE_VIDEO)->media_description();
  ASSERT_EQ(1u, description->streams().size());
  ASSERT_TRUE(description->streams()[0].get_ssrc_group("SIM"));
  EXPECT_EQ(3u, description->streams()[0].get_ssrc_group("SIM")->ssrcs.size());

  // Check that it actually creates simulcast aswell.
  callee->SetLocalDescription(std::move(answer));
  auto senders = callee->pc()->GetSenders();
  ASSERT_EQ(1u, senders.size());
  EXPECT_EQ(cricket::MediaType::MEDIA_TYPE_VIDEO, senders[0]->media_type());
  EXPECT_EQ(3u, senders[0]->GetParameters().encodings.size());
}

// Test that stopping the callee transceivers causes the media channels to be
// destroyed on the callee after calling SetLocalDescription on the local
// answer.
// See next test for equivalent behavior with Plan B semantics.
TEST_F(PeerConnectionMediaTestUnifiedPlan,
       StoppedLocalTransceiversRemovesMediaChannels) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Stop both audio and video transceivers on the callee.
  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  transceivers[0]->Stop();
  transceivers[1]->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  EXPECT_FALSE(callee->media_engine()->GetVoiceChannel(0));
  EXPECT_FALSE(callee->media_engine()->GetVideoChannel(0));
}

// Test that removing streams from a subsequent answer causes the send streams
// on the callee to be removed when applied locally.
// See previous test for equivalent behavior with Unified Plan semantics.
TEST_F(PeerConnectionMediaTestPlanB, EmptyLocalAnswerRemovesSendStreams) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();
  auto callee_audio_track = callee->AddAudioTrack("a");
  auto callee_video_track = callee->AddVideoTrack("v");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Remove both tracks from callee.
  callee->pc()->RemoveTrack(callee_audio_track);
  callee->pc()->RemoveTrack(callee_video_track);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_EQ(0u, callee_voice->send_streams().size());
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(0u, callee_video->send_streams().size());
  EXPECT_EQ(1u, callee_video->recv_streams().size());
}

// Test that a new stream in a subsequent offer causes a new receive stream to
// be created on the callee.
TEST_P(PeerConnectionMediaTest, NewStreamInRemoteOfferAddsRecvStreams) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Add second set of tracks to the caller.
  caller->AddAudioTrack("a2");
  caller->AddVideoTrack("v2");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto a1 = callee->media_engine()->GetVoiceChannel(0);
  auto a2 = callee->media_engine()->GetVoiceChannel(1);
  auto v1 = callee->media_engine()->GetVideoChannel(0);
  auto v2 = callee->media_engine()->GetVideoChannel(1);
  if (IsUnifiedPlan()) {
    ASSERT_TRUE(a1);
    EXPECT_EQ(1u, a1->recv_streams().size());
    ASSERT_TRUE(a2);
    EXPECT_EQ(1u, a2->recv_streams().size());
    ASSERT_TRUE(v1);
    EXPECT_EQ(1u, v1->recv_streams().size());
    ASSERT_TRUE(v2);
    EXPECT_EQ(1u, v2->recv_streams().size());
  } else {
    ASSERT_TRUE(a1);
    EXPECT_EQ(2u, a1->recv_streams().size());
    ASSERT_FALSE(a2);
    ASSERT_TRUE(v1);
    EXPECT_EQ(2u, v1->recv_streams().size());
    ASSERT_FALSE(v2);
  }
}

// Test that a new stream in a subsequent answer causes a new send stream to be
// created on the callee when added locally.
TEST_P(PeerConnectionMediaTest, NewStreamInLocalAnswerAddsSendStreams) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnectionWithAudioVideo();

  RTCOfferAnswerOptions offer_options;
  offer_options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  offer_options.offer_to_receive_video =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  RTCOfferAnswerOptions answer_options;

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get(), offer_options,
                                              answer_options));

  // Add second set of tracks to the callee.
  callee->AddAudioTrack("a2");
  callee->AddVideoTrack("v2");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get(), offer_options,
                                              answer_options));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);

  if (IsUnifiedPlan()) {
    EXPECT_EQ(1u, callee_voice->send_streams().size());
    EXPECT_EQ(1u, callee_video->send_streams().size());
  } else {
    EXPECT_EQ(2u, callee_voice->send_streams().size());
    EXPECT_EQ(2u, callee_video->send_streams().size());
  }
}

// A PeerConnection with no local streams and no explicit answer constraints
// should not reject any offered media sections.
TEST_P(PeerConnectionMediaTest,
       CreateAnswerWithNoStreamsAndDefaultOptionsDoesNotReject) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer();

  const auto* audio_content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(audio_content);
  EXPECT_FALSE(audio_content->rejected);

  const auto* video_content =
      cricket::GetFirstVideoContent(answer->description());
  ASSERT_TRUE(video_content);
  EXPECT_FALSE(video_content->rejected);
}

class PeerConnectionMediaOfferDirectionTest
    : public PeerConnectionMediaBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics,
                     std::tuple<bool, int, RtpTransceiverDirection>>> {
 protected:
  PeerConnectionMediaOfferDirectionTest()
      : PeerConnectionMediaBaseTest(std::get<0>(GetParam())) {
    auto param = std::get<1>(GetParam());
    send_media_ = std::get<0>(param);
    offer_to_receive_ = std::get<1>(param);
    expected_direction_ = std::get<2>(param);
  }

  bool send_media_;
  int offer_to_receive_;
  RtpTransceiverDirection expected_direction_;
};

// Tests that the correct direction is set on the media description according
// to the presence of a local media track and the offer_to_receive setting.
TEST_P(PeerConnectionMediaOfferDirectionTest, VerifyDirection) {
  auto caller = CreatePeerConnection();
  if (send_media_) {
    caller->AddAudioTrack("a");
  }

  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = offer_to_receive_;
  auto offer = caller->CreateOffer(options);

  auto* content = cricket::GetFirstMediaContent(offer->description(),
                                                cricket::MEDIA_TYPE_AUDIO);
  if (expected_direction_ == RtpTransceiverDirection::kInactive) {
    EXPECT_FALSE(content);
  } else {
    EXPECT_EQ(expected_direction_, content->media_description()->direction());
  }
}

// Note that in these tests, MD_INACTIVE indicates that no media section is
// included in the offer, not that the media direction is inactive.
INSTANTIATE_TEST_CASE_P(
    PeerConnectionMediaTest,
    PeerConnectionMediaOfferDirectionTest,
    Combine(
        Values(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
        Values(std::make_tuple(false, -1, RtpTransceiverDirection::kInactive),
               std::make_tuple(false, 0, RtpTransceiverDirection::kInactive),
               std::make_tuple(false, 1, RtpTransceiverDirection::kRecvOnly),
               std::make_tuple(true, -1, RtpTransceiverDirection::kSendRecv),
               std::make_tuple(true, 0, RtpTransceiverDirection::kSendOnly),
               std::make_tuple(true, 1, RtpTransceiverDirection::kSendRecv))));

class PeerConnectionMediaAnswerDirectionTest
    : public PeerConnectionMediaBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, RtpTransceiverDirection, bool, int>> {
 protected:
  PeerConnectionMediaAnswerDirectionTest()
      : PeerConnectionMediaBaseTest(std::get<0>(GetParam())) {
    offer_direction_ = std::get<1>(GetParam());
    send_media_ = std::get<2>(GetParam());
    offer_to_receive_ = std::get<3>(GetParam());
  }

  RtpTransceiverDirection offer_direction_;
  bool send_media_;
  int offer_to_receive_;
};

// Tests that the direction in an answer is correct according to direction sent
// in the offer, the presence of a local media track on the receive side and the
// offer_to_receive setting.
TEST_P(PeerConnectionMediaAnswerDirectionTest, VerifyDirection) {
  if (IsUnifiedPlan() &&
      offer_to_receive_ != RTCOfferAnswerOptions::kUndefined) {
    // offer_to_receive_ is not implemented when creating answers with Unified
    // Plan semantics specified.
    return;
  }

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");

  // Create the offer with an audio section and set its direction.
  auto offer = caller->CreateOffer();
  cricket::GetFirstAudioContentDescription(offer->description())
      ->set_direction(offer_direction_);

  auto callee = CreatePeerConnection();
  if (send_media_) {
    callee->AddAudioTrack("a");
  }
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  // Create the answer according to the test parameters.
  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = offer_to_receive_;
  auto answer = callee->CreateAnswer(options);

  // The expected direction in the answer is the intersection of each side's
  // capability to send/recv media.
  // For the offerer, the direction is given in the offer (offer_direction_).
  // For the answerer, the direction has two components:
  // 1. Send if the answerer has a local track to send.
  // 2. Receive if the answerer has explicitly set the offer_to_receive to 1 or
  //    if it has been left as default.
  bool offer_send = RtpTransceiverDirectionHasSend(offer_direction_);
  bool offer_recv = RtpTransceiverDirectionHasRecv(offer_direction_);

  // The negotiated components determine the direction set in the answer.
  bool negotiate_send = (send_media_ && offer_recv);
  bool negotiate_recv = ((offer_to_receive_ != 0) && offer_send);

  auto expected_direction =
      RtpTransceiverDirectionFromSendRecv(negotiate_send, negotiate_recv);
  EXPECT_EQ(expected_direction,
            GetMediaContentDirection(answer.get(), cricket::MEDIA_TYPE_AUDIO));
}

// Tests that the media section is rejected if and only if the callee has no
// local media track and has set offer_to_receive to 0, no matter which
// direction the caller indicated in the offer.
TEST_P(PeerConnectionMediaAnswerDirectionTest, VerifyRejected) {
  if (IsUnifiedPlan() &&
      offer_to_receive_ != RTCOfferAnswerOptions::kUndefined) {
    // offer_to_receive_ is not implemented when creating answers with Unified
    // Plan semantics specified.
    return;
  }

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");

  // Create the offer with an audio section and set its direction.
  auto offer = caller->CreateOffer();
  cricket::GetFirstAudioContentDescription(offer->description())
      ->set_direction(offer_direction_);

  auto callee = CreatePeerConnection();
  if (send_media_) {
    callee->AddAudioTrack("a");
  }
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  // Create the answer according to the test parameters.
  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = offer_to_receive_;
  auto answer = callee->CreateAnswer(options);

  // The media section is rejected if and only if offer_to_receive is explicitly
  // set to 0 and there is no media to send.
  auto* audio_content = cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(audio_content);
  EXPECT_EQ((offer_to_receive_ == 0 && !send_media_), audio_content->rejected);
}

INSTANTIATE_TEST_CASE_P(PeerConnectionMediaTest,
                        PeerConnectionMediaAnswerDirectionTest,
                        Combine(Values(SdpSemantics::kPlanB,
                                       SdpSemantics::kUnifiedPlan),
                                Values(RtpTransceiverDirection::kInactive,
                                       RtpTransceiverDirection::kSendOnly,
                                       RtpTransceiverDirection::kRecvOnly,
                                       RtpTransceiverDirection::kSendRecv),
                                Bool(),
                                Values(-1, 0, 1)));

TEST_P(PeerConnectionMediaTest, OfferHasDifferentDirectionForAudioVideo) {
  auto caller = CreatePeerConnection();
  caller->AddVideoTrack("v");

  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 0;
  auto offer = caller->CreateOffer(options);

  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            GetMediaContentDirection(offer.get(), cricket::MEDIA_TYPE_AUDIO));
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            GetMediaContentDirection(offer.get(), cricket::MEDIA_TYPE_VIDEO));
}

TEST_P(PeerConnectionMediaTest, AnswerHasDifferentDirectionsForAudioVideo) {
  if (IsUnifiedPlan()) {
    // offer_to_receive_ is not implemented when creating answers with Unified
    // Plan semantics specified.
    return;
  }

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();
  callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 0;
  auto answer = callee->CreateAnswer(options);

  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            GetMediaContentDirection(answer.get(), cricket::MEDIA_TYPE_AUDIO));
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            GetMediaContentDirection(answer.get(), cricket::MEDIA_TYPE_VIDEO));
}

void AddComfortNoiseCodecsToSend(cricket::FakeMediaEngine* media_engine) {
  const cricket::AudioCodec kComfortNoiseCodec8k(102, "CN", 8000, 0, 1);
  const cricket::AudioCodec kComfortNoiseCodec16k(103, "CN", 16000, 0, 1);

  auto codecs = media_engine->voice().send_codecs();
  codecs.push_back(kComfortNoiseCodec8k);
  codecs.push_back(kComfortNoiseCodec16k);
  media_engine->SetAudioCodecs(codecs);
}

bool HasAnyComfortNoiseCodecs(const cricket::SessionDescription* desc) {
  const auto* audio_desc = cricket::GetFirstAudioContentDescription(desc);
  for (const auto& codec : audio_desc->codecs()) {
    if (codec.name == "CN") {
      return true;
    }
  }
  return false;
}

TEST_P(PeerConnectionMediaTest,
       CreateOfferWithNoVoiceActivityDetectionIncludesNoComfortNoiseCodecs) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  AddComfortNoiseCodecsToSend(caller->media_engine());

  RTCOfferAnswerOptions options;
  options.voice_activity_detection = false;
  auto offer = caller->CreateOffer(options);

  EXPECT_FALSE(HasAnyComfortNoiseCodecs(offer->description()));
}

TEST_P(PeerConnectionMediaTest,
       CreateAnswerWithNoVoiceActivityDetectionIncludesNoComfortNoiseCodecs) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  AddComfortNoiseCodecsToSend(caller->media_engine());
  auto callee = CreatePeerConnectionWithAudioVideo();
  AddComfortNoiseCodecsToSend(callee->media_engine());

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCOfferAnswerOptions options;
  options.voice_activity_detection = false;
  auto answer = callee->CreateAnswer(options);

  EXPECT_FALSE(HasAnyComfortNoiseCodecs(answer->description()));
}

// The following test group verifies that we reject answers with invalid media
// sections as per RFC 3264.

class PeerConnectionMediaInvalidMediaTest
    : public PeerConnectionMediaBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          SdpSemantics,
          std::tuple<std::string,
                     std::function<void(cricket::SessionDescription*)>,
                     std::string>>> {
 protected:
  PeerConnectionMediaInvalidMediaTest()
      : PeerConnectionMediaBaseTest(std::get<0>(GetParam())) {
    auto param = std::get<1>(GetParam());
    mutator_ = std::get<1>(param);
    expected_error_ = std::get<2>(param);
  }

  std::function<void(cricket::SessionDescription*)> mutator_;
  std::string expected_error_;
};

TEST_P(PeerConnectionMediaInvalidMediaTest, FailToSetRemoteAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  mutator_(answer->description());

  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(std::move(answer), &error));
  EXPECT_EQ("Failed to set remote answer sdp: " + expected_error_, error);
}

TEST_P(PeerConnectionMediaInvalidMediaTest, FailToSetLocalAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  mutator_(answer->description());

  std::string error;
  ASSERT_FALSE(callee->SetLocalDescription(std::move(answer), &error));
  EXPECT_EQ("Failed to set local answer sdp: " + expected_error_, error);
}

void RemoveVideoContent(cricket::SessionDescription* desc) {
  auto content_name = cricket::GetFirstVideoContent(desc)->name;
  desc->RemoveContentByName(content_name);
  desc->RemoveTransportInfoByName(content_name);
}

void RenameVideoContent(cricket::SessionDescription* desc) {
  auto* video_content = cricket::GetFirstVideoContent(desc);
  auto* transport_info = desc->GetTransportInfoByName(video_content->name);
  video_content->name = "video_renamed";
  transport_info->content_name = video_content->name;
}

void ReverseMediaContent(cricket::SessionDescription* desc) {
  std::reverse(desc->contents().begin(), desc->contents().end());
  std::reverse(desc->transport_infos().begin(), desc->transport_infos().end());
}

void ChangeMediaTypeAudioToVideo(cricket::SessionDescription* desc) {
  std::string audio_mid = cricket::GetFirstAudioContent(desc)->name;
  desc->RemoveContentByName(audio_mid);
  auto* video_content = cricket::GetFirstVideoContent(desc);
  desc->AddContent(audio_mid, video_content->type,
                   video_content->media_description()->Copy());
}

constexpr char kMLinesOutOfOrder[] =
    "The order of m-lines in answer doesn't match order in offer. Rejecting "
    "answer.";

INSTANTIATE_TEST_CASE_P(
    PeerConnectionMediaTest,
    PeerConnectionMediaInvalidMediaTest,
    Combine(Values(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
            Values(std::make_tuple("remove video",
                                   RemoveVideoContent,
                                   kMLinesOutOfOrder),
                   std::make_tuple("rename video",
                                   RenameVideoContent,
                                   kMLinesOutOfOrder),
                   std::make_tuple("reverse media sections",
                                   ReverseMediaContent,
                                   kMLinesOutOfOrder),
                   std::make_tuple("change audio type to video type",
                                   ChangeMediaTypeAudioToVideo,
                                   kMLinesOutOfOrder))));

// Test that the correct media engine send/recv streams are created when doing
// a series of offer/answers where audio/video are both sent, then audio is
// rejected, then both audio/video sent again.
TEST_P(PeerConnectionMediaTest, TestAVOfferWithAudioOnlyAnswer) {
  if (IsUnifiedPlan()) {
    // offer_to_receive_ is not implemented when creating answers with Unified
    // Plan semantics specified.
    return;
  }

  RTCOfferAnswerOptions options_reject_video;
  options_reject_video.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  options_reject_video.offer_to_receive_video = 0;

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  caller->AddVideoTrack("v");
  auto callee = CreatePeerConnection();

  // Caller initially offers to send/recv audio and video.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  // Callee accepts the audio as recv only but rejects the video.
  ASSERT_TRUE(caller->SetRemoteDescription(
      callee->CreateAnswerAndSetAsLocal(options_reject_video)));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  EXPECT_EQ(0u, caller_voice->recv_streams().size());
  EXPECT_EQ(1u, caller_voice->send_streams().size());
  auto caller_video = caller->media_engine()->GetVideoChannel(0);
  EXPECT_FALSE(caller_video);

  // Callee adds its own audio/video stream and offers to receive audio/video
  // too.
  callee->AddAudioTrack("a");
  auto callee_video_track = callee->AddVideoTrack("v");
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);
  EXPECT_EQ(1u, callee_video->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());

  // Callee removes video but keeps audio and rejects the video once again.
  callee->pc()->RemoveTrack(callee_video_track);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      callee->SetLocalDescription(callee->CreateAnswer(options_reject_video)));

  callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_FALSE(callee_video);
}

// Test that the correct media engine send/recv streams are created when doing
// a series of offer/answers where audio/video are both sent, then video is
// rejected, then both audio/video sent again.
TEST_P(PeerConnectionMediaTest, TestAVOfferWithVideoOnlyAnswer) {
  if (IsUnifiedPlan()) {
    // offer_to_receive_ is not implemented when creating answers with Unified
    // Plan semantics specified.
    return;
  }

  // Disable the bundling here. If the media is bundled on audio
  // transport, then we can't reject the audio because switching the bundled
  // transport is not currently supported.
  // (https://bugs.chromium.org/p/webrtc/issues/detail?id=6704)
  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  RTCOfferAnswerOptions options_reject_audio = options_no_bundle;
  options_reject_audio.offer_to_receive_audio = 0;
  options_reject_audio.offer_to_receive_video =
      RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  caller->AddVideoTrack("v");
  auto callee = CreatePeerConnection();

  // Caller initially offers to send/recv audio and video.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  // Callee accepts the video as recv only but rejects the audio.
  ASSERT_TRUE(caller->SetRemoteDescription(
      callee->CreateAnswerAndSetAsLocal(options_reject_audio)));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  EXPECT_FALSE(caller_voice);
  auto caller_video = caller->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(caller_video);
  EXPECT_EQ(0u, caller_video->recv_streams().size());
  EXPECT_EQ(1u, caller_video->send_streams().size());

  // Callee adds its own audio/video stream and offers to receive audio/video
  // too.
  auto callee_audio_track = callee->AddAudioTrack("a");
  callee->AddVideoTrack("v");
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(caller->SetRemoteDescription(
      callee->CreateAnswerAndSetAsLocal(options_no_bundle)));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);
  EXPECT_EQ(1u, callee_video->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());

  // Callee removes audio but keeps video and rejects the audio once again.
  callee->pc()->RemoveTrack(callee_audio_track);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      callee->SetLocalDescription(callee->CreateAnswer(options_reject_audio)));

  callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_FALSE(callee_voice);
  callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);
  EXPECT_EQ(1u, callee_video->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());
}

// Tests that if the underlying video encoder fails to be initialized (signaled
// by failing to set send codecs), the PeerConnection signals the error to the
// client.
TEST_P(PeerConnectionMediaTest, MediaEngineErrorPropagatedToClients) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto video_channel = caller->media_engine()->GetVideoChannel(0);
  video_channel->set_fail_set_send_codecs(true);

  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal(),
                                            &error));
  EXPECT_EQ(
      "Failed to set remote answer sdp: Failed to set remote video description "
      "send parameters.",
      error);
}

// Tests that if the underlying video encoder fails once then subsequent
// attempts at setting the local/remote description will also fail, even if
// SetSendCodecs no longer fails.
TEST_P(PeerConnectionMediaTest,
       FailToApplyDescriptionIfVideoEncoderHasEverFailed) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto video_channel = caller->media_engine()->GetVideoChannel(0);
  video_channel->set_fail_set_send_codecs(true);

  EXPECT_FALSE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  video_channel->set_fail_set_send_codecs(false);

  EXPECT_FALSE(caller->SetRemoteDescription(callee->CreateAnswer()));
  EXPECT_FALSE(caller->SetLocalDescription(caller->CreateOffer()));
}

void RenameContent(cricket::SessionDescription* desc,
                   cricket::MediaType media_type,
                   const std::string& new_name) {
  auto* content = cricket::GetFirstMediaContent(desc, media_type);
  RTC_DCHECK(content);
  std::string old_name = content->name;
  content->name = new_name;
  auto* transport = desc->GetTransportInfoByName(old_name);
  RTC_DCHECK(transport);
  transport->content_name = new_name;

  // Rename the content name in the BUNDLE group.
  cricket::ContentGroup new_bundle_group =
      *desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group.RemoveContentName(old_name);
  new_bundle_group.AddContentName(new_name);
  desc->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  desc->AddGroup(new_bundle_group);
}

// Tests that an answer responds with the same MIDs as the offer.
TEST_P(PeerConnectionMediaTest, AnswerHasSameMidsAsOffer) {
  const std::string kAudioMid = "notdefault1";
  const std::string kVideoMid = "notdefault2";

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  RenameContent(offer->description(), cricket::MEDIA_TYPE_AUDIO, kAudioMid);
  RenameContent(offer->description(), cricket::MEDIA_TYPE_VIDEO, kVideoMid);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  EXPECT_EQ(kAudioMid,
            cricket::GetFirstAudioContent(answer->description())->name);
  EXPECT_EQ(kVideoMid,
            cricket::GetFirstVideoContent(answer->description())->name);
}

// Test that if the callee creates a re-offer, the MIDs are the same as the
// original offer.
TEST_P(PeerConnectionMediaTest, ReOfferHasSameMidsAsFirstOffer) {
  const std::string kAudioMid = "notdefault1";
  const std::string kVideoMid = "notdefault2";

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  RenameContent(offer->description(), cricket::MEDIA_TYPE_AUDIO, kAudioMid);
  RenameContent(offer->description(), cricket::MEDIA_TYPE_VIDEO, kVideoMid);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto reoffer = callee->CreateOffer();
  EXPECT_EQ(kAudioMid,
            cricket::GetFirstAudioContent(reoffer->description())->name);
  EXPECT_EQ(kVideoMid,
            cricket::GetFirstVideoContent(reoffer->description())->name);
}

TEST_P(PeerConnectionMediaTest,
       CombinedAudioVideoBweConfigPropagatedToMediaEngine) {
  RTCConfiguration config;
  config.combined_audio_video_bwe.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  const cricket::AudioOptions& audio_options = caller_voice->options();
  EXPECT_EQ(config.combined_audio_video_bwe,
            audio_options.combined_audio_video_bwe);
}

TEST_P(PeerConnectionMediaTest, MediaTransportPropagatedToVoiceEngine) {
  RTCConfiguration config;

  // Setup PeerConnection to use media transport.
  config.use_media_transport = true;

  // Force SDES.
  config.enable_dtls_srtp = false;

  auto caller = CreatePeerConnectionWithAudio(config);
  auto callee = CreatePeerConnectionWithAudio(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  ASSERT_TRUE(callee_voice);

  // Make sure media transport is propagated to voice channel.
  FakeMediaTransport* caller_voice_media_transport =
      static_cast<FakeMediaTransport*>(caller_voice->media_transport());
  FakeMediaTransport* callee_voice_media_transport =
      static_cast<FakeMediaTransport*>(callee_voice->media_transport());
  ASSERT_NE(nullptr, caller_voice_media_transport);
  ASSERT_NE(nullptr, callee_voice_media_transport);

  // Make sure media transport is created with correct is_caller.
  EXPECT_TRUE(caller_voice_media_transport->is_caller());
  EXPECT_FALSE(callee_voice_media_transport->is_caller());

  // TODO(sukhanov): Propagate media transport to video channel.
  // This test does NOT set up video channels, because currently it causes
  // us to create two media transports.
}

TEST_P(PeerConnectionMediaTest, MediaTransportOnlyForDataChannels) {
  RTCConfiguration config;

  // Setup PeerConnection to use media transport for data channels.
  config.use_media_transport_for_data_channels = true;

  // Force SDES.
  config.enable_dtls_srtp = false;

  auto caller = CreatePeerConnectionWithAudio(config);
  auto callee = CreatePeerConnectionWithAudio(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  ASSERT_TRUE(callee_voice);

  // Make sure media transport is not propagated to voice channel.
  EXPECT_EQ(nullptr, caller_voice->media_transport());
  EXPECT_EQ(nullptr, callee_voice->media_transport());
}

TEST_P(PeerConnectionMediaTest, MediaTransportForMediaAndDataChannels) {
  RTCConfiguration config;

  // Setup PeerConnection to use media transport for both media and data
  // channels.
  config.use_media_transport = true;
  config.use_media_transport_for_data_channels = true;

  // Force SDES.
  config.enable_dtls_srtp = false;

  auto caller = CreatePeerConnectionWithAudio(config);
  auto callee = CreatePeerConnectionWithAudio(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  ASSERT_TRUE(callee_voice);

  // Make sure media transport is propagated to voice channel.
  FakeMediaTransport* caller_voice_media_transport =
      static_cast<FakeMediaTransport*>(caller_voice->media_transport());
  FakeMediaTransport* callee_voice_media_transport =
      static_cast<FakeMediaTransport*>(callee_voice->media_transport());
  ASSERT_NE(nullptr, caller_voice_media_transport);
  ASSERT_NE(nullptr, callee_voice_media_transport);

  // Make sure media transport is created with correct is_caller.
  EXPECT_TRUE(caller_voice_media_transport->is_caller());
  EXPECT_FALSE(callee_voice_media_transport->is_caller());
}

TEST_P(PeerConnectionMediaTest, MediaTransportNotPropagatedToVoiceEngine) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  ASSERT_TRUE(callee_voice);

  // Since we did not setup PeerConnection to use media transport, media
  // transport should not be created / propagated to the voice engine.
  ASSERT_EQ(nullptr, caller_voice->media_transport());
  ASSERT_EQ(nullptr, callee_voice->media_transport());

  auto caller_video = caller->media_engine()->GetVideoChannel(0);
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_EQ(nullptr, caller_video->media_transport());
  ASSERT_EQ(nullptr, callee_video->media_transport());
}

INSTANTIATE_TEST_CASE_P(PeerConnectionMediaTest,
                        PeerConnectionMediaTest,
                        Values(SdpSemantics::kPlanB,
                               SdpSemantics::kUnifiedPlan));

}  // namespace webrtc
