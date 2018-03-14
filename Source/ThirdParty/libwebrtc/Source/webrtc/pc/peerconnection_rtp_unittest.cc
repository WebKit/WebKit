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
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/jsep.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "pc/mediastream.h"
#include "pc/mediastreamtrack.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"

// This file contains tests for RTP Media API-related behavior of
// |webrtc::PeerConnection|, see https://w3c.github.io/webrtc-pc/#rtp-media-api.

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

const uint32_t kDefaultTimeout = 10000u;

template <typename MethodFunctor>
class OnSuccessObserver : public rtc::RefCountedObject<
                              webrtc::SetRemoteDescriptionObserverInterface> {
 public:
  explicit OnSuccessObserver(MethodFunctor on_success)
      : on_success_(std::move(on_success)) {}

  // webrtc::SetRemoteDescriptionObserverInterface implementation.
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    RTC_CHECK(error.ok());
    on_success_();
  }

 private:
  MethodFunctor on_success_;
};

class PeerConnectionRtpTest : public testing::Test {
 public:
  PeerConnectionRtpTest()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWithUnifiedPlan() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      const RTCConfiguration& config) {
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(config, nullptr, nullptr,
                                                observer.get());
    return rtc::MakeUnique<PeerConnectionWrapper>(pc_factory_, pc,
                                                  std::move(observer));
  }

 protected:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

// These tests cover |webrtc::PeerConnectionObserver| callbacks firing upon
// setting the remote description.
class PeerConnectionRtpCallbacksTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionRtpCallbacksTest, AddTrackWithoutStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track")));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  // TODO(hbos): When "no stream" is handled correctly we would expect
  // |add_track_events_[0].streams| to be empty. https://crbug.com/webrtc/7933
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_F(PeerConnectionRtpCallbacksTest, AddTrackWithStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                               {"audio_stream"}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_EQ("audio_stream", add_track_event.streams[0]->label());
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithoutStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"), {});
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                                 {"audio_stream"});
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithSharedStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kSharedStreamLabel[] = "shared_audio_stream";
  auto sender1 = caller->AddTrack(caller->CreateAudioTrack("audio_track1"),
                                  {kSharedStreamLabel});
  auto sender2 = caller->AddTrack(caller->CreateAudioTrack("audio_track2"),
                                  {kSharedStreamLabel});
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          callee->observer()->add_track_events_[0].receiver},
      callee->observer()->remove_track_events_);

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

