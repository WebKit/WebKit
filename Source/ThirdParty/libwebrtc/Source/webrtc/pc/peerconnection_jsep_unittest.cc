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
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "pc/mediasession.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "absl/memory/memory.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakesctptransport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

// This file contains tests that ensure the PeerConnection's implementation of
// CreateOffer/CreateAnswer/SetLocalDescription/SetRemoteDescription conform
// to the JavaScript Session Establishment Protocol (JSEP).
// For now these semantics are only available when configuring the
// PeerConnection with Unified Plan, but eventually that will be the default.

namespace webrtc {

using cricket::MediaContentDescription;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::Values;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class PeerConnectionFactoryForJsepTest : public PeerConnectionFactory {
 public:
  PeerConnectionFactoryForJsepTest()
      : PeerConnectionFactory(rtc::Thread::Current(),
                              rtc::Thread::Current(),
                              rtc::Thread::Current(),
                              cricket::WebRtcMediaEngineFactory::Create(
                                  rtc::scoped_refptr<AudioDeviceModule>(
                                      FakeAudioCaptureModule::Create()),
                                  CreateBuiltinAudioEncoderFactory(),
                                  CreateBuiltinAudioDecoderFactory(),
                                  CreateBuiltinVideoEncoderFactory(),
                                  CreateBuiltinVideoDecoderFactory(),
                                  nullptr,
                                  AudioProcessingBuilder().Create()),
                              CreateCallFactory(),
                              nullptr) {}

  std::unique_ptr<cricket::SctpTransportInternalFactory>
  CreateSctpTransportInternalFactory() {
    return absl::make_unique<FakeSctpTransportFactory>();
  }
};

class PeerConnectionJsepTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapper> WrapperPtr;

  PeerConnectionJsepTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
        new rtc::RefCountedObject<PeerConnectionFactoryForJsepTest>());
    RTC_CHECK(pc_factory->Initialize());
    auto observer = absl::make_unique<MockPeerConnectionObserver>();
    auto pc = pc_factory->CreatePeerConnection(config, nullptr, nullptr,
                                               observer.get());
    if (!pc) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(pc.get());
    return absl::make_unique<PeerConnectionWrapper>(pc_factory, pc,
                                                    std::move(observer));
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

// Tests for JSEP initial offer generation.

// Test that an offer created by a PeerConnection with no transceivers generates
// no media sections.
TEST_F(PeerConnectionJsepTest, EmptyInitialOffer) {
  auto caller = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  ASSERT_EQ(0u, offer->description()->contents().size());
}

// Test that an initial offer with one audio track generates one audio media
// section.
TEST_F(PeerConnectionJsepTest, AudioOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, contents[0].media_description()->type());
}

// Test than an initial offer with one video track generates one video media
// section
TEST_F(PeerConnectionJsepTest, VideoOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, contents[0].media_description()->type());
}

// Test that an initial offer with one data channel generates one data media
// section.
TEST_F(PeerConnectionJsepTest, DataOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("dc");

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA, contents[0].media_description()->type());
}

// Test that creating multiple data channels only results in one data section
// generated in the offer.
TEST_F(PeerConnectionJsepTest, MultipleDataChannelsCreateOnlyOneDataSection) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("first");
  caller->CreateDataChannel("second");
  caller->CreateDataChannel("third");

  auto offer = caller->CreateOffer();
  ASSERT_EQ(1u, offer->description()->contents().size());
}

// Test that multiple media sections in the initial offer are ordered in the
// order the transceivers were added to the PeerConnection. This is required by
// JSEP section 5.2.1.
TEST_F(PeerConnectionJsepTest, MediaSectionsInInitialOfferOrderedCorrectly) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(3u, contents.size());

  const MediaContentDescription* media_description1 =
      contents[0].media_description();
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description1->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv,
            media_description1->direction());

  const MediaContentDescription* media_description2 =
      contents[1].media_description();
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, media_description2->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv,
            media_description2->direction());

  const MediaContentDescription* media_description3 =
      contents[2].media_description();
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description3->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            media_description3->direction());
}

// Test that media sections in the initial offer have different mids.
TEST_F(PeerConnectionJsepTest, MediaSectionsInInitialOfferHaveDifferentMids) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_NE(contents[0].name, contents[1].name);
}

TEST_F(PeerConnectionJsepTest,
       StoppedTransceiverHasNoMediaSectionInInitialOffer) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->Stop();

  auto offer = caller->CreateOffer();
  EXPECT_EQ(0u, offer->description()->contents().size());
}

// Tests for JSEP SetLocalDescription with a local offer.

TEST_F(PeerConnectionJsepTest, SetLocalEmptyOfferCreatesNoTransceivers) {
  auto caller = CreatePeerConnection();

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetSenders(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetReceivers(), ElementsAre());
}