// These tests examine the state of the peer connection as a result of
// performing SetRemoteDescription().
class PeerConnectionRtpObserverTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionRtpObserverTest, AddSenderWithoutStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track"), {}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  // TODO(hbos): When "no stream" is handled correctly we would expect
  // |receiver_added->streams()| to be empty. https://crbug.com/webrtc/7933
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_F(PeerConnectionRtpObserverTest, AddSenderWithStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                               {"audio_stream"}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_EQ("audio_stream", receiver_added->streams()[0]->label());
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_F(PeerConnectionRtpObserverTest,
       RemoveSenderWithoutStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"), {});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

TEST_F(PeerConnectionRtpObserverTest, RemoveSenderWithStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                                 {"audio_stream"});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

TEST_F(PeerConnectionRtpObserverTest,
       RemoveSenderWithSharedStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kSharedStreamLabel[] = "shared_audio_stream";
  auto sender1 = caller->AddTrack(caller->CreateAudioTrack("audio_track1"),
                                  {kSharedStreamLabel});
  auto sender2 = caller->AddTrack(caller->CreateAudioTrack("audio_track2"),
                                  {kSharedStreamLabel});
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->pc()->GetReceivers().size(), 2u);
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver1;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver2;
  if (callee->pc()->GetReceivers()[0]->track()->id() == "audio_track1") {
    receiver1 = callee->pc()->GetReceivers()[0];
    receiver2 = callee->pc()->GetReceivers()[1];
  } else {
    receiver1 = callee->pc()->GetReceivers()[1];
    receiver2 = callee->pc()->GetReceivers()[0];
  }
  EXPECT_EQ("audio_track1", receiver1->track()->id());
  EXPECT_EQ("audio_track2", receiver2->track()->id());

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  // Only |receiver2| should remain.
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>{receiver2},
      callee->pc()->GetReceivers());

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

// Invokes SetRemoteDescription() twice in a row without synchronizing the two
// calls and examine the state of the peer connection inside the callbacks to
// ensure that the second call does not occur prematurely, contaminating the
// state of the peer connection of the first callback.
TEST_F(PeerConnectionRtpObserverTest,
       StatesCorrelateWithSetRemoteDescriptionCall) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  // Create SDP for adding a track and for removing it. This will be used in the
  // first and second SetRemoteDescription() calls.
  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"), {});
  auto srd1_sdp = caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  auto srd2_sdp = caller->CreateOfferAndSetAsLocal();

  // In the first SetRemoteDescription() callback, check that we have a
  // receiver for the track.
  auto pc = callee->pc();
  bool srd1_callback_called = false;
  auto srd1_callback = [&srd1_callback_called, &pc]() {
    EXPECT_EQ(pc->GetReceivers().size(), 1u);
    srd1_callback_called = true;
  };

  // In the second SetRemoteDescription() callback, check that the receiver has
  // been removed.
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  // https://crbug.com/webrtc/7600
  bool srd2_callback_called = false;
  auto srd2_callback = [&srd2_callback_called, &pc]() {
    EXPECT_TRUE(pc->GetReceivers().empty());
    srd2_callback_called = true;
  };

  // Invoke SetRemoteDescription() twice in a row without synchronizing the two
  // calls. The callbacks verify that the two calls are synchronized, as in, the
  // effects of the second SetRemoteDescription() call must not have happened by
  // the time the first callback is invoked. If it has then the receiver that is
  // added as a result of the first SetRemoteDescription() call will already
  // have been removed as a result of the second SetRemoteDescription() call
  // when the first callback is invoked.
  callee->pc()->SetRemoteDescription(
      std::move(srd1_sdp),
      new OnSuccessObserver<decltype(srd1_callback)>(srd1_callback));
  callee->pc()->SetRemoteDescription(
      std::move(srd2_sdp),
      new OnSuccessObserver<decltype(srd2_callback)>(srd2_callback));
  EXPECT_TRUE_WAIT(srd1_callback_called, kDefaultTimeout);
  EXPECT_TRUE_WAIT(srd2_callback_called, kDefaultTimeout);
}

// Tests for the legacy SetRemoteDescription() function signature.
class PeerConnectionRtpLegacyObserverTest : public PeerConnectionRtpTest {};

// Sanity test making sure the callback is invoked.
TEST_F(PeerConnectionRtpLegacyObserverTest, OnSuccess) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  std::string error;
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(), &error));
}

// Verifies legacy behavior: The observer is not called if if the peer
// connection is destroyed because the asynchronous callback is executed in the
// peer connection's message handler.
TEST_F(PeerConnectionRtpLegacyObserverTest,
       ObserverNotCalledIfPeerConnectionDereferenced) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::MockSetSessionDescriptionObserver> observer =
      new rtc::RefCountedObject<webrtc::MockSetSessionDescriptionObserver>();

  auto offer = caller->CreateOfferAndSetAsLocal();
  callee->pc()->SetRemoteDescription(observer, offer.release());
  callee = nullptr;
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_FALSE(observer->called());
}

// RtpTransceiver Tests.

// Test that by default there are no transceivers with Unified Plan.
TEST_F(PeerConnectionRtpTest, PeerConnectionHasNoTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
}

// Test that a transceiver created with the audio kind has the correct initial
// properties.
TEST_F(PeerConnectionRtpTest, AddTransceiverHasCorrectInitProperties) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(rtc::nullopt, transceiver->mid());
  EXPECT_FALSE(transceiver->stopped());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
  EXPECT_EQ(rtc::nullopt, transceiver->current_direction());
}

// Test that adding a transceiver with the audio kind creates an audio sender
// and audio receiver with the receiver having a live audio track.
TEST_F(PeerConnectionRtpTest,
       AddAudioTransceiverCreatesAudioSenderAndReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

// Test that adding a transceiver with the video kind creates an video sender
// and video receiver with the receiver having a live video track.
TEST_F(PeerConnectionRtpTest,
       AddAudioTransceiverCreatesVideoSenderAndReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kVideoKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

// Test that after a call to AddTransceiver, the transceiver shows in
// GetTransceivers(), the transceiver's sender shows in GetSenders(), and the
// transceiver's receiver shows in GetReceivers().
TEST_F(PeerConnectionRtpTest, AddTransceiverShowsInLists) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>{transceiver},
      caller->pc()->GetTransceivers());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpSenderInterface>>{
          transceiver->sender()},
      caller->pc()->GetSenders());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          transceiver->receiver()},
      caller->pc()->GetReceivers());
}