TEST_F(PeerConnectionJsepTest, SetLocalOfferSetsTransceiverMid) {
  auto caller = CreatePeerConnection();
  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  auto offer = caller->CreateOffer();
  std::string audio_mid = offer->description()->contents()[0].name;
  std::string video_mid = offer->description()->contents()[1].name;

  ASSERT_TRUE(caller->SetLocalDescription(std::move(offer)));

  EXPECT_EQ(audio_mid, audio_transceiver->mid());
  EXPECT_EQ(video_mid, video_transceiver->mid());
}

// Tests for JSEP SetRemoteDescription with a remote offer.

// Test that setting a remote offer with sendrecv audio and video creates two
// transceivers, one for receiving audio and one for receiving video.
TEST_F(PeerConnectionJsepTest, SetRemoteOfferCreatesTransceivers) {
  auto caller = CreatePeerConnection();
  auto caller_audio = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto caller_video = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());

  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceivers[0]->media_type());
  EXPECT_EQ(caller_audio->mid(), transceivers[0]->mid());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceivers[0]->direction());
  EXPECT_EQ(0u, transceivers[0]->sender()->stream_ids().size());

  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceivers[1]->media_type());
  EXPECT_EQ(caller_video->mid(), transceivers[1]->mid());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceivers[1]->direction());
  EXPECT_EQ(0u, transceivers[1]->sender()->stream_ids().size());
}

// Test that setting a remote offer with an audio track will reuse the
// transceiver created for a local audio track added by AddTrack.
// This is specified in JSEP section 5.10 (Applying a Remote Description). The
// intent is to preserve backwards compatibility with clients who only use the
// AddTrack API.
TEST_F(PeerConnectionJsepTest, SetRemoteOfferReusesTransceiverFromAddTrack) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto caller_audio = caller->pc()->GetTransceivers()[0];
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[0]->receiver()->track()->kind());
  EXPECT_EQ(caller_audio->mid(), transceivers[0]->mid());
}

// Test that setting a remote offer with an audio track marked sendonly will not
// reuse a transceiver created by AddTrack. JSEP only allows the transceiver to
// be reused if the offer direction is sendrecv or recvonly.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferDoesNotReuseTransceiverIfDirectionSendOnly) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto caller_audio = caller->pc()->GetTransceivers()[0];
  caller_audio->SetDirection(RtpTransceiverDirection::kSendOnly);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller_audio->mid(), transceivers[1]->mid());
}

// Test that setting a remote offer with an audio track will not reuse a
// transceiver added by AddTransceiver. The logic for reusing a transceiver is
// specific to those added by AddTrack and is tested above.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferDoesNotReuseTransceiverFromAddTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  auto transceiver = callee->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[1]->receiver()->track()->kind());
}

// Test that setting a remote offer with an audio track will not reuse a
// transceiver created for a local video track added by AddTrack.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferDoesNotReuseTransceiverOfWrongType) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  auto video_sender = callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[1]->receiver()->track()->kind());
}

// Test that setting a remote offer with an audio track will not reuse a
// stopped transceiver.
TEST_F(PeerConnectionJsepTest, SetRemoteOfferDoesNotReuseStoppedTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->Stop();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_TRUE(transceivers[0]->stopped());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_FALSE(transceivers[1]->stopped());
}

// Test that audio and video transceivers created on the remote side with
// AddTrack will all be reused if there is the same number of audio/video tracks
// in the remote offer. Additionally, this tests that transceivers are
// successfully matched even if they are in a different order on the remote
// side.
TEST_F(PeerConnectionJsepTest, SetRemoteOfferReusesTransceiversOfBothTypes) {
  auto caller = CreatePeerConnection();
  caller->AddVideoTrack("v");
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto caller_transceivers = caller->pc()->GetTransceivers();
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(caller_transceivers[0]->mid(), callee_transceivers[1]->mid());
  EXPECT_EQ(caller_transceivers[1]->mid(), callee_transceivers[0]->mid());
}

// Tests for JSEP initial CreateAnswer.

// Test that the answer to a remote offer creates media sections for each
// offered media in the same order and with the same mids.
TEST_F(PeerConnectionJsepTest, CreateAnswerHasSameMidsAsOffer) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto third_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  caller->CreateDataChannel("dc");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  const auto* offer_data = cricket::GetFirstDataContent(offer->description());
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(4u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, contents[0].media_description()->type());
  EXPECT_EQ(first_transceiver->mid(), contents[0].name);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, contents[1].media_description()->type());
  EXPECT_EQ(second_transceiver->mid(), contents[1].name);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, contents[2].media_description()->type());
  EXPECT_EQ(third_transceiver->mid(), contents[2].name);
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA, contents[3].media_description()->type());
  EXPECT_EQ(offer_data->name, contents[3].name);
}

// Test that an answering media section is marked as rejected if the underlying
// transceiver has been stopped.
TEST_F(PeerConnectionJsepTest, CreateAnswerRejectsStoppedTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  callee->pc()->GetTransceivers()[0]->Stop();

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
}

// Test that CreateAnswer will generate media sections which will only send or
// receive if the offer indicates it can do the reciprocating direction.
// The full matrix is tested more extensively in MediaSession.
TEST_F(PeerConnectionJsepTest, CreateAnswerNegotiatesDirection) {
  auto caller = CreatePeerConnection();
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            contents[0].media_description()->direction());
}

// Tests for JSEP SetLocalDescription with a local answer.
// Note that these test only the additional behaviors not covered by
// SetLocalDescription with a local offer.

// Test that SetLocalDescription with an answer sets the current_direction
// property of the transceivers mentioned in the session description.
TEST_F(PeerConnectionJsepTest, SetLocalAnswerUpdatesCurrentDirection) {
  auto caller = CreatePeerConnection();
  auto caller_audio = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller_audio->SetDirection(RtpTransceiverDirection::kRecvOnly);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  // Since the offer was recvonly and the transceiver direction is sendrecv,
  // the negotiated direction will be sendonly.
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            transceivers[0]->current_direction());
}

// Tests for JSEP SetRemoteDescription with a remote answer.
// Note that these test only the additional behaviors not covered by
// SetRemoteDescription with a remote offer.

TEST_F(PeerConnectionJsepTest, SetRemoteAnswerUpdatesCurrentDirection) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  auto callee_audio = callee->pc()->GetTransceivers()[0];
  callee_audio->SetDirection(RtpTransceiverDirection::kSendOnly);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  // Since the remote transceiver was set to sendonly, the negotiated direction
  // in the answer would be sendonly which we apply as recvonly to the local
  // transceiver.
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            transceivers[0]->current_direction());
}

// Tests for multiple round trips.

// Test that setting a transceiver with the inactive direction does not stop it
// on either the caller or the callee.
TEST_F(PeerConnectionJsepTest, SettingTransceiverInactiveDoesNotStopIt) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->SetDirection(
      RtpTransceiverDirection::kInactive);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->pc()->GetTransceivers()[0]->stopped());
  EXPECT_FALSE(callee->pc()->GetTransceivers()[0]->stopped());
}

// Test that if a transceiver had been associated and later stopped, then a
// media section is still generated for it and the media section is marked as
// rejected.
TEST_F(PeerConnectionJsepTest,
       ReOfferMediaSectionForAssociatedStoppedTransceiverIsRejected) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_TRUE(transceiver->mid());
  transceiver->Stop();

  auto reoffer = caller->CreateOffer();
  auto contents = reoffer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
}

// Test that stopping an associated transceiver on the caller side will stop the
// corresponding transceiver on the remote side when the remote offer is
// applied.
TEST_F(PeerConnectionJsepTest,
       StoppingTransceiverInOfferStopsTransceiverOnRemoteSide) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  transceiver->Stop();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  EXPECT_TRUE(transceivers[0]->stopped());
  EXPECT_TRUE(transceivers[0]->mid());
}

// Test that CreateOffer will only generate a recycled media section if the
// transceiver to be recycled has been seen stopped by the other side first.
TEST_F(PeerConnectionJsepTest,
       CreateOfferDoesNotRecycleMediaSectionIfFirstStopped) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  first_transceiver->Stop();

  auto reoffer = caller->CreateOffer();
  auto contents = reoffer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
  EXPECT_FALSE(contents[1].rejected);
}

// Test that the offer/answer and the transceivers are correctly generated and
// updated when the media section is recycled after the callee stops a
// transceiver and sends an answer with a 0 port.
TEST_F(PeerConnectionJsepTest,
       RecycleMediaSectionWhenStoppingTransceiverOnAnswerer) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  callee->pc()->GetTransceivers()[0]->Stop();
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
  EXPECT_TRUE(first_transceiver->stopped());
  // First transceivers aren't dissociated yet.
  ASSERT_NE(absl::nullopt, first_transceiver->mid());
  std::string first_mid = *first_transceiver->mid();
  EXPECT_EQ(first_mid, callee->pc()->GetTransceivers()[0]->mid());

  // New offer exchange with new transceivers that recycles the m section
  // correctly.
  caller->AddAudioTrack("audio2");
  callee->AddAudioTrack("audio2");
  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  std::string second_mid = offer_contents[0].name;
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_NE(first_mid, second_mid);

  // Setting the offer on each side will dissociate the first transceivers and
  // associate the new transceivers.
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, first_transceiver->mid());
  EXPECT_EQ(second_mid, caller->pc()->GetTransceivers()[1]->mid());
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  EXPECT_EQ(absl::nullopt, callee->pc()->GetTransceivers()[0]->mid());
  EXPECT_EQ(second_mid, callee->pc()->GetTransceivers()[1]->mid());

  // The new answer should also recycle the m section correctly.
  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);

  // Finishing the negotiation shouldn't add or dissociate any transceivers.
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, caller_transceivers.size());
  EXPECT_EQ(absl::nullopt, caller_transceivers[0]->mid());
  EXPECT_EQ(second_mid, caller_transceivers[1]->mid());
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(absl::nullopt, callee_transceivers[0]->mid());
  EXPECT_EQ(second_mid, callee_transceivers[1]->mid());
}