// Test that the direction passed in through the AddTransceiver init parameter
// is set in the returned transceiver.
TEST_F(PeerConnectionRtpTest, AddTransceiverWithDirectionIsReflected) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

// Test that calling AddTransceiver with a track creates a transceiver which has
// its sender's track set to the passed-in track.
TEST_F(PeerConnectionRtpTest, AddTransceiverWithTrackCreatesSenderWithTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("audio track");
  auto transceiver = caller->AddTransceiver(audio_track);

  auto sender = transceiver->sender();
  ASSERT_TRUE(sender->track());
  EXPECT_EQ(audio_track, sender->track());

  auto receiver = transceiver->receiver();
  ASSERT_TRUE(receiver->track());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, receiver->track()->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive,
            receiver->track()->state());
}

// Test that calling AddTransceiver twice with the same track creates distinct
// transceivers, senders with the same track.
TEST_F(PeerConnectionRtpTest,
       AddTransceiverTwiceWithSameTrackCreatesMultipleTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("audio track");

  auto transceiver1 = caller->AddTransceiver(audio_track);
  auto transceiver2 = caller->AddTransceiver(audio_track);

  EXPECT_NE(transceiver1, transceiver2);

  auto sender1 = transceiver1->sender();
  auto sender2 = transceiver2->sender();
  EXPECT_NE(sender1, sender2);
  EXPECT_EQ(audio_track, sender1->track());
  EXPECT_EQ(audio_track, sender2->track());

  EXPECT_THAT(caller->pc()->GetTransceivers(),
              UnorderedElementsAre(transceiver1, transceiver2));
  EXPECT_THAT(caller->pc()->GetSenders(),
              UnorderedElementsAre(sender1, sender2));
}

// RtpTransceiver error handling tests.

TEST_F(PeerConnectionRtpTest, AddTransceiverWithInvalidKindReturnsError) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto result = caller->pc()->AddTransceiver(cricket::MEDIA_TYPE_DATA);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

TEST_F(PeerConnectionRtpTest, UnifiedPlanCanClosePeerConnection) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  caller->pc()->Close();
}

// Unified Plan AddTrack tests.

class PeerConnectionRtpUnifiedPlanTest : public PeerConnectionRtpTest {};

// Test that adding an audio track creates a new audio RtpSender with the given
// track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddAudioTrackCreatesAudioSender) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("a");
  auto sender = caller->AddTrack(audio_track);
  ASSERT_TRUE(sender);

  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, sender->media_type());
  EXPECT_EQ(audio_track, sender->track());
}

// Test that adding a video track creates a new video RtpSender with the given
// track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddVideoTrackCreatesVideoSender) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto video_track = caller->CreateVideoTrack("a");
  auto sender = caller->AddTrack(video_track);
  ASSERT_TRUE(sender);

  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, sender->media_type());
  EXPECT_EQ(video_track, sender->track());
}

// Test that adding a track to a new PeerConnection creates an RtpTransceiver
// with the sender that AddTrack returns and in the sendrecv direction.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddFirstTrackCreatesTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(sender, transceivers[0]->sender());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceivers[0]->direction());
}

// Test that if a transceiver of the same type but no track had been added to
// the PeerConnection and later a call to AddTrack is made, the resulting sender
// is the transceiver's sender and the sender's track is the newly-added track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackReusesTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto audio_track = caller->CreateAudioTrack("a");
  auto sender = caller->AddTrack(audio_track);
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(transceiver, transceivers[0]);
  EXPECT_EQ(sender, transceiver->sender());
  EXPECT_EQ(audio_track, sender->track());
}