// Test that creating/setting a local offer that recycles an m= section is
// idempotent.
TEST_F(PeerConnectionJsepTest, CreateOfferRecyclesWhenOfferingTwice) {
  // Do a negotiation with a port 0 for the media section.
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  first_transceiver->Stop();
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->AddAudioTrack("audio2");

  // Create a new offer that recycles the media section and set it as a local
  // description.
  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  ASSERT_TRUE(caller->SetLocalDescription(std::move(offer)));
  EXPECT_FALSE(caller->pc()->GetTransceivers()[1]->stopped());
  std::string second_mid = offer_contents[0].name;

  // Create another new offer and set the local description again without the
  // rest of any negotation ocurring.
  auto second_offer = caller->CreateOffer();
  auto second_offer_contents = second_offer->description()->contents();
  ASSERT_EQ(1u, second_offer_contents.size());
  EXPECT_FALSE(second_offer_contents[0].rejected);
  // The mid shouldn't change.
  EXPECT_EQ(second_mid, second_offer_contents[0].name);

  ASSERT_TRUE(caller->SetLocalDescription(std::move(second_offer)));
  // Make sure that the caller's transceivers are associated correctly.
  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, caller_transceivers.size());
  EXPECT_EQ(absl::nullopt, caller_transceivers[0]->mid());
  EXPECT_EQ(second_mid, caller_transceivers[1]->mid());
  EXPECT_FALSE(caller_transceivers[1]->stopped());
}

// Test that the offer/answer and transceivers for both the caller and callee
// side are generated/updated correctly when recycling an audio/video media
// section as a media section of either the same or opposite type.
// Correct recycling works as follows:
// - The m= section is re-offered with a new MID value and the new media type.
// - The previously-associated transceiver is dissociated when the new offer is
//   set as a local description on the offerer or as a remote description on
//   the answerer.
// - The new transceiver is associated with the new MID value.
class RecycleMediaSectionTest
    : public PeerConnectionJsepTest,
      public testing::WithParamInterface<
          std::tuple<cricket::MediaType, cricket::MediaType>> {
 protected:
  RecycleMediaSectionTest() {
    first_type_ = std::get<0>(GetParam());
    second_type_ = std::get<1>(GetParam());
  }

  cricket::MediaType first_type_;
  cricket::MediaType second_type_;
};

// Test that recycling works properly when a new transceiver recycles an m=
// section that was rejected in both the current local and remote descriptions.
TEST_P(RecycleMediaSectionTest, CurrentLocalAndCurrentRemoteRejected) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  std::string first_mid = *first_transceiver->mid();
  first_transceiver->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto second_transceiver = caller->AddTransceiver(second_type_);

  // The offer should reuse the previous media section but allocate a new MID
  // and change the media type.
  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_EQ(second_type_, offer_contents[0].media_description()->type());
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  // Setting the local offer will dissociate the previous transceiver and set
  // the MID for the new transceiver.
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, first_transceiver->mid());
  EXPECT_EQ(second_mid, second_transceiver->mid());

  // Setting the remote offer will dissociate the previous transceiver and
  // create a new transceiver for the media section.
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(absl::nullopt, callee_transceivers[0]->mid());
  EXPECT_EQ(first_type_, callee_transceivers[0]->media_type());
  EXPECT_EQ(second_mid, callee_transceivers[1]->mid());
  EXPECT_EQ(second_type_, callee_transceivers[1]->media_type());

  // The answer should have only one media section for the new transceiver.
  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);
  EXPECT_EQ(second_type_, answer_contents[0].media_description()->type());

  // Setting the local answer should succeed.
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));

  // Setting the remote answer should succeed and not create any new
  // transceivers.
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
  ASSERT_EQ(2u, caller->pc()->GetTransceivers().size());
  ASSERT_EQ(2u, callee->pc()->GetTransceivers().size());
}

// Test that recycling works properly when a new transceiver recycles an m=
// section that was rejected in only the current remote description.
TEST_P(RecycleMediaSectionTest, CurrentRemoteOnlyRejected) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  std::string first_mid = *caller_first_transceiver->mid();
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  callee_first_transceiver->Stop();

  // The answer will have a rejected m= section.
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // The offer should reuse the previous media section but allocate a new MID
  // and change the media type.
  auto caller_second_transceiver = caller->AddTransceiver(second_type_);
  auto offer = caller->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_EQ(second_type_, offer_contents[0].media_description()->type());
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  // Setting the local offer will dissociate the previous transceiver and set
  // the MID for the new transceiver.
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, caller_first_transceiver->mid());
  EXPECT_EQ(second_mid, caller_second_transceiver->mid());

  // Setting the remote offer will dissociate the previous transceiver and
  // create a new transceiver for the media section.
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(absl::nullopt, callee_transceivers[0]->mid());
  EXPECT_EQ(first_type_, callee_transceivers[0]->media_type());
  EXPECT_EQ(second_mid, callee_transceivers[1]->mid());
  EXPECT_EQ(second_type_, callee_transceivers[1]->media_type());

  // The answer should have only one media section for the new transceiver.
  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);
  EXPECT_EQ(second_type_, answer_contents[0].media_description()->type());

  // Setting the local answer should succeed.
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));

  // Setting the remote answer should succeed and not create any new
  // transceivers.
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
  ASSERT_EQ(2u, caller->pc()->GetTransceivers().size());
  ASSERT_EQ(2u, callee->pc()->GetTransceivers().size());
}

// Test that recycling works properly when a new transceiver recycles an m=
// section that was rejected only in the current local description.
TEST_P(RecycleMediaSectionTest, CurrentLocalOnlyRejected) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  std::string first_mid = *caller_first_transceiver->mid();
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  callee_first_transceiver->Stop();

  // The answer will have a rejected m= section.
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // The offer should reuse the previous media section but allocate a new MID
  // and change the media type.
  auto callee_second_transceiver = callee->AddTransceiver(second_type_);
  auto offer = callee->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_EQ(second_type_, offer_contents[0].media_description()->type());
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  // Setting the local offer will dissociate the previous transceiver and set
  // the MID for the new transceiver.
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, callee_first_transceiver->mid());
  EXPECT_EQ(second_mid, callee_second_transceiver->mid());

  // Setting the remote offer will dissociate the previous transceiver and
  // create a new transceiver for the media section.
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(offer)));
  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, caller_transceivers.size());
  EXPECT_EQ(absl::nullopt, caller_transceivers[0]->mid());
  EXPECT_EQ(first_type_, caller_transceivers[0]->media_type());
  EXPECT_EQ(second_mid, caller_transceivers[1]->mid());
  EXPECT_EQ(second_type_, caller_transceivers[1]->media_type());

  // The answer should have only one media section for the new transceiver.
  auto answer = caller->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);
  EXPECT_EQ(second_type_, answer_contents[0].media_description()->type());

  // Setting the local answer should succeed.
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(answer.get())));

  // Setting the remote answer should succeed and not create any new
  // transceivers.
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(answer)));
  ASSERT_EQ(2u, callee->pc()->GetTransceivers().size());
  ASSERT_EQ(2u, caller->pc()->GetTransceivers().size());
}

// Test that a m= section is *not* recycled if the media section is only
// rejected in the pending local description and there is no current remote
// description.
TEST_P(RecycleMediaSectionTest, PendingLocalRejectedAndNoRemote) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  std::string first_mid = *caller_first_transceiver->mid();
  caller_first_transceiver->Stop();

  // The reoffer will have a rejected m= section.
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto caller_second_transceiver = caller->AddTransceiver(second_type_);

  // The reoffer should not recycle the existing m= section since it is not
  // rejected in either the *current* local or *current* remote description.
  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(caller->SetLocalDescription(std::move(reoffer)));

  // Both RtpTransceivers are associated.
  EXPECT_EQ(first_mid, caller_first_transceiver->mid());
  EXPECT_EQ(second_mid, caller_second_transceiver->mid());
}

// Test that a m= section is *not* recycled if the media section is only
// rejected in the pending local description and not rejected in the current
// remote description.
TEST_P(RecycleMediaSectionTest, PendingLocalRejectedAndNotRejectedRemote) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  std::string first_mid = *caller_first_transceiver->mid();
  caller_first_transceiver->Stop();

  // The reoffer will have a rejected m= section.
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto caller_second_transceiver = caller->AddTransceiver(second_type_);

  // The reoffer should not recycle the existing m= section since it is not
  // rejected in either the *current* local or *current* remote description.
  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(caller->SetLocalDescription(std::move(reoffer)));

  // Both RtpTransceivers are associated.
  EXPECT_EQ(first_mid, caller_first_transceiver->mid());
  EXPECT_EQ(second_mid, caller_second_transceiver->mid());
}

// Test that an m= section is *not* recycled if the media section is only
// rejected in the pending remote description and there is no current local
// description.
TEST_P(RecycleMediaSectionTest, PendingRemoteRejectedAndNoLocal) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  std::string first_mid = *callee_first_transceiver->mid();
  caller_first_transceiver->Stop();

  // The reoffer will have a rejected m= section.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto callee_second_transceiver = callee->AddTransceiver(second_type_);

  // The reoffer should not recycle the existing m= section since it is not
  // rejected in either the *current* local or *current* remote description.
  auto reoffer = callee->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

  // Note: Cannot actually set the reoffer since the callee is in the signaling
  // state 'have-remote-offer'.
}

// Test that an m= section is *not* recycled if the media section is only
// rejected in the pending remote description and not rejected in the current
// local description.
TEST_P(RecycleMediaSectionTest, PendingRemoteRejectedAndNotRejectedLocal) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  std::string first_mid = *callee_first_transceiver->mid();
  caller_first_transceiver->Stop();

  // The reoffer will have a rejected m= section.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto callee_second_transceiver = callee->AddTransceiver(second_type_);

  // The reoffer should not recycle the existing m= section since it is not
  // rejected in either the *current* local or *current* remote description.
  auto reoffer = callee->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

  // Note: Cannot actually set the reoffer since the callee is in the signaling
  // state 'have-remote-offer'.
}