// Test that adding two tracks to a new PeerConnection creates two
// RtpTransceivers in the same order.
TEST_F(PeerConnectionRtpUnifiedPlanTest, TwoAddTrackCreatesTwoTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender1 = caller->AddAudioTrack("a");
  auto sender2 = caller->AddVideoTrack("v");
  ASSERT_TRUE(sender2);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(sender1, transceivers[0]->sender());
  EXPECT_EQ(sender2, transceivers[1]->sender());
}

// Test that if there are multiple transceivers with no sending track then a
// later call to AddTrack will use the one of the same type as the newly-added
// track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackReusesTransceiverOfType) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto sender = caller->AddVideoTrack("v");

  ASSERT_EQ(2u, caller->pc()->GetTransceivers().size());
  EXPECT_NE(sender, audio_transceiver->sender());
  EXPECT_EQ(sender, video_transceiver->sender());
}

// Test that if the only transceivers that do not have a sending track have a
// different type from the added track, then AddTrack will create a new
// transceiver for the track.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackDoesNotReuseTransceiverOfWrongType) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto sender = caller->AddVideoTrack("v");

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_NE(sender, transceivers[0]->sender());
  EXPECT_EQ(sender, transceivers[1]->sender());
}

// Test that the first available transceiver is reused by AddTrack when multiple
// are available.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackReusesFirstMatchingTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto sender = caller->AddAudioTrack("a");

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(sender, transceivers[0]->sender());
  EXPECT_NE(sender, transceivers[1]->sender());
}

// Test that a call to AddTrack that reuses a transceiver will change the
// direction from inactive to sendonly.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackChangesDirectionFromInactiveToSendOnly) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kInactive;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->AddAudioTrack("a"));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

// Test that a call to AddTrack that reuses a transceiver will change the
// direction from recvonly to sendrecv.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackChangesDirectionFromRecvOnlyToSendRecv) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kRecvOnly;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->AddAudioTrack("a"));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
}

TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackCreatesSenderWithTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack(kTrackId);
  auto sender = caller->AddTrack(audio_track);

  EXPECT_EQ(kTrackId, sender->id());
}

// Unified Plan AddTrack error handling.

TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackErrorIfClosed) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("a");
  caller->pc()->Close();

  caller->observer()->clear_negotiation_needed();
  auto result = caller->pc()
                ->AddTrack(audio_track, std::vector<std::string>());
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.error().type());
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackErrorIfTrackAlreadyHasSender) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("a");
  ASSERT_TRUE(caller->AddTrack(audio_track));

  caller->observer()->clear_negotiation_needed();
  auto result = caller->pc()
                ->AddTrack(audio_track, std::vector<std::string>());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

// Unified Plan RemoveTrack tests.

// Test that calling RemoveTrack on a sender with a previously-added track
// clears the sender's track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, RemoveTrackClearsSenderTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));

  EXPECT_FALSE(sender->track());
}

// Test that calling RemoveTrack on a sender where the transceiver is configured
// in the sendrecv direction changes the transceiver's direction to recvonly.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RemoveTrackChangesDirectionFromSendRecvToRecvOnly) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendRecv;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->pc()->RemoveTrack(transceiver->sender()));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceiver->direction());
  EXPECT_TRUE(caller->observer()->renegotiation_needed_);
}

// Test that calling RemoveTrack on a sender where the transceiver is configured
// in the sendonly direction changes the transceiver's direction to inactive.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RemoveTrackChangesDirectionFromSendOnlyToInactive) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->pc()->RemoveTrack(transceiver->sender()));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kInactive, transceiver->direction());
}

// Test that calling RemoveTrack with a sender that has a null track results in
// no change in state.
TEST_F(PeerConnectionRtpUnifiedPlanTest, RemoveTrackWithNullSenderTrackIsNoOp) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  auto transceiver = caller->pc()->GetTransceivers()[0];
  ASSERT_TRUE(sender->SetTrack(nullptr));

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  EXPECT_FALSE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
}

// Unified Plan RemoveTrack error handling.

TEST_F(PeerConnectionRtpUnifiedPlanTest, RemoveTrackErrorIfClosed) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  caller->pc()->Close();

  caller->observer()->clear_negotiation_needed();
  EXPECT_FALSE(caller->pc()->RemoveTrack(sender));
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RemoveTrackNoErrorIfTrackAlreadyRemoved) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));

  caller->observer()->clear_negotiation_needed();
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

}  // namespace webrtc