// Test all combinations of audio and video as the first and second media type
// for the media section. This is needed for full test coverage because
// MediaSession has separate functions for processing audio and video media
// sections.
INSTANTIATE_TEST_CASE_P(
    PeerConnectionJsepTest,
    RecycleMediaSectionTest,
    Combine(Values(cricket::MEDIA_TYPE_AUDIO, cricket::MEDIA_TYPE_VIDEO),
            Values(cricket::MEDIA_TYPE_AUDIO, cricket::MEDIA_TYPE_VIDEO)));

// Test that a new data channel section will not reuse a recycleable audio or
// video media section. Additionally, tests that the new section is added to the
// end of the session description.
TEST_F(PeerConnectionJsepTest, DataChannelDoesNotRecycleMediaSection) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  transceiver->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->CreateDataChannel("dc");

  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(2u, offer_contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO,
            offer_contents[0].media_description()->type());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA,
            offer_contents[1].media_description()->type());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(2u, answer_contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO,
            answer_contents[0].media_description()->type());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA,
            answer_contents[1].media_description()->type());
}

// Test that if a new track is added to an existing session that has a data,
// the new section comes at the end of the new offer, after the existing data
// section.
TEST_F(PeerConnectionJsepTest, AudioTrackAddedAfterDataSectionInReoffer) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("dc");
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->AddAudioTrack("a");

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA, contents[0].media_description()->type());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, contents[1].media_description()->type());
}

// Tests for MID properties.

static void RenameSection(size_t mline_index,
                          const std::string& new_mid,
                          SessionDescriptionInterface* sdesc) {
  cricket::SessionDescription* desc = sdesc->description();
  std::string old_mid = desc->contents()[mline_index].name;
  desc->contents()[mline_index].name = new_mid;
  desc->transport_infos()[mline_index].content_name = new_mid;
  const cricket::ContentGroup* bundle =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  if (bundle) {
    cricket::ContentGroup new_bundle = *bundle;
    if (new_bundle.RemoveContentName(old_mid)) {
      new_bundle.AddContentName(new_mid);
    }
    desc->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
    desc->AddGroup(new_bundle);
  }
}

// Test that two PeerConnections can have a successful offer/answer exchange if
// the MIDs are changed from the defaults.
TEST_F(PeerConnectionJsepTest, OfferAnswerWithChangedMids) {
  constexpr char kFirstMid[] = "nondefaultmid";
  constexpr char kSecondMid[] = "randommid";

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  caller->AddAudioTrack("b");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  RenameSection(0, kFirstMid, offer.get());
  RenameSection(1, kSecondMid, offer.get());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  auto caller_transceivers = caller->pc()->GetTransceivers();
  EXPECT_EQ(kFirstMid, caller_transceivers[0]->mid());
  EXPECT_EQ(kSecondMid, caller_transceivers[1]->mid());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto callee_transceivers = callee->pc()->GetTransceivers();
  EXPECT_EQ(kFirstMid, callee_transceivers[0]->mid());
  EXPECT_EQ(kSecondMid, callee_transceivers[1]->mid());

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  EXPECT_EQ(kFirstMid, answer_contents[0].name);
  EXPECT_EQ(kSecondMid, answer_contents[1].name);

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

// Test that CreateOffer will generate a MID that is not already used if the
// default it would have picked is already taken. This is tested by using a
// third PeerConnection to determine what the default would be for the second
// media section then setting that as the first media section's MID.
TEST_F(PeerConnectionJsepTest, CreateOfferGeneratesUniqueMidIfAlreadyTaken) {
  // First, find what the default MID is for the second media section.
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("a");
  pc->AddAudioTrack("b");
  auto default_offer = pc->CreateOffer();
  std::string default_second_mid =
      default_offer->description()->contents()[1].name;

  // Now, do an offer/answer with one track which has the MID set to the default
  // second MID.
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  RenameSection(0, default_second_mid, offer.get());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  // Add a second track and ensure that the MID is different.
  caller->AddAudioTrack("b");

  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  EXPECT_EQ(default_second_mid, reoffer_contents[0].name);
  EXPECT_NE(reoffer_contents[0].name, reoffer_contents[1].name);
}

// Test that if an audio or video section has the default data section MID, then
// CreateOffer will generate a unique MID for the newly added data section.
TEST_F(PeerConnectionJsepTest,
       CreateOfferGeneratesUniqueMidForDataSectionIfAlreadyTaken) {
  // First, find what the default MID is for the data channel.
  auto pc = CreatePeerConnection();
  pc->CreateDataChannel("dc");
  auto default_offer = pc->CreateOffer();
  std::string default_data_mid =
      default_offer->description()->contents()[0].name;

  // Now do an offer/answer with one audio track which has a MID set to the
  // default data MID.
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  RenameSection(0, default_data_mid, offer.get());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  // Add a data channel and ensure that the MID is different.
  caller->CreateDataChannel("dc");

  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  EXPECT_EQ(default_data_mid, reoffer_contents[0].name);
  EXPECT_NE(reoffer_contents[0].name, reoffer_contents[1].name);
}

// Test that a reoffer initiated by the callee adds a new track to the caller.
TEST_F(PeerConnectionJsepTest, CalleeDoesReoffer) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->AddVideoTrack("v");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  EXPECT_EQ(1u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(2u, callee->pc()->GetTransceivers().size());

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));

  EXPECT_EQ(2u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(2u, callee->pc()->GetTransceivers().size());
}

// Tests for MSID properties.

// Test that adding a track with AddTrack results in an offer that signals the
// track's ID.
TEST_F(PeerConnectionJsepTest, AddingTrackWithAddTrackSpecifiesTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack(kTrackId);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto streams = contents[0].media_description()->streams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(kTrackId, streams[0].id);
}

// Test that adding a track by calling AddTransceiver then SetTrack results in
// an offer that does not signal the track's ID and signals a random ID.
TEST_F(PeerConnectionJsepTest,
       AddingTrackWithAddTransceiverSpecifiesRandomTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->sender()->SetTrack(caller->CreateAudioTrack(kTrackId));

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto streams = contents[0].media_description()->streams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_NE(kTrackId, streams[0].id);
}

// Test that if the transceiver is recvonly or inactive, then no MSID
// information is included in the offer.
TEST_F(PeerConnectionJsepTest, NoMsidInOfferIfTransceiverDirectionHasNoSend) {
  auto caller = CreatePeerConnection();

  RtpTransceiverInit init_recvonly;
  init_recvonly.direction = RtpTransceiverDirection::kRecvOnly;
  ASSERT_TRUE(caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init_recvonly));

  RtpTransceiverInit init_inactive;
  init_inactive.direction = RtpTransceiverDirection::kInactive;
  ASSERT_TRUE(caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init_inactive));

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  // MSID is specified in the first stream, so no streams means no MSID.
  EXPECT_EQ(0u, contents[0].media_description()->streams().size());
  EXPECT_EQ(0u, contents[1].media_description()->streams().size());
}

// Test that if an answer negotiates transceiver directions of recvonly or
// inactive, then no MSID information is included in the answer.
TEST_F(PeerConnectionJsepTest, NoMsidInAnswerIfNoRespondingTracks) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  // recvonly transceiver will get negotiated to inactive since the callee has
  // no tracks to send in response.
  RtpTransceiverInit init_recvonly;
  init_recvonly.direction = RtpTransceiverDirection::kRecvOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init_recvonly);

  // sendrecv transceiver will get negotiated to recvonly since the callee has
  // no tracks to send in response.
  RtpTransceiverInit init_sendrecv;
  init_sendrecv.direction = RtpTransceiverDirection::kSendRecv;
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init_sendrecv);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  // MSID is specified in the first stream, so no streams means no MSID.
  EXPECT_EQ(0u, contents[0].media_description()->streams().size());
  EXPECT_EQ(0u, contents[1].media_description()->streams().size());
}

// Test that the MSID is included even if the transceiver direction has changed
// to inactive if the transceiver had previously sent media.
TEST_F(PeerConnectionJsepTest, IncludeMsidEvenIfDirectionHasChanged) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->pc()->GetTransceivers()[0]->SetDirection(
      RtpTransceiverDirection::kInactive);

  // The transceiver direction on both sides will turn to inactive.
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  // MSID is specified in the first stream. If it is present, assume that MSID
  // is there.
  EXPECT_EQ(1u, offer_contents[0].media_description()->streams().size());

  auto* answer = caller->pc()->remote_description();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_EQ(1u, answer_contents[0].media_description()->streams().size());
}

// Test that stopping a RtpTransceiver will cause future offers to not include
// any MSID information for that section.
TEST_F(PeerConnectionJsepTest, RemoveMsidIfTransceiverStopped) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  transceiver->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  // MSID is specified in the first stream, so no streams means no MSID.
  EXPECT_EQ(0u, offer_contents[0].media_description()->streams().size());
}

// Test that the callee RtpReceiver created by a call to SetRemoteDescription
// has its ID set to the signaled track ID.
TEST_F(PeerConnectionJsepTest,
       RtpReceiverCreatedBySetRemoteDescriptionHasSignaledTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack(kTrackId);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->pc()->GetReceivers().size());
  auto receiver = callee->pc()->GetReceivers()[0];
  EXPECT_EQ(kTrackId, receiver->id());
}

// Test that if the callee RtpReceiver is reused by a call to
// SetRemoteDescription, its ID does not change.
TEST_F(PeerConnectionJsepTest,
       RtpReceiverCreatedBeforeSetRemoteDescriptionKeepsId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack(kTrackId);
  callee->AddAudioTrack("dummy_track");

  ASSERT_EQ(1u, callee->pc()->GetReceivers().size());
  auto receiver = callee->pc()->GetReceivers()[0];
  std::string receiver_id = receiver->id();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(receiver_id, receiver->id());
}

// Test that setting a remote offer with one track that has no streams fires off
// the correct OnAddTrack event.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithOneTrackNoStreamFiresOnAddTrack) {
  const std::string kTrackLabel = "audio_track";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrackLabel));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(1u, track_events.size());
  const auto& event = track_events[0];
  EXPECT_EQ(kTrackLabel, event.receiver->track()->id());
  EXPECT_EQ(0u, event.streams.size());
}

// Test that setting a remote offer with one track that has one stream fires off
// the correct OnAddTrack event.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithOneTrackOneStreamFiresOnAddTrack) {
  const std::string kTrackLabel = "audio_track";
  const std::string kStreamId = "audio_stream";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrackLabel, {kStreamId}));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(1u, track_events.size());
  const auto& event = track_events[0];
  ASSERT_EQ(1u, event.streams.size());
  auto stream = event.streams[0];
  EXPECT_EQ(kStreamId, stream->id());
  EXPECT_THAT(track_events[0].snapshotted_stream_tracks.at(stream),
              ElementsAre(event.receiver->track()));
  EXPECT_EQ(event.receiver->streams(), track_events[0].streams);
}

// Test that setting a remote offer with two tracks that share the same stream
// fires off two OnAddTrack events, both with the same stream that has both
// tracks present at the time of firing. This is to ensure that track events are
// not fired until SetRemoteDescription has finished processing all the media
// sections.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithTwoTracksSameStreamFiresOnAddTrack) {
  const std::string kTrack1Label = "audio_track1";
  const std::string kTrack2Label = "audio_track2";
  const std::string kSharedStreamId = "stream";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrack1Label, {kSharedStreamId}));
  ASSERT_TRUE(caller->AddAudioTrack(kTrack2Label, {kSharedStreamId}));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(2u, track_events.size());
  const auto& event1 = track_events[0];
  const auto& event2 = track_events[1];
  ASSERT_EQ(1u, event1.streams.size());
  auto stream = event1.streams[0];
  ASSERT_THAT(event2.streams, ElementsAre(stream));
  auto track1 = event1.receiver->track();
  auto track2 = event2.receiver->track();
  EXPECT_THAT(event1.snapshotted_stream_tracks.at(stream),
              UnorderedElementsAre(track1, track2));
  EXPECT_THAT(event2.snapshotted_stream_tracks.at(stream),
              UnorderedElementsAre(track1, track2));
}

// Test that setting a remote offer with one track that has two streams fires
// off the correct OnAddTrack event.
TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithOneTrackTwoStreamFiresOnAddTrack) {
  const std::string kTrackLabel = "audio_track";
  const std::string kStreamId1 = "audio_stream1";
  const std::string kStreamId2 = "audio_stream2";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrackLabel, {kStreamId1, kStreamId2}));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(1u, track_events.size());
  const auto& event = track_events[0];
  ASSERT_EQ(2u, event.streams.size());
  EXPECT_EQ(kStreamId1, event.streams[0]->id());
  EXPECT_EQ(kStreamId2, event.streams[1]->id());
}

// Test that if an RtpTransceiver with a current_direction set is stopped, then
// current_direction is changed to null.
TEST_F(PeerConnectionJsepTest, CurrentDirectionResetWhenRtpTransceiverStopped) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_TRUE(transceiver->current_direction());
  transceiver->Stop();
  EXPECT_FALSE(transceiver->current_direction());
}

// Test that you can't set an answer on a PeerConnection before setting the
// offer.
TEST_F(PeerConnectionJsepTest, AnswerBeforeOfferFails) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  RTCError error;
  ASSERT_FALSE(caller->SetRemoteDescription(callee->CreateAnswer(), &error));
  EXPECT_EQ(RTCErrorType::INVALID_STATE, error.type());
}

// Test that a Unified Plan PeerConnection fails to set a Plan B offer if it has
// two video tracks.
TEST_F(PeerConnectionJsepTest, TwoVideoPlanBToUnifiedPlanFails) {
  RTCConfiguration config_planb;
  config_planb.sdp_semantics = SdpSemantics::kPlanB;
  auto caller = CreatePeerConnection(config_planb);
  auto callee = CreatePeerConnection();
  caller->AddVideoTrack("video1");
  caller->AddVideoTrack("video2");

  RTCError error;
  ASSERT_FALSE(callee->SetRemoteDescription(caller->CreateOffer(), &error));
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.type());
}

// Test that a Unified Plan PeerConnection fails to set a Plan B answer if it
// has two video tracks.
TEST_F(PeerConnectionJsepTest, OneVideoUnifiedPlanToTwoVideoPlanBFails) {
  auto caller = CreatePeerConnection();
  RTCConfiguration config_planb;
  config_planb.sdp_semantics = SdpSemantics::kPlanB;
  auto callee = CreatePeerConnection(config_planb);
  caller->AddVideoTrack("video");
  callee->AddVideoTrack("video1");
  callee->AddVideoTrack("video2");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCError error;
  ASSERT_FALSE(caller->SetRemoteDescription(caller->CreateAnswer(), &error));
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.type());
}

}  // namespace webrtc
