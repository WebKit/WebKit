/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/fakemetricsobserver.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/media/engine/fakewebrtcvideoengine.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "webrtc/p2p/base/testturnserver.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/pc/dtmfsender.h"
#include "webrtc/pc/localaudiosource.h"
#include "webrtc/pc/mediasession.h"
#include "webrtc/pc/peerconnection.h"
#include "webrtc/pc/peerconnectionfactory.h"
#include "webrtc/pc/test/fakeaudiocapturemodule.h"
#include "webrtc/pc/test/fakeperiodicvideocapturer.h"
#include "webrtc/pc/test/fakertccertificategenerator.h"
#include "webrtc/pc/test/fakevideotrackrenderer.h"
#include "webrtc/pc/test/mockpeerconnectionobservers.h"

using cricket::ContentInfo;
using cricket::FakeWebRtcVideoDecoder;
using cricket::FakeWebRtcVideoDecoderFactory;
using cricket::FakeWebRtcVideoEncoder;
using cricket::FakeWebRtcVideoEncoderFactory;
using cricket::MediaContentDescription;
using webrtc::DataBuffer;
using webrtc::DataChannelInterface;
using webrtc::DtmfSender;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::FakeConstraints;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::MockCreateSessionDescriptionObserver;
using webrtc::MockDataChannelObserver;
using webrtc::MockSetSessionDescriptionObserver;
using webrtc::MockStatsObserver;
using webrtc::ObserverInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionFactory;
using webrtc::SessionDescriptionInterface;
using webrtc::StreamCollectionInterface;

namespace {

static const int kMaxWaitMs = 10000;
// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
// This declaration is also #ifdef'd as it causes uninitialized-variable
// warnings.
#if !defined(THREAD_SANITIZER)
static const int kMaxWaitForStatsMs = 3000;
#endif
static const int kMaxWaitForActivationMs = 5000;
static const int kMaxWaitForFramesMs = 10000;
static const int kEndAudioFrameCount = 3;
static const int kEndVideoFrameCount = 3;

static const char kStreamLabelBase[] = "stream_label";
static const char kVideoTrackLabelBase[] = "video_track";
static const char kAudioTrackLabelBase[] = "audio_track";
static const char kDataChannelLabel[] = "data_channel";

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
// This declaration is also #ifdef'd as it causes unused-variable errors.
#if !defined(THREAD_SANITIZER)
// SRTP cipher name negotiated by the tests. This must be updated if the
// default changes.
static const int kDefaultSrtpCryptoSuite = rtc::SRTP_AES128_CM_SHA1_32;
static const int kDefaultSrtpCryptoSuiteGcm = rtc::SRTP_AEAD_AES_256_GCM;
#endif

// Used to simulate signaling ICE/SDP between two PeerConnections.
enum Message { MSG_SDP_MESSAGE, MSG_ICE_MESSAGE };

struct SdpMessage {
  std::string type;
  std::string msg;
};

struct IceMessage {
  std::string sdp_mid;
  int sdp_mline_index;
  std::string msg;
};

static void RemoveLinesFromSdp(const std::string& line_start,
                               std::string* sdp) {
  const char kSdpLineEnd[] = "\r\n";
  size_t ssrc_pos = 0;
  while ((ssrc_pos = sdp->find(line_start, ssrc_pos)) !=
      std::string::npos) {
    size_t end_ssrc = sdp->find(kSdpLineEnd, ssrc_pos);
    sdp->erase(ssrc_pos, end_ssrc - ssrc_pos + strlen(kSdpLineEnd));
  }
}

bool StreamsHaveAudioTrack(StreamCollectionInterface* streams) {
  for (size_t idx = 0; idx < streams->count(); idx++) {
    auto stream = streams->at(idx);
    if (stream->GetAudioTracks().size() > 0) {
      return true;
    }
  }
  return false;
}

bool StreamsHaveVideoTrack(StreamCollectionInterface* streams) {
  for (size_t idx = 0; idx < streams->count(); idx++) {
    auto stream = streams->at(idx);
    if (stream->GetVideoTracks().size() > 0) {
      return true;
    }
  }
  return false;
}

class SignalingMessageReceiver {
 public:
  virtual void ReceiveSdpMessage(const std::string& type,
                                 std::string& msg) = 0;
  virtual void ReceiveIceMessage(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& msg) = 0;

 protected:
  SignalingMessageReceiver() {}
  virtual ~SignalingMessageReceiver() {}
};

class MockRtpReceiverObserver : public webrtc::RtpReceiverObserverInterface {
 public:
  MockRtpReceiverObserver(cricket::MediaType media_type)
      : expected_media_type_(media_type) {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override {
    ASSERT_EQ(expected_media_type_, media_type);
    first_packet_received_ = true;
  }

  bool first_packet_received() { return first_packet_received_; }

  virtual ~MockRtpReceiverObserver() {}

 private:
  bool first_packet_received_ = false;
  cricket::MediaType expected_media_type_;
};

class PeerConnectionTestClient : public webrtc::PeerConnectionObserver,
                                 public SignalingMessageReceiver,
                                 public ObserverInterface,
                                 public rtc::MessageHandler {
 public:
  // We need these using declarations because there are two versions of each of
  // the below methods and we only override one of them.
  // TODO(deadbeef): Remove once there's only one version of the methods.
  using PeerConnectionObserver::OnAddStream;
  using PeerConnectionObserver::OnRemoveStream;
  using PeerConnectionObserver::OnDataChannel;

  // If |config| is not provided, uses a default constructed RTCConfiguration.
  static PeerConnectionTestClient* CreateClientWithDtlsIdentityStore(
      const std::string& id,
      const MediaConstraintsInterface* constraints,
      const PeerConnectionFactory::Options* options,
      const PeerConnectionInterface::RTCConfiguration* config,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      bool prefer_constraint_apis,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    PeerConnectionTestClient* client(new PeerConnectionTestClient(id));
    if (!client->Init(constraints, options, config, std::move(cert_generator),
                      prefer_constraint_apis, network_thread, worker_thread)) {
      delete client;
      return nullptr;
    }
    return client;
  }

  static PeerConnectionTestClient* CreateClient(
      const std::string& id,
      const MediaConstraintsInterface* constraints,
      const PeerConnectionFactory::Options* options,
      const PeerConnectionInterface::RTCConfiguration* config,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());

    return CreateClientWithDtlsIdentityStore(id, constraints, options, config,
                                             std::move(cert_generator), true,
                                             network_thread, worker_thread);
  }

  static PeerConnectionTestClient* CreateClientPreferNoConstraints(
      const std::string& id,
      const PeerConnectionFactory::Options* options,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());

    return CreateClientWithDtlsIdentityStore(id, nullptr, options, nullptr,
                                             std::move(cert_generator), false,
                                             network_thread, worker_thread);
  }

  ~PeerConnectionTestClient() {
  }

  void Negotiate() { Negotiate(true, true); }

  void Negotiate(bool audio, bool video) {
    std::unique_ptr<SessionDescriptionInterface> offer;
    ASSERT_TRUE(DoCreateOffer(&offer));

    if (offer->description()->GetContentByName("audio")) {
      offer->description()->GetContentByName("audio")->rejected = !audio;
    }
    if (offer->description()->GetContentByName("video")) {
      offer->description()->GetContentByName("video")->rejected = !video;
    }

    std::string sdp;
    EXPECT_TRUE(offer->ToString(&sdp));
    EXPECT_TRUE(DoSetLocalDescription(offer.release()));
    SendSdpMessage(webrtc::SessionDescriptionInterface::kOffer, sdp);
  }

  void SendSdpMessage(const std::string& type, std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      if (signaling_message_receiver_) {
        signaling_message_receiver_->ReceiveSdpMessage(type, msg);
      }
    } else {
      rtc::Thread::Current()->PostDelayed(
          RTC_FROM_HERE, signaling_delay_ms_, this, MSG_SDP_MESSAGE,
          new rtc::TypedMessageData<SdpMessage>({type, msg}));
    }
  }

  void SendIceMessage(const std::string& sdp_mid,
                      int sdp_mline_index,
                      const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      if (signaling_message_receiver_) {
        signaling_message_receiver_->ReceiveIceMessage(sdp_mid, sdp_mline_index,
                                                       msg);
      }
    } else {
      rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, signaling_delay_ms_,
                                          this, MSG_ICE_MESSAGE,
                                          new rtc::TypedMessageData<IceMessage>(
                                              {sdp_mid, sdp_mline_index, msg}));
    }
  }

  // MessageHandler callback.
  void OnMessage(rtc::Message* msg) override {
    switch (msg->message_id) {
      case MSG_SDP_MESSAGE: {
        auto sdp_message =
            static_cast<rtc::TypedMessageData<SdpMessage>*>(msg->pdata);
        if (signaling_message_receiver_) {
          signaling_message_receiver_->ReceiveSdpMessage(
              sdp_message->data().type, sdp_message->data().msg);
        }
        delete sdp_message;
        break;
      }
      case MSG_ICE_MESSAGE: {
        auto ice_message =
            static_cast<rtc::TypedMessageData<IceMessage>*>(msg->pdata);
        if (signaling_message_receiver_) {
          signaling_message_receiver_->ReceiveIceMessage(
              ice_message->data().sdp_mid, ice_message->data().sdp_mline_index,
              ice_message->data().msg);
        }
        delete ice_message;
        break;
      }
      default:
        RTC_CHECK(false);
    }
  }

  // SignalingMessageReceiver callback.
  void ReceiveSdpMessage(const std::string& type, std::string& msg) override {
    FilterIncomingSdpMessage(&msg);
    if (type == webrtc::SessionDescriptionInterface::kOffer) {
      HandleIncomingOffer(msg);
    } else {
      HandleIncomingAnswer(msg);
    }
  }

  // SignalingMessageReceiver callback.
  void ReceiveIceMessage(const std::string& sdp_mid,
                         int sdp_mline_index,
                         const std::string& msg) override {
    LOG(INFO) << id_ << "ReceiveIceMessage";
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, msg, nullptr));
    EXPECT_TRUE(pc()->AddIceCandidate(candidate.get()));
  }

  // PeerConnectionObserver callbacks.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    EXPECT_EQ(pc()->signaling_state(), new_state);
  }
  void OnAddStream(
      rtc::scoped_refptr<MediaStreamInterface> media_stream) override {
    media_stream->RegisterObserver(this);
    for (size_t i = 0; i < media_stream->GetVideoTracks().size(); ++i) {
      const std::string id = media_stream->GetVideoTracks()[i]->id();
      ASSERT_TRUE(fake_video_renderers_.find(id) ==
                  fake_video_renderers_.end());
      fake_video_renderers_[id].reset(new webrtc::FakeVideoTrackRenderer(
          media_stream->GetVideoTracks()[i]));
    }
  }
  void OnRemoveStream(
      rtc::scoped_refptr<MediaStreamInterface> media_stream) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    EXPECT_EQ(pc()->ice_connection_state(), new_state);
  }
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    EXPECT_EQ(pc()->ice_gathering_state(), new_state);
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    LOG(INFO) << id_ << "OnIceCandidate";

    std::string ice_sdp;
    EXPECT_TRUE(candidate->ToString(&ice_sdp));
    if (signaling_message_receiver_ == nullptr) {
      // Remote party may be deleted.
      return;
    }
    SendIceMessage(candidate->sdp_mid(), candidate->sdp_mline_index(), ice_sdp);
  }

  // MediaStreamInterface callback
  void OnChanged() override {
    // Track added or removed from MediaStream, so update our renderers.
    rtc::scoped_refptr<StreamCollectionInterface> remote_streams =
        pc()->remote_streams();
    // Remove renderers for tracks that were removed.
    for (auto it = fake_video_renderers_.begin();
         it != fake_video_renderers_.end();) {
      if (remote_streams->FindVideoTrack(it->first) == nullptr) {
        auto to_remove = it++;
        removed_fake_video_renderers_.push_back(std::move(to_remove->second));
        fake_video_renderers_.erase(to_remove);
      } else {
        ++it;
      }
    }
    // Create renderers for new video tracks.
    for (size_t stream_index = 0; stream_index < remote_streams->count();
         ++stream_index) {
      MediaStreamInterface* remote_stream = remote_streams->at(stream_index);
      for (size_t track_index = 0;
           track_index < remote_stream->GetVideoTracks().size();
           ++track_index) {
        const std::string id =
            remote_stream->GetVideoTracks()[track_index]->id();
        if (fake_video_renderers_.find(id) != fake_video_renderers_.end()) {
          continue;
        }
        fake_video_renderers_[id].reset(new webrtc::FakeVideoTrackRenderer(
            remote_stream->GetVideoTracks()[track_index]));
      }
    }
  }

  void SetVideoConstraints(const webrtc::FakeConstraints& video_constraint) {
    video_constraints_ = video_constraint;
  }

  void AddMediaStream(bool audio, bool video) {
    std::string stream_label =
        kStreamLabelBase +
        rtc::ToString<int>(static_cast<int>(pc()->local_streams()->count()));
    rtc::scoped_refptr<MediaStreamInterface> stream =
        peer_connection_factory_->CreateLocalMediaStream(stream_label);

    if (audio && can_receive_audio()) {
      stream->AddTrack(CreateLocalAudioTrack(stream_label));
    }
    if (video && can_receive_video()) {
      stream->AddTrack(CreateLocalVideoTrack(stream_label));
    }

    EXPECT_TRUE(pc()->AddStream(stream));
  }

  size_t NumberOfLocalMediaStreams() { return pc()->local_streams()->count(); }

  bool SessionActive() {
    return pc()->signaling_state() == webrtc::PeerConnectionInterface::kStable;
  }

  // Automatically add a stream when receiving an offer, if we don't have one.
  // Defaults to true.
  void set_auto_add_stream(bool auto_add_stream) {
    auto_add_stream_ = auto_add_stream;
  }

  void set_signaling_message_receiver(
      SignalingMessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  void set_signaling_delay_ms(int delay_ms) { signaling_delay_ms_ = delay_ms; }

  void EnableVideoDecoderFactory() {
    video_decoder_factory_enabled_ = true;
    fake_video_decoder_factory_->AddSupportedVideoCodecType(
        webrtc::kVideoCodecVP8);
  }

  void IceRestart() {
    offer_answer_constraints_.SetMandatoryIceRestart(true);
    offer_answer_options_.ice_restart = true;
    SetExpectIceRestart(true);
  }

  void SetExpectIceRestart(bool expect_restart) {
    expect_ice_restart_ = expect_restart;
  }

  bool ExpectIceRestart() const { return expect_ice_restart_; }

  void SetExpectIceRenomination(bool expect_renomination) {
    expect_ice_renomination_ = expect_renomination;
  }
  void SetExpectRemoteIceRenomination(bool expect_renomination) {
    expect_remote_ice_renomination_ = expect_renomination;
  }
  bool ExpectIceRenomination() { return expect_ice_renomination_; }
  bool ExpectRemoteIceRenomination() { return expect_remote_ice_renomination_; }

  // The below 3 methods assume streams will be offered.
  // Thus they'll only set the "offer to receive" flag to true if it's
  // currently false, not if it's just unset.
  void SetReceiveAudioVideo(bool audio, bool video) {
    SetReceiveAudio(audio);
    SetReceiveVideo(video);
    ASSERT_EQ(audio, can_receive_audio());
    ASSERT_EQ(video, can_receive_video());
  }

  void SetReceiveAudio(bool audio) {
    if (audio && can_receive_audio()) {
      return;
    }
    offer_answer_constraints_.SetMandatoryReceiveAudio(audio);
    offer_answer_options_.offer_to_receive_audio = audio ? 1 : 0;
  }

  void SetReceiveVideo(bool video) {
    if (video && can_receive_video()) {
      return;
    }
    offer_answer_constraints_.SetMandatoryReceiveVideo(video);
    offer_answer_options_.offer_to_receive_video = video ? 1 : 0;
  }

  void SetOfferToReceiveAudioVideo(bool audio, bool video) {
    offer_answer_constraints_.SetMandatoryReceiveAudio(audio);
    offer_answer_options_.offer_to_receive_audio = audio ? 1 : 0;
    offer_answer_constraints_.SetMandatoryReceiveVideo(video);
    offer_answer_options_.offer_to_receive_video = video ? 1 : 0;
  }

  void RemoveMsidFromReceivedSdp(bool remove) { remove_msid_ = remove; }

  void RemoveSdesCryptoFromReceivedSdp(bool remove) { remove_sdes_ = remove; }

  void RemoveBundleFromReceivedSdp(bool remove) { remove_bundle_ = remove; }

  void RemoveCvoFromReceivedSdp(bool remove) { remove_cvo_ = remove; }

  void MakeSpecCompliantMaxBundleOfferFromReceivedSdp(bool real) {
    make_spec_compliant_max_bundle_offer_ = real;
  }

  bool can_receive_audio() {
    bool value;
    if (prefer_constraint_apis_) {
      if (webrtc::FindConstraint(
              &offer_answer_constraints_,
              MediaConstraintsInterface::kOfferToReceiveAudio, &value,
              nullptr)) {
        return value;
      }
      return true;
    }
    return offer_answer_options_.offer_to_receive_audio > 0 ||
           offer_answer_options_.offer_to_receive_audio ==
               PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined;
  }

  bool can_receive_video() {
    bool value;
    if (prefer_constraint_apis_) {
      if (webrtc::FindConstraint(
              &offer_answer_constraints_,
              MediaConstraintsInterface::kOfferToReceiveVideo, &value,
              nullptr)) {
        return value;
      }
      return true;
    }
    return offer_answer_options_.offer_to_receive_video > 0 ||
           offer_answer_options_.offer_to_receive_video ==
               PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined;
  }

  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    LOG(INFO) << id_ << "OnDataChannel";
    data_channel_ = data_channel;
    data_observer_.reset(new MockDataChannelObserver(data_channel));
  }

  void CreateDataChannel() { CreateDataChannel(nullptr); }

  void CreateDataChannel(const webrtc::DataChannelInit* init) {
    data_channel_ = pc()->CreateDataChannel(kDataChannelLabel, init);
    ASSERT_TRUE(data_channel_.get() != nullptr);
    data_observer_.reset(new MockDataChannelObserver(data_channel_));
  }

  rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateLocalAudioTrack(
      const std::string& stream_label) {
    FakeConstraints constraints;
    // Disable highpass filter so that we can get all the test audio frames.
    constraints.AddMandatory(MediaConstraintsInterface::kHighpassFilter, false);
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(&constraints);
    // TODO(perkj): Test audio source when it is implemented. Currently audio
    // always use the default input.
    std::string label = stream_label + kAudioTrackLabelBase;
    return peer_connection_factory_->CreateAudioTrack(label, source);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack(
      const std::string& stream_label) {
    // Set max frame rate to 10fps to reduce the risk of the tests to be flaky.
    FakeConstraints source_constraints = video_constraints_;
    source_constraints.SetMandatoryMaxFrameRate(10);

    cricket::FakeVideoCapturer* fake_capturer =
        new webrtc::FakePeriodicVideoCapturer();
    fake_capturer->SetRotation(capture_rotation_);
    video_capturers_.push_back(fake_capturer);
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source =
        peer_connection_factory_->CreateVideoSource(
            std::unique_ptr<cricket::VideoCapturer>(fake_capturer),
            &source_constraints);
    std::string label = stream_label + kVideoTrackLabelBase;

    rtc::scoped_refptr<webrtc::VideoTrackInterface> track(
        peer_connection_factory_->CreateVideoTrack(label, source));
    if (!local_video_renderer_) {
      local_video_renderer_.reset(new webrtc::FakeVideoTrackRenderer(track));
    }
    return track;
  }

  DataChannelInterface* data_channel() { return data_channel_; }
  const MockDataChannelObserver* data_observer() const {
    return data_observer_.get();
  }

  webrtc::PeerConnectionInterface* pc() const { return peer_connection_.get(); }

  void StopVideoCapturers() {
    for (auto* capturer : video_capturers_) {
      capturer->Stop();
    }
  }

  void SetCaptureRotation(webrtc::VideoRotation rotation) {
    ASSERT_TRUE(video_capturers_.empty());
    capture_rotation_ = rotation;
  }

  bool AudioFramesReceivedCheck(int number_of_frames) const {
    return number_of_frames <= fake_audio_capture_module_->frames_received();
  }

  int audio_frames_received() const {
    return fake_audio_capture_module_->frames_received();
  }

  bool VideoFramesReceivedCheck(int number_of_frames) {
    if (video_decoder_factory_enabled_) {
      const std::vector<FakeWebRtcVideoDecoder*>& decoders
          = fake_video_decoder_factory_->decoders();
      if (decoders.empty()) {
        return number_of_frames <= 0;
      }
      // Note - this checks that EACH decoder has the requisite number
      // of frames. The video_frames_received() function sums them.
      for (FakeWebRtcVideoDecoder* decoder : decoders) {
        if (number_of_frames > decoder->GetNumFramesReceived()) {
          return false;
        }
      }
      return true;
    } else {
      if (fake_video_renderers_.empty()) {
        return number_of_frames <= 0;
      }

      for (const auto& pair : fake_video_renderers_) {
        if (number_of_frames > pair.second->num_rendered_frames()) {
          return false;
        }
      }
      return true;
    }
  }

  int video_frames_received() const {
    int total = 0;
    if (video_decoder_factory_enabled_) {
      const std::vector<FakeWebRtcVideoDecoder*>& decoders =
          fake_video_decoder_factory_->decoders();
      for (const FakeWebRtcVideoDecoder* decoder : decoders) {
        total += decoder->GetNumFramesReceived();
      }
    } else {
      for (const auto& pair : fake_video_renderers_) {
        total += pair.second->num_rendered_frames();
      }
      for (const auto& renderer : removed_fake_video_renderers_) {
        total += renderer->num_rendered_frames();
      }
    }
    return total;
  }

  // Verify the CreateDtmfSender interface
  void VerifyDtmf() {
    std::unique_ptr<DummyDtmfObserver> observer(new DummyDtmfObserver());
    rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender;

    // We can't create a DTMF sender with an invalid audio track or a non local
    // track.
    EXPECT_TRUE(peer_connection_->CreateDtmfSender(nullptr) == nullptr);
    rtc::scoped_refptr<webrtc::AudioTrackInterface> non_localtrack(
        peer_connection_factory_->CreateAudioTrack("dummy_track", nullptr));
    EXPECT_TRUE(peer_connection_->CreateDtmfSender(non_localtrack) == nullptr);

    // We should be able to create a DTMF sender from a local track.
    webrtc::AudioTrackInterface* localtrack =
        peer_connection_->local_streams()->at(0)->GetAudioTracks()[0];
    dtmf_sender = peer_connection_->CreateDtmfSender(localtrack);
    EXPECT_TRUE(dtmf_sender.get() != nullptr);
    dtmf_sender->RegisterObserver(observer.get());

    // Test the DtmfSender object just created.
    EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
    EXPECT_TRUE(dtmf_sender->InsertDtmf("1a", 100, 50));

    // We don't need to verify that the DTMF tones are actually sent out because
    // that is already covered by the tests of the lower level components.

    EXPECT_TRUE_WAIT(observer->completed(), kMaxWaitMs);
    std::vector<std::string> tones;
    tones.push_back("1");
    tones.push_back("a");
    tones.push_back("");
    observer->Verify(tones);

    dtmf_sender->UnregisterObserver();
  }

  // Verifies that the SessionDescription have rejected the appropriate media
  // content.
  void VerifyRejectedMediaInSessionDescription() {
    ASSERT_TRUE(peer_connection_->remote_description() != nullptr);
    ASSERT_TRUE(peer_connection_->local_description() != nullptr);
    const cricket::SessionDescription* remote_desc =
        peer_connection_->remote_description()->description();
    const cricket::SessionDescription* local_desc =
        peer_connection_->local_description()->description();

    const ContentInfo* remote_audio_content = GetFirstAudioContent(remote_desc);
    if (remote_audio_content) {
      const ContentInfo* audio_content =
          GetFirstAudioContent(local_desc);
      EXPECT_EQ(can_receive_audio(), !audio_content->rejected);
    }

    const ContentInfo* remote_video_content = GetFirstVideoContent(remote_desc);
    if (remote_video_content) {
      const ContentInfo* video_content =
          GetFirstVideoContent(local_desc);
      EXPECT_EQ(can_receive_video(), !video_content->rejected);
    }
  }

  void VerifyLocalIceUfragAndPassword() {
    ASSERT_TRUE(peer_connection_->local_description() != nullptr);
    const cricket::SessionDescription* desc =
        peer_connection_->local_description()->description();
    const cricket::ContentInfos& contents = desc->contents();

    for (size_t index = 0; index < contents.size(); ++index) {
      if (contents[index].rejected)
        continue;
      const cricket::TransportDescription* transport_desc =
          desc->GetTransportDescriptionByName(contents[index].name);

      std::map<int, IceUfragPwdPair>::const_iterator ufragpair_it =
          ice_ufrag_pwd_.find(static_cast<int>(index));
      if (ufragpair_it == ice_ufrag_pwd_.end()) {
        ASSERT_FALSE(ExpectIceRestart());
        ice_ufrag_pwd_[static_cast<int>(index)] =
            IceUfragPwdPair(transport_desc->ice_ufrag, transport_desc->ice_pwd);
      } else if (ExpectIceRestart()) {
        const IceUfragPwdPair& ufrag_pwd = ufragpair_it->second;
        EXPECT_NE(ufrag_pwd.first, transport_desc->ice_ufrag);
        EXPECT_NE(ufrag_pwd.second, transport_desc->ice_pwd);
      } else {
        const IceUfragPwdPair& ufrag_pwd = ufragpair_it->second;
        EXPECT_EQ(ufrag_pwd.first, transport_desc->ice_ufrag);
        EXPECT_EQ(ufrag_pwd.second, transport_desc->ice_pwd);
      }
    }
  }

  void VerifyLocalIceRenomination() {
    ASSERT_TRUE(peer_connection_->local_description() != nullptr);
    const cricket::SessionDescription* desc =
        peer_connection_->local_description()->description();
    const cricket::ContentInfos& contents = desc->contents();

    for (auto content : contents) {
      if (content.rejected)
        continue;
      const cricket::TransportDescription* transport_desc =
          desc->GetTransportDescriptionByName(content.name);
      const auto& options = transport_desc->transport_options;
      auto iter = std::find(options.begin(), options.end(),
                            cricket::ICE_RENOMINATION_STR);
      EXPECT_EQ(ExpectIceRenomination(), iter != options.end());
    }
  }

  void VerifyRemoteIceRenomination() {
    ASSERT_TRUE(peer_connection_->remote_description() != nullptr);
    const cricket::SessionDescription* desc =
        peer_connection_->remote_description()->description();
    const cricket::ContentInfos& contents = desc->contents();

    for (auto content : contents) {
      if (content.rejected)
        continue;
      const cricket::TransportDescription* transport_desc =
          desc->GetTransportDescriptionByName(content.name);
      const auto& options = transport_desc->transport_options;
      auto iter = std::find(options.begin(), options.end(),
                            cricket::ICE_RENOMINATION_STR);
      EXPECT_EQ(ExpectRemoteIceRenomination(), iter != options.end());
    }
  }

  int GetAudioOutputLevelStats(webrtc::MediaStreamTrackInterface* track) {
    rtc::scoped_refptr<MockStatsObserver>
        observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, track, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    return observer->AudioOutputLevel();
  }

  int GetAudioInputLevelStats() {
    rtc::scoped_refptr<MockStatsObserver>
        observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, nullptr, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    return observer->AudioInputLevel();
  }

  int GetBytesReceivedStats(webrtc::MediaStreamTrackInterface* track) {
    rtc::scoped_refptr<MockStatsObserver>
    observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, track, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    return observer->BytesReceived();
  }

  int GetBytesSentStats(webrtc::MediaStreamTrackInterface* track) {
    rtc::scoped_refptr<MockStatsObserver>
    observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, track, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    return observer->BytesSent();
  }

  int GetAvailableReceivedBandwidthStats() {
    rtc::scoped_refptr<MockStatsObserver>
        observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, nullptr, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    int bw = observer->AvailableReceiveBandwidth();
    return bw;
  }

  std::string GetDtlsCipherStats() {
    rtc::scoped_refptr<MockStatsObserver>
        observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, nullptr, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    return observer->DtlsCipher();
  }

  std::string GetSrtpCipherStats() {
    rtc::scoped_refptr<MockStatsObserver>
        observer(new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, nullptr, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    EXPECT_NE(0, observer->timestamp());
    return observer->SrtpCipher();
  }

  int rendered_width() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty() ? 1 :
        fake_video_renderers_.begin()->second->width();
  }

  int rendered_height() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty() ? 1 :
        fake_video_renderers_.begin()->second->height();
  }

  webrtc::VideoRotation rendered_rotation() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? webrtc::kVideoRotation_0
               : fake_video_renderers_.begin()->second->rotation();
  }

  int local_rendered_width() {
    return local_video_renderer_ ? local_video_renderer_->width() : 1;
  }

  int local_rendered_height() {
    return local_video_renderer_ ? local_video_renderer_->height() : 1;
  }

  size_t number_of_remote_streams() {
    if (!pc())
      return 0;
    return pc()->remote_streams()->count();
  }

  StreamCollectionInterface* remote_streams() const {
    if (!pc()) {
      ADD_FAILURE();
      return nullptr;
    }
    return pc()->remote_streams();
  }

  StreamCollectionInterface* local_streams() {
    if (!pc()) {
      ADD_FAILURE();
      return nullptr;
    }
    return pc()->local_streams();
  }

  bool HasLocalAudioTrack() { return StreamsHaveAudioTrack(local_streams()); }

  bool HasLocalVideoTrack() { return StreamsHaveVideoTrack(local_streams()); }

  webrtc::PeerConnectionInterface::SignalingState signaling_state() {
    return pc()->signaling_state();
  }

  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state() {
    return pc()->ice_connection_state();
  }

  webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state() {
    return pc()->ice_gathering_state();
  }

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> const&
  rtp_receiver_observers() {
    return rtp_receiver_observers_;
  }

  void SetRtpReceiverObservers() {
    rtp_receiver_observers_.clear();
    for (auto receiver : pc()->GetReceivers()) {
      std::unique_ptr<MockRtpReceiverObserver> observer(
          new MockRtpReceiverObserver(receiver->media_type()));
      receiver->SetObserver(observer.get());
      rtp_receiver_observers_.push_back(std::move(observer));
    }
  }

 private:
  class DummyDtmfObserver : public DtmfSenderObserverInterface {
   public:
    DummyDtmfObserver() : completed_(false) {}

    // Implements DtmfSenderObserverInterface.
    void OnToneChange(const std::string& tone) override {
      tones_.push_back(tone);
      if (tone.empty()) {
        completed_ = true;
      }
    }

    void Verify(const std::vector<std::string>& tones) const {
      ASSERT_TRUE(tones_.size() == tones.size());
      EXPECT_TRUE(std::equal(tones.begin(), tones.end(), tones_.begin()));
    }

    bool completed() const { return completed_; }

   private:
    bool completed_;
    std::vector<std::string> tones_;
  };

  explicit PeerConnectionTestClient(const std::string& id) : id_(id) {}

  bool Init(
      const MediaConstraintsInterface* constraints,
      const PeerConnectionFactory::Options* options,
      const PeerConnectionInterface::RTCConfiguration* config,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      bool prefer_constraint_apis,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    EXPECT_TRUE(!peer_connection_);
    EXPECT_TRUE(!peer_connection_factory_);
    if (!prefer_constraint_apis) {
      EXPECT_TRUE(!constraints);
    }
    prefer_constraint_apis_ = prefer_constraint_apis;

    fake_network_manager_.reset(new rtc::FakeNetworkManager());
    fake_network_manager_->AddInterface(rtc::SocketAddress("192.168.1.1", 0));

    std::unique_ptr<cricket::PortAllocator> port_allocator(
        new cricket::BasicPortAllocator(fake_network_manager_.get()));
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create();

    if (fake_audio_capture_module_ == nullptr) {
      return false;
    }
    fake_video_decoder_factory_ = new FakeWebRtcVideoDecoderFactory();
    fake_video_encoder_factory_ = new FakeWebRtcVideoEncoderFactory();
    rtc::Thread* const signaling_thread = rtc::Thread::Current();
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread, worker_thread, signaling_thread,
        fake_audio_capture_module_, fake_video_encoder_factory_,
        fake_video_decoder_factory_);
    if (!peer_connection_factory_) {
      return false;
    }
    if (options) {
      peer_connection_factory_->SetOptions(*options);
    }
    peer_connection_ =
        CreatePeerConnection(std::move(port_allocator), constraints, config,
                             std::move(cert_generator));
    return peer_connection_.get() != nullptr;
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      std::unique_ptr<cricket::PortAllocator> port_allocator,
      const MediaConstraintsInterface* constraints,
      const PeerConnectionInterface::RTCConfiguration* config,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator) {
    // CreatePeerConnection with RTCConfiguration.
    PeerConnectionInterface::RTCConfiguration default_config;

    if (!config) {
      config = &default_config;
    }

    return peer_connection_factory_->CreatePeerConnection(
        *config, constraints, std::move(port_allocator),
        std::move(cert_generator), this);
  }

  void HandleIncomingOffer(const std::string& msg) {
    LOG(INFO) << id_ << "HandleIncomingOffer ";
    if (NumberOfLocalMediaStreams() == 0 && auto_add_stream_) {
      // If we are not sending any streams ourselves it is time to add some.
      AddMediaStream(true, true);
    }
    std::unique_ptr<SessionDescriptionInterface> desc(
        webrtc::CreateSessionDescription("offer", msg, nullptr));

    // Do the equivalent of setting the port to 0, adding a=bundle-only, and
    // removing a=ice-ufrag, a=ice-pwd, a=fingerprint and a=setup from all but
    // the first m= section.
    if (make_spec_compliant_max_bundle_offer_) {
      bool first = true;
      for (cricket::ContentInfo& content : desc->description()->contents()) {
        if (first) {
          first = false;
          continue;
        }
        content.bundle_only = true;
      }
      first = true;
      for (cricket::TransportInfo& transport :
           desc->description()->transport_infos()) {
        if (first) {
          first = false;
          continue;
        }
        transport.description.ice_ufrag.clear();
        transport.description.ice_pwd.clear();
        transport.description.connection_role = cricket::CONNECTIONROLE_NONE;
        transport.description.identity_fingerprint.reset(nullptr);
      }
    }

    EXPECT_TRUE(DoSetRemoteDescription(desc.release()));
    // Set the RtpReceiverObserver after receivers are created.
    SetRtpReceiverObservers();
    std::unique_ptr<SessionDescriptionInterface> answer;
    EXPECT_TRUE(DoCreateAnswer(&answer));
    std::string sdp;
    EXPECT_TRUE(answer->ToString(&sdp));
    EXPECT_TRUE(DoSetLocalDescription(answer.release()));
    SendSdpMessage(webrtc::SessionDescriptionInterface::kAnswer, sdp);
  }

  void HandleIncomingAnswer(const std::string& msg) {
    LOG(INFO) << id_ << "HandleIncomingAnswer";
    std::unique_ptr<SessionDescriptionInterface> desc(
        webrtc::CreateSessionDescription("answer", msg, nullptr));
    EXPECT_TRUE(DoSetRemoteDescription(desc.release()));
    // Set the RtpReceiverObserver after receivers are created.
    SetRtpReceiverObservers();
  }

  bool DoCreateOfferAnswer(std::unique_ptr<SessionDescriptionInterface>* desc,
                           bool offer) {
    rtc::scoped_refptr<MockCreateSessionDescriptionObserver>
        observer(new rtc::RefCountedObject<
            MockCreateSessionDescriptionObserver>());
    if (prefer_constraint_apis_) {
      if (offer) {
        pc()->CreateOffer(observer, &offer_answer_constraints_);
      } else {
        pc()->CreateAnswer(observer, &offer_answer_constraints_);
      }
    } else {
      if (offer) {
        pc()->CreateOffer(observer, offer_answer_options_);
      } else {
        pc()->CreateAnswer(observer, offer_answer_options_);
      }
    }
    EXPECT_EQ_WAIT(true, observer->called(), kMaxWaitMs);
    desc->reset(observer->release_desc());
    if (observer->result() && ExpectIceRestart()) {
      EXPECT_EQ(0u, (*desc)->candidates(0)->count());
    }
    return observer->result();
  }

  bool DoCreateOffer(std::unique_ptr<SessionDescriptionInterface>* desc) {
    return DoCreateOfferAnswer(desc, true);
  }

  bool DoCreateAnswer(std::unique_ptr<SessionDescriptionInterface>* desc) {
    return DoCreateOfferAnswer(desc, false);
  }

  bool DoSetLocalDescription(SessionDescriptionInterface* desc) {
    rtc::scoped_refptr<MockSetSessionDescriptionObserver>
            observer(new rtc::RefCountedObject<
                MockSetSessionDescriptionObserver>());
    LOG(INFO) << id_ << "SetLocalDescription ";
    pc()->SetLocalDescription(observer, desc);
    // Ignore the observer result. If we wait for the result with
    // EXPECT_TRUE_WAIT, local ice candidates might be sent to the remote peer
    // before the offer which is an error.
    // The reason is that EXPECT_TRUE_WAIT uses
    // rtc::Thread::Current()->ProcessMessages(1);
    // ProcessMessages waits at least 1ms but processes all messages before
    // returning. Since this test is synchronous and send messages to the remote
    // peer whenever a callback is invoked, this can lead to messages being
    // sent to the remote peer in the wrong order.
    // TODO(perkj): Find a way to check the result without risking that the
    // order of sent messages are changed. Ex- by posting all messages that are
    // sent to the remote peer.
    return true;
  }

  bool DoSetRemoteDescription(SessionDescriptionInterface* desc) {
    rtc::scoped_refptr<MockSetSessionDescriptionObserver>
        observer(new rtc::RefCountedObject<
            MockSetSessionDescriptionObserver>());
    LOG(INFO) << id_ << "SetRemoteDescription ";
    pc()->SetRemoteDescription(observer, desc);
    EXPECT_TRUE_WAIT(observer->called(), kMaxWaitMs);
    return observer->result();
  }

  // This modifies all received SDP messages before they are processed.
  void FilterIncomingSdpMessage(std::string* sdp) {
    if (remove_msid_) {
      const char kSdpSsrcAttribute[] = "a=ssrc:";
      RemoveLinesFromSdp(kSdpSsrcAttribute, sdp);
      const char kSdpMsidSupportedAttribute[] = "a=msid-semantic:";
      RemoveLinesFromSdp(kSdpMsidSupportedAttribute, sdp);
    }
    if (remove_bundle_) {
      const char kSdpBundleAttribute[] = "a=group:BUNDLE";
      RemoveLinesFromSdp(kSdpBundleAttribute, sdp);
    }
    if (remove_sdes_) {
      const char kSdpSdesCryptoAttribute[] = "a=crypto";
      RemoveLinesFromSdp(kSdpSdesCryptoAttribute, sdp);
    }
    if (remove_cvo_) {
      const char kSdpCvoExtenstion[] = "urn:3gpp:video-orientation";
      RemoveLinesFromSdp(kSdpCvoExtenstion, sdp);
    }
  }

  std::string id_;

  std::unique_ptr<rtc::FakeNetworkManager> fake_network_manager_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;

  bool prefer_constraint_apis_ = true;
  bool auto_add_stream_ = true;

  typedef std::pair<std::string, std::string> IceUfragPwdPair;
  std::map<int, IceUfragPwdPair> ice_ufrag_pwd_;
  bool expect_ice_restart_ = false;
  bool expect_ice_renomination_ = false;
  bool expect_remote_ice_renomination_ = false;

  // Needed to keep track of number of frames sent.
  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;
  // Needed to keep track of number of frames received.
  std::map<std::string, std::unique_ptr<webrtc::FakeVideoTrackRenderer>>
      fake_video_renderers_;
  // Needed to ensure frames aren't received for removed tracks.
  std::vector<std::unique_ptr<webrtc::FakeVideoTrackRenderer>>
      removed_fake_video_renderers_;
  // Needed to keep track of number of frames received when external decoder
  // used.
  FakeWebRtcVideoDecoderFactory* fake_video_decoder_factory_ = nullptr;
  FakeWebRtcVideoEncoderFactory* fake_video_encoder_factory_ = nullptr;
  bool video_decoder_factory_enabled_ = false;
  webrtc::FakeConstraints video_constraints_;

  // For remote peer communication.
  SignalingMessageReceiver* signaling_message_receiver_ = nullptr;
  int signaling_delay_ms_ = 0;

  // Store references to the video capturers we've created, so that we can stop
  // them, if required.
  std::vector<cricket::FakeVideoCapturer*> video_capturers_;
  webrtc::VideoRotation capture_rotation_ = webrtc::kVideoRotation_0;
  // |local_video_renderer_| attached to the first created local video track.
  std::unique_ptr<webrtc::FakeVideoTrackRenderer> local_video_renderer_;

  webrtc::FakeConstraints offer_answer_constraints_;
  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options_;
  bool remove_msid_ = false;  // True if MSID should be removed in received SDP.
  bool remove_bundle_ =
      false;  // True if bundle should be removed in received SDP.
  bool remove_sdes_ =
      false;  // True if a=crypto should be removed in received SDP.
  // |remove_cvo_| is true if extension urn:3gpp:video-orientation should be
  // removed in the received SDP.
  bool remove_cvo_ = false;
  // See LocalP2PTestWithSpecCompliantMaxBundleOffer.
  bool make_spec_compliant_max_bundle_offer_ = false;

  rtc::scoped_refptr<DataChannelInterface> data_channel_;
  std::unique_ptr<MockDataChannelObserver> data_observer_;

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> rtp_receiver_observers_;
};

class P2PTestConductor : public testing::Test {
 public:
  P2PTestConductor()
      : pss_(new rtc::PhysicalSocketServer),
        ss_(new rtc::VirtualSocketServer(pss_.get())),
        network_thread_(new rtc::Thread(ss_.get())),
        worker_thread_(rtc::Thread::Create()) {
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());
  }

  bool SessionActive() {
    return initiating_client_->SessionActive() &&
           receiving_client_->SessionActive();
  }

  // Return true if the number of frames provided have been received
  // on the video and audio tracks provided.
  bool FramesHaveArrived(int audio_frames_to_receive,
                         int video_frames_to_receive) {
    bool all_good = true;
    if (initiating_client_->HasLocalAudioTrack() &&
        receiving_client_->can_receive_audio()) {
      all_good &=
          receiving_client_->AudioFramesReceivedCheck(audio_frames_to_receive);
    }
    if (initiating_client_->HasLocalVideoTrack() &&
        receiving_client_->can_receive_video()) {
      all_good &=
          receiving_client_->VideoFramesReceivedCheck(video_frames_to_receive);
    }
    if (receiving_client_->HasLocalAudioTrack() &&
        initiating_client_->can_receive_audio()) {
      all_good &=
          initiating_client_->AudioFramesReceivedCheck(audio_frames_to_receive);
    }
    if (receiving_client_->HasLocalVideoTrack() &&
        initiating_client_->can_receive_video()) {
      all_good &=
          initiating_client_->VideoFramesReceivedCheck(video_frames_to_receive);
    }
    return all_good;
  }

  void VerifyDtmf() {
    initiating_client_->VerifyDtmf();
    receiving_client_->VerifyDtmf();
  }

  void TestUpdateOfferWithRejectedContent() {
    // Renegotiate, rejecting the video m-line.
    initiating_client_->Negotiate(true, false);
    ASSERT_TRUE_WAIT(SessionActive(), kMaxWaitForActivationMs);

    int pc1_audio_received = initiating_client_->audio_frames_received();
    int pc1_video_received = initiating_client_->video_frames_received();
    int pc2_audio_received = receiving_client_->audio_frames_received();
    int pc2_video_received = receiving_client_->video_frames_received();

    // Wait for some additional audio frames to be received.
    EXPECT_TRUE_WAIT(initiating_client_->AudioFramesReceivedCheck(
                         pc1_audio_received + kEndAudioFrameCount) &&
                         receiving_client_->AudioFramesReceivedCheck(
                             pc2_audio_received + kEndAudioFrameCount),
                     kMaxWaitForFramesMs);

    // During this time, we shouldn't have received any additional video frames
    // for the rejected video tracks.
    EXPECT_EQ(pc1_video_received, initiating_client_->video_frames_received());
    EXPECT_EQ(pc2_video_received, receiving_client_->video_frames_received());
  }

  void VerifyRenderedAspectRatio(int width, int height) {
    VerifyRenderedAspectRatio(width, height, webrtc::kVideoRotation_0);
  }

  void VerifyRenderedAspectRatio(int width,
                                 int height,
                                 webrtc::VideoRotation rotation) {
    double expected_aspect_ratio = static_cast<double>(width) / height;
    double receiving_client_rendered_aspect_ratio =
        static_cast<double>(receiving_client()->rendered_width()) /
        receiving_client()->rendered_height();
    double initializing_client_rendered_aspect_ratio =
        static_cast<double>(initializing_client()->rendered_width()) /
        initializing_client()->rendered_height();
    double initializing_client_local_rendered_aspect_ratio =
        static_cast<double>(initializing_client()->local_rendered_width()) /
        initializing_client()->local_rendered_height();
    // Verify end-to-end rendered aspect ratio.
    EXPECT_EQ(expected_aspect_ratio, receiving_client_rendered_aspect_ratio);
    EXPECT_EQ(expected_aspect_ratio, initializing_client_rendered_aspect_ratio);
    // Verify aspect ratio of the local preview.
    EXPECT_EQ(expected_aspect_ratio,
              initializing_client_local_rendered_aspect_ratio);

    // Verify rotation.
    EXPECT_EQ(rotation, receiving_client()->rendered_rotation());
    EXPECT_EQ(rotation, initializing_client()->rendered_rotation());
  }

  void VerifySessionDescriptions() {
    initiating_client_->VerifyRejectedMediaInSessionDescription();
    receiving_client_->VerifyRejectedMediaInSessionDescription();
    initiating_client_->VerifyLocalIceUfragAndPassword();
    receiving_client_->VerifyLocalIceUfragAndPassword();
  }

  ~P2PTestConductor() {
    if (initiating_client_) {
      initiating_client_->set_signaling_message_receiver(nullptr);
    }
    if (receiving_client_) {
      receiving_client_->set_signaling_message_receiver(nullptr);
    }
  }

  bool CreateTestClients() { return CreateTestClients(nullptr, nullptr); }

  bool CreateTestClients(MediaConstraintsInterface* init_constraints,
                         MediaConstraintsInterface* recv_constraints) {
    return CreateTestClients(init_constraints, nullptr, nullptr,
                             recv_constraints, nullptr, nullptr);
  }

  bool CreateTestClients(
      const PeerConnectionInterface::RTCConfiguration& init_config,
      const PeerConnectionInterface::RTCConfiguration& recv_config) {
    return CreateTestClients(nullptr, nullptr, &init_config, nullptr, nullptr,
                             &recv_config);
  }

  bool CreateTestClientsThatPreferNoConstraints() {
    initiating_client_.reset(
        PeerConnectionTestClient::CreateClientPreferNoConstraints(
            "Caller: ", nullptr, network_thread_.get(), worker_thread_.get()));
    receiving_client_.reset(
        PeerConnectionTestClient::CreateClientPreferNoConstraints(
            "Callee: ", nullptr, network_thread_.get(), worker_thread_.get()));
    if (!initiating_client_ || !receiving_client_) {
      return false;
    }
    // Remember the choice for possible later resets of the clients.
    prefer_constraint_apis_ = false;
    SetSignalingReceivers();
    return true;
  }

  bool CreateTestClients(
      MediaConstraintsInterface* init_constraints,
      PeerConnectionFactory::Options* init_options,
      const PeerConnectionInterface::RTCConfiguration* init_config,
      MediaConstraintsInterface* recv_constraints,
      PeerConnectionFactory::Options* recv_options,
      const PeerConnectionInterface::RTCConfiguration* recv_config) {
    initiating_client_.reset(PeerConnectionTestClient::CreateClient(
        "Caller: ", init_constraints, init_options, init_config,
        network_thread_.get(), worker_thread_.get()));
    receiving_client_.reset(PeerConnectionTestClient::CreateClient(
        "Callee: ", recv_constraints, recv_options, recv_config,
        network_thread_.get(), worker_thread_.get()));
    if (!initiating_client_ || !receiving_client_) {
      return false;
    }
    SetSignalingReceivers();
    return true;
  }

  void SetSignalingReceivers() {
    initiating_client_->set_signaling_message_receiver(receiving_client_.get());
    receiving_client_->set_signaling_message_receiver(initiating_client_.get());
  }

  void SetSignalingDelayMs(int delay_ms) {
    initiating_client_->set_signaling_delay_ms(delay_ms);
    receiving_client_->set_signaling_delay_ms(delay_ms);
  }

  void SetVideoConstraints(const webrtc::FakeConstraints& init_constraints,
                           const webrtc::FakeConstraints& recv_constraints) {
    initiating_client_->SetVideoConstraints(init_constraints);
    receiving_client_->SetVideoConstraints(recv_constraints);
  }

  void SetCaptureRotation(webrtc::VideoRotation rotation) {
    initiating_client_->SetCaptureRotation(rotation);
    receiving_client_->SetCaptureRotation(rotation);
  }

  void EnableVideoDecoderFactory() {
    initiating_client_->EnableVideoDecoderFactory();
    receiving_client_->EnableVideoDecoderFactory();
  }

  // This test sets up a call between two parties. Both parties send static
  // frames to each other. Once the test is finished the number of sent frames
  // is compared to the number of received frames.
  void LocalP2PTest() {
    if (initiating_client_->NumberOfLocalMediaStreams() == 0) {
      initiating_client_->AddMediaStream(true, true);
    }
    initiating_client_->Negotiate();
    // Assert true is used here since next tests are guaranteed to fail and
    // would eat up 5 seconds.
    ASSERT_TRUE_WAIT(SessionActive(), kMaxWaitForActivationMs);
    VerifySessionDescriptions();

    int audio_frame_count = kEndAudioFrameCount;
    int video_frame_count = kEndVideoFrameCount;
    // TODO(ronghuawu): Add test to cover the case of sendonly and recvonly.

    if ((!initiating_client_->can_receive_audio() &&
         !initiating_client_->can_receive_video()) ||
        (!receiving_client_->can_receive_audio() &&
         !receiving_client_->can_receive_video())) {
      // Neither audio nor video will flow, so connections won't be
      // established. There's nothing more to check.
      // TODO(hta): Check connection if there's a data channel.
      return;
    }

    // Audio or video is expected to flow, so both clients should reach the
    // Connected state, and the offerer (ICE controller) should proceed to
    // Completed.
    // Note: These tests have been observed to fail under heavy load at
    // shorter timeouts, so they may be flaky.
    EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                   initiating_client_->ice_connection_state(),
                   kMaxWaitForFramesMs);
    EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                   receiving_client_->ice_connection_state(),
                   kMaxWaitForFramesMs);

    // The ICE gathering state should end up in kIceGatheringComplete,
    // but there's a bug that prevents this at the moment, and the state
    // machine is being updated by the WEBRTC WG.
    // TODO(hta): Update this check when spec revisions finish.
    EXPECT_NE(webrtc::PeerConnectionInterface::kIceGatheringNew,
              initiating_client_->ice_gathering_state());
    EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceGatheringComplete,
                   receiving_client_->ice_gathering_state(),
                   kMaxWaitForFramesMs);

    // Check that the expected number of frames have arrived.
    EXPECT_TRUE_WAIT(FramesHaveArrived(audio_frame_count, video_frame_count),
                     kMaxWaitForFramesMs);
  }

  void SetupAndVerifyDtlsCall() {
    FakeConstraints setup_constraints;
    setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                   true);
    // Disable resolution adaptation, we don't want it interfering with the
    // test results.
    webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
    rtc_config.set_cpu_adaptation(false);

    ASSERT_TRUE(CreateTestClients(&setup_constraints, nullptr, &rtc_config,
                                  &setup_constraints, nullptr, &rtc_config));
    LocalP2PTest();
    VerifyRenderedAspectRatio(640, 480);
  }

  PeerConnectionTestClient* CreateDtlsClientWithAlternateKey() {
    FakeConstraints setup_constraints;
    setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                   true);
    // Disable resolution adaptation, we don't want it interfering with the
    // test results.
    webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
    rtc_config.set_cpu_adaptation(false);

    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    cert_generator->use_alternate_key();

    // Make sure the new client is using a different certificate.
    return PeerConnectionTestClient::CreateClientWithDtlsIdentityStore(
        "New Peer: ", &setup_constraints, nullptr, &rtc_config,
        std::move(cert_generator), prefer_constraint_apis_,
        network_thread_.get(), worker_thread_.get());
  }

  void SendRtpData(webrtc::DataChannelInterface* dc, const std::string& data) {
    // Messages may get lost on the unreliable DataChannel, so we send multiple
    // times to avoid test flakiness.
    static const size_t kSendAttempts = 5;

    for (size_t i = 0; i < kSendAttempts; ++i) {
      dc->Send(DataBuffer(data));
    }
  }

  rtc::Thread* network_thread() { return network_thread_.get(); }

  rtc::VirtualSocketServer* virtual_socket_server() { return ss_.get(); }

  PeerConnectionTestClient* initializing_client() {
    return initiating_client_.get();
  }

  // Set the |initiating_client_| to the |client| passed in and return the
  // original |initiating_client_|.
  PeerConnectionTestClient* set_initializing_client(
      PeerConnectionTestClient* client) {
    PeerConnectionTestClient* old = initiating_client_.release();
    initiating_client_.reset(client);
    return old;
  }

  PeerConnectionTestClient* receiving_client() {
    return receiving_client_.get();
  }

  // Set the |receiving_client_| to the |client| passed in and return the
  // original |receiving_client_|.
  PeerConnectionTestClient* set_receiving_client(
      PeerConnectionTestClient* client) {
    PeerConnectionTestClient* old = receiving_client_.release();
    receiving_client_.reset(client);
    return old;
  }

  bool AllObserversReceived(
      const std::vector<std::unique_ptr<MockRtpReceiverObserver>>& observers) {
    for (auto& observer : observers) {
      if (!observer->first_packet_received()) {
        return false;
      }
    }
    return true;
  }

  void TestGcmNegotiation(bool local_gcm_enabled, bool remote_gcm_enabled,
      int expected_cipher_suite) {
    PeerConnectionFactory::Options init_options;
    init_options.crypto_options.enable_gcm_crypto_suites = local_gcm_enabled;
    PeerConnectionFactory::Options recv_options;
    recv_options.crypto_options.enable_gcm_crypto_suites = remote_gcm_enabled;
    ASSERT_TRUE(CreateTestClients(nullptr, &init_options, nullptr, nullptr,
                                  &recv_options, nullptr));
    rtc::scoped_refptr<webrtc::FakeMetricsObserver>
        init_observer =
            new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
    initializing_client()->pc()->RegisterUMAObserver(init_observer);
    LocalP2PTest();

    EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(expected_cipher_suite),
                   initializing_client()->GetSrtpCipherStats(),
                   kMaxWaitMs);
    EXPECT_EQ(1,
              init_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                            expected_cipher_suite));
  }

 private:
  // |ss_| is used by |network_thread_| so it must be destroyed later.
  std::unique_ptr<rtc::PhysicalSocketServer> pss_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  // |network_thread_| and |worker_thread_| are used by both
  // |initiating_client_| and |receiving_client_| so they must be destroyed
  // later.
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<PeerConnectionTestClient> initiating_client_;
  std::unique_ptr<PeerConnectionTestClient> receiving_client_;
  bool prefer_constraint_apis_ = true;
};

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
#if !defined(THREAD_SANITIZER)

TEST_F(P2PTestConductor, TestRtpReceiverObserverCallbackFunction) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();
  EXPECT_TRUE_WAIT(
      AllObserversReceived(initializing_client()->rtp_receiver_observers()),
      kMaxWaitForFramesMs);
  EXPECT_TRUE_WAIT(
      AllObserversReceived(receiving_client()->rtp_receiver_observers()),
      kMaxWaitForFramesMs);
}

// The observers are expected to fire the signal even if they are set after the
// first packet is received.
TEST_F(P2PTestConductor, TestSetRtpReceiverObserverAfterFirstPacketIsReceived) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();
  // Reset the RtpReceiverObservers.
  initializing_client()->SetRtpReceiverObservers();
  receiving_client()->SetRtpReceiverObservers();
  EXPECT_TRUE_WAIT(
      AllObserversReceived(initializing_client()->rtp_receiver_observers()),
      kMaxWaitForFramesMs);
  EXPECT_TRUE_WAIT(
      AllObserversReceived(receiving_client()->rtp_receiver_observers()),
      kMaxWaitForFramesMs);
}

// This test sets up a Jsep call between two parties and test Dtmf.
// TODO(holmer): Disabled due to sometimes crashing on buildbots.
// See issue webrtc/2378.
TEST_F(P2PTestConductor, DISABLED_LocalP2PTestDtmf) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();
  VerifyDtmf();
}

// This test sets up a Jsep call between two parties and test that we can get a
// video aspect ratio of 16:9.
TEST_F(P2PTestConductor, LocalP2PTest16To9) {
  ASSERT_TRUE(CreateTestClients());
  FakeConstraints constraint;
  double requested_ratio = 640.0/360;
  constraint.SetMandatoryMinAspectRatio(requested_ratio);
  SetVideoConstraints(constraint, constraint);
  LocalP2PTest();

  ASSERT_LE(0, initializing_client()->rendered_height());
  double initiating_video_ratio =
      static_cast<double>(initializing_client()->rendered_width()) /
      initializing_client()->rendered_height();
  EXPECT_LE(requested_ratio, initiating_video_ratio);

  ASSERT_LE(0, receiving_client()->rendered_height());
  double receiving_video_ratio =
      static_cast<double>(receiving_client()->rendered_width()) /
      receiving_client()->rendered_height();
  EXPECT_LE(requested_ratio, receiving_video_ratio);
}

// This test sets up a Jsep call between two parties and test that the
// received video has a resolution of 1280*720.
// TODO(mallinath): Enable when
// http://code.google.com/p/webrtc/issues/detail?id=981 is fixed.
TEST_F(P2PTestConductor, DISABLED_LocalP2PTest1280By720) {
  ASSERT_TRUE(CreateTestClients());
  FakeConstraints constraint;
  constraint.SetMandatoryMinWidth(1280);
  constraint.SetMandatoryMinHeight(720);
  SetVideoConstraints(constraint, constraint);
  LocalP2PTest();
  VerifyRenderedAspectRatio(1280, 720);
}

// This test sets up a call between two endpoints that are configured to use
// DTLS key agreement. As a result, DTLS is negotiated and used for transport.
TEST_F(P2PTestConductor, LocalP2PTestDtls) {
  SetupAndVerifyDtlsCall();
}

// This test sets up an one-way call, with media only from initiator to
// responder.
TEST_F(P2PTestConductor, OneWayMediaCall) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->set_auto_add_stream(false);
  LocalP2PTest();
}

TEST_F(P2PTestConductor, OneWayMediaCallWithoutConstraints) {
  ASSERT_TRUE(CreateTestClientsThatPreferNoConstraints());
  receiving_client()->set_auto_add_stream(false);
  LocalP2PTest();
}

// This test sets up a audio call initially and then upgrades to audio/video,
// using DTLS.
TEST_F(P2PTestConductor, LocalP2PTestDtlsRenegotiate) {
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  receiving_client()->SetReceiveAudioVideo(true, false);
  LocalP2PTest();
  receiving_client()->SetReceiveAudioVideo(true, true);
  receiving_client()->Negotiate();
}

// This test sets up a call transfer to a new caller with a different DTLS
// fingerprint.
TEST_F(P2PTestConductor, LocalP2PTestDtlsTransferCallee) {
  SetupAndVerifyDtlsCall();

  // Keeping the original peer around which will still send packets to the
  // receiving client. These SRTP packets will be dropped.
  std::unique_ptr<PeerConnectionTestClient> original_peer(
      set_initializing_client(CreateDtlsClientWithAlternateKey()));
  original_peer->pc()->Close();

  SetSignalingReceivers();
  receiving_client()->SetExpectIceRestart(true);
  LocalP2PTest();
  VerifyRenderedAspectRatio(640, 480);
}

// This test sets up a non-bundle call and apply bundle during ICE restart. When
// bundle is in effect in the restart, the channel can successfully reset its
// DTLS-SRTP context.
TEST_F(P2PTestConductor, LocalP2PTestDtlsBundleInIceRestart) {
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  receiving_client()->RemoveBundleFromReceivedSdp(true);
  LocalP2PTest();
  VerifyRenderedAspectRatio(640, 480);

  initializing_client()->IceRestart();
  receiving_client()->SetExpectIceRestart(true);
  receiving_client()->RemoveBundleFromReceivedSdp(false);
  LocalP2PTest();
  VerifyRenderedAspectRatio(640, 480);
}

// This test sets up a call transfer to a new callee with a different DTLS
// fingerprint.
TEST_F(P2PTestConductor, LocalP2PTestDtlsTransferCaller) {
  SetupAndVerifyDtlsCall();

  // Keeping the original peer around which will still send packets to the
  // receiving client. These SRTP packets will be dropped.
  std::unique_ptr<PeerConnectionTestClient> original_peer(
      set_receiving_client(CreateDtlsClientWithAlternateKey()));
  original_peer->pc()->Close();

  SetSignalingReceivers();
  initializing_client()->IceRestart();
  LocalP2PTest();
  VerifyRenderedAspectRatio(640, 480);
}

TEST_F(P2PTestConductor, LocalP2PTestCVO) {
  ASSERT_TRUE(CreateTestClients());
  SetCaptureRotation(webrtc::kVideoRotation_90);
  LocalP2PTest();
  VerifyRenderedAspectRatio(640, 480, webrtc::kVideoRotation_90);
}

TEST_F(P2PTestConductor, LocalP2PTestReceiverDoesntSupportCVO) {
  ASSERT_TRUE(CreateTestClients());
  SetCaptureRotation(webrtc::kVideoRotation_90);
  receiving_client()->RemoveCvoFromReceivedSdp(true);
  LocalP2PTest();
  VerifyRenderedAspectRatio(480, 640, webrtc::kVideoRotation_0);
}

// This test sets up a call between two endpoints that are configured to use
// DTLS key agreement. The offerer don't support SDES. As a result, DTLS is
// negotiated and used for transport.
TEST_F(P2PTestConductor, LocalP2PTestOfferDtlsButNotSdes) {
  FakeConstraints setup_constraints;
  setup_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                 true);
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  receiving_client()->RemoveSdesCryptoFromReceivedSdp(true);
  LocalP2PTest();
  VerifyRenderedAspectRatio(640, 480);
}

#ifdef HAVE_SCTP
// This test verifies that the negotiation will succeed with data channel only
// in max-bundle mode.
TEST_F(P2PTestConductor, LocalP2PTestOfferDataChannelOnly) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.bundle_policy =
      webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
  ASSERT_TRUE(CreateTestClients(rtc_config, rtc_config));
  initializing_client()->CreateDataChannel();
  initializing_client()->Negotiate();
}
#endif

// This test sets up a Jsep call between two parties, and the callee only
// accept to receive video.
TEST_F(P2PTestConductor, LocalP2PTestAnswerVideo) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->SetReceiveAudioVideo(false, true);
  LocalP2PTest();
}

// This test sets up a Jsep call between two parties, and the callee only
// accept to receive audio.
TEST_F(P2PTestConductor, LocalP2PTestAnswerAudio) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->SetReceiveAudioVideo(true, false);
  LocalP2PTest();
}

// This test sets up a Jsep call between two parties, and the callee reject both
// audio and video.
TEST_F(P2PTestConductor, LocalP2PTestAnswerNone) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->SetReceiveAudioVideo(false, false);
  LocalP2PTest();
}

// This test sets up an audio and video call between two parties. After the call
// runs for a while (10 frames), the caller sends an update offer with video
// being rejected. Once the re-negotiation is done, the video flow should stop
// and the audio flow should continue.
TEST_F(P2PTestConductor, UpdateOfferWithRejectedContent) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();
  TestUpdateOfferWithRejectedContent();
}

// This test sets up a Jsep call between two parties. The MSID is removed from
// the SDP strings from the caller.
TEST_F(P2PTestConductor, LocalP2PTestWithoutMsid) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->RemoveMsidFromReceivedSdp(true);
  // TODO(perkj): Currently there is a bug that cause audio to stop playing if
  // audio and video is muxed when MSID is disabled. Remove
  // SetRemoveBundleFromSdp once
  // https://code.google.com/p/webrtc/issues/detail?id=1193 is fixed.
  receiving_client()->RemoveBundleFromReceivedSdp(true);
  LocalP2PTest();
}

TEST_F(P2PTestConductor, LocalP2PTestTwoStreams) {
  ASSERT_TRUE(CreateTestClients());
  // Set optional video constraint to max 320pixels to decrease CPU usage.
  FakeConstraints constraint;
  constraint.SetOptionalMaxWidth(320);
  SetVideoConstraints(constraint, constraint);
  initializing_client()->AddMediaStream(true, true);
  initializing_client()->AddMediaStream(false, true);
  ASSERT_EQ(2u, initializing_client()->NumberOfLocalMediaStreams());
  LocalP2PTest();
  EXPECT_EQ(2u, receiving_client()->number_of_remote_streams());
}

// Test that if applying a true "max bundle" offer, which uses ports of 0,
// "a=bundle-only", omitting "a=fingerprint", "a=setup", "a=ice-ufrag" and
// "a=ice-pwd" for all but the audio "m=" section, negotiation still completes
// successfully and media flows.
// TODO(deadbeef): Update this test to also omit "a=rtcp-mux", once that works.
// TODO(deadbeef): Won't need this test once we start generating actual
// standards-compliant SDP.
TEST_F(P2PTestConductor, LocalP2PTestWithSpecCompliantMaxBundleOffer) {
  ASSERT_TRUE(CreateTestClients());
  receiving_client()->MakeSpecCompliantMaxBundleOfferFromReceivedSdp(true);
  LocalP2PTest();
}

// Test that we can receive the audio output level from a remote audio track.
TEST_F(P2PTestConductor, GetAudioOutputLevelStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  StreamCollectionInterface* remote_streams =
      initializing_client()->remote_streams();
  ASSERT_GT(remote_streams->count(), 0u);
  ASSERT_GT(remote_streams->at(0)->GetAudioTracks().size(), 0u);
  MediaStreamTrackInterface* remote_audio_track =
      remote_streams->at(0)->GetAudioTracks()[0];

  // Get the audio output level stats. Note that the level is not available
  // until a RTCP packet has been received.
  EXPECT_TRUE_WAIT(
      initializing_client()->GetAudioOutputLevelStats(remote_audio_track) > 0,
      kMaxWaitForStatsMs);
}

// Test that an audio input level is reported.
TEST_F(P2PTestConductor, GetAudioInputLevelStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  // Get the audio input level stats.  The level should be available very
  // soon after the test starts.
  EXPECT_TRUE_WAIT(initializing_client()->GetAudioInputLevelStats() > 0,
      kMaxWaitForStatsMs);
}

// Test that we can get incoming byte counts from both audio and video tracks.
TEST_F(P2PTestConductor, GetBytesReceivedStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  StreamCollectionInterface* remote_streams =
      initializing_client()->remote_streams();
  ASSERT_GT(remote_streams->count(), 0u);
  ASSERT_GT(remote_streams->at(0)->GetAudioTracks().size(), 0u);
  MediaStreamTrackInterface* remote_audio_track =
      remote_streams->at(0)->GetAudioTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesReceivedStats(remote_audio_track) > 0,
      kMaxWaitForStatsMs);

  MediaStreamTrackInterface* remote_video_track =
      remote_streams->at(0)->GetVideoTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesReceivedStats(remote_video_track) > 0,
      kMaxWaitForStatsMs);
}

// Test that we can get outgoing byte counts from both audio and video tracks.
TEST_F(P2PTestConductor, GetBytesSentStats) {
  ASSERT_TRUE(CreateTestClients());
  LocalP2PTest();

  StreamCollectionInterface* local_streams =
      initializing_client()->local_streams();
  ASSERT_GT(local_streams->count(), 0u);
  ASSERT_GT(local_streams->at(0)->GetAudioTracks().size(), 0u);
  MediaStreamTrackInterface* local_audio_track =
      local_streams->at(0)->GetAudioTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesSentStats(local_audio_track) > 0,
      kMaxWaitForStatsMs);

  MediaStreamTrackInterface* local_video_track =
      local_streams->at(0)->GetVideoTracks()[0];
  EXPECT_TRUE_WAIT(
      initializing_client()->GetBytesSentStats(local_video_track) > 0,
      kMaxWaitForStatsMs);
}

// Test that DTLS 1.0 is used if both sides only support DTLS 1.0.
TEST_F(P2PTestConductor, GetDtls12None) {
  PeerConnectionFactory::Options init_options;
  init_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  PeerConnectionFactory::Options recv_options;
  recv_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(CreateTestClients(nullptr, &init_options, nullptr, nullptr,
                                &recv_options, nullptr));
  rtc::scoped_refptr<webrtc::FakeMetricsObserver>
      init_observer = new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  initializing_client()->pc()->RegisterUMAObserver(init_observer);
  LocalP2PTest();

  EXPECT_TRUE_WAIT(
      rtc::SSLStreamAdapter::IsAcceptableCipher(
          initializing_client()->GetDtlsCipherStats(), rtc::KT_DEFAULT),
      kMaxWaitForStatsMs);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 initializing_client()->GetSrtpCipherStats(),
                 kMaxWaitForStatsMs);
  EXPECT_EQ(1,
            init_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                          kDefaultSrtpCryptoSuite));
}

// Test that DTLS 1.2 is used if both ends support it.
TEST_F(P2PTestConductor, GetDtls12Both) {
  PeerConnectionFactory::Options init_options;
  init_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  PeerConnectionFactory::Options recv_options;
  recv_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  ASSERT_TRUE(CreateTestClients(nullptr, &init_options, nullptr, nullptr,
                                &recv_options, nullptr));
  rtc::scoped_refptr<webrtc::FakeMetricsObserver>
      init_observer = new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  initializing_client()->pc()->RegisterUMAObserver(init_observer);
  LocalP2PTest();

  EXPECT_TRUE_WAIT(
      rtc::SSLStreamAdapter::IsAcceptableCipher(
          initializing_client()->GetDtlsCipherStats(), rtc::KT_DEFAULT),
      kMaxWaitForStatsMs);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 initializing_client()->GetSrtpCipherStats(),
                 kMaxWaitForStatsMs);
  EXPECT_EQ(1,
            init_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                          kDefaultSrtpCryptoSuite));
}

// Test that DTLS 1.0 is used if the initator supports DTLS 1.2 and the
// received supports 1.0.
TEST_F(P2PTestConductor, GetDtls12Init) {
  PeerConnectionFactory::Options init_options;
  init_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  PeerConnectionFactory::Options recv_options;
  recv_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(CreateTestClients(nullptr, &init_options, nullptr, nullptr,
                                &recv_options, nullptr));
  rtc::scoped_refptr<webrtc::FakeMetricsObserver>
      init_observer = new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  initializing_client()->pc()->RegisterUMAObserver(init_observer);
  LocalP2PTest();

  EXPECT_TRUE_WAIT(
      rtc::SSLStreamAdapter::IsAcceptableCipher(
          initializing_client()->GetDtlsCipherStats(), rtc::KT_DEFAULT),
      kMaxWaitForStatsMs);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 initializing_client()->GetSrtpCipherStats(),
                 kMaxWaitForStatsMs);
  EXPECT_EQ(1,
            init_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                          kDefaultSrtpCryptoSuite));
}

// Test that DTLS 1.0 is used if the initator supports DTLS 1.0 and the
// received supports 1.2.
TEST_F(P2PTestConductor, GetDtls12Recv) {
  PeerConnectionFactory::Options init_options;
  init_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  PeerConnectionFactory::Options recv_options;
  recv_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  ASSERT_TRUE(CreateTestClients(nullptr, &init_options, nullptr, nullptr,
                                &recv_options, nullptr));
  rtc::scoped_refptr<webrtc::FakeMetricsObserver>
      init_observer = new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  initializing_client()->pc()->RegisterUMAObserver(init_observer);
  LocalP2PTest();

  EXPECT_TRUE_WAIT(
      rtc::SSLStreamAdapter::IsAcceptableCipher(
          initializing_client()->GetDtlsCipherStats(), rtc::KT_DEFAULT),
      kMaxWaitForStatsMs);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 initializing_client()->GetSrtpCipherStats(),
                 kMaxWaitForStatsMs);
  EXPECT_EQ(1,
            init_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                          kDefaultSrtpCryptoSuite));
}

// Test that a non-GCM cipher is used if both sides only support non-GCM.
TEST_F(P2PTestConductor, GetGcmNone) {
  TestGcmNegotiation(false, false, kDefaultSrtpCryptoSuite);
}

// Test that a GCM cipher is used if both ends support it.
TEST_F(P2PTestConductor, GetGcmBoth) {
  TestGcmNegotiation(true, true, kDefaultSrtpCryptoSuiteGcm);
}

// Test that GCM isn't used if only the initiator supports it.
TEST_F(P2PTestConductor, GetGcmInit) {
  TestGcmNegotiation(true, false, kDefaultSrtpCryptoSuite);
}

// Test that GCM isn't used if only the receiver supports it.
TEST_F(P2PTestConductor, GetGcmRecv) {
  TestGcmNegotiation(false, true, kDefaultSrtpCryptoSuite);
}

// This test sets up a call between two parties with audio, video and an RTP
// data channel.
TEST_F(P2PTestConductor, LocalP2PTestRtpDataChannel) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  initializing_client()->CreateDataChannel();
  LocalP2PTest();
  ASSERT_TRUE(initializing_client()->data_channel() != nullptr);
  ASSERT_TRUE(receiving_client()->data_channel() != nullptr);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);

  std::string data = "hello world";

  SendRtpData(initializing_client()->data_channel(), data);
  EXPECT_EQ_WAIT(data, receiving_client()->data_observer()->last_message(),
                 kMaxWaitMs);

  SendRtpData(receiving_client()->data_channel(), data);
  EXPECT_EQ_WAIT(data, initializing_client()->data_observer()->last_message(),
                 kMaxWaitMs);

  receiving_client()->data_channel()->Close();
  // Send new offer and answer.
  receiving_client()->Negotiate();
  EXPECT_FALSE(initializing_client()->data_observer()->IsOpen());
  EXPECT_FALSE(receiving_client()->data_observer()->IsOpen());
}

#ifdef HAVE_SCTP
// This test sets up a call between two parties with audio, video and an SCTP
// data channel.
TEST_F(P2PTestConductor, LocalP2PTestSctpDataChannel) {
  ASSERT_TRUE(CreateTestClients());
  initializing_client()->CreateDataChannel();
  LocalP2PTest();
  ASSERT_TRUE(initializing_client()->data_channel() != nullptr);
  EXPECT_TRUE_WAIT(receiving_client()->data_channel() != nullptr, kMaxWaitMs);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(), kMaxWaitMs);

  std::string data = "hello world";

  initializing_client()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, receiving_client()->data_observer()->last_message(),
                 kMaxWaitMs);

  receiving_client()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, initializing_client()->data_observer()->last_message(),
                 kMaxWaitMs);

  receiving_client()->data_channel()->Close();
  EXPECT_TRUE_WAIT(!initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(!receiving_client()->data_observer()->IsOpen(), kMaxWaitMs);
}

TEST_F(P2PTestConductor, UnorderedSctpDataChannel) {
  ASSERT_TRUE(CreateTestClients());
  webrtc::DataChannelInit init;
  init.ordered = false;
  initializing_client()->CreateDataChannel(&init);

  // Introduce random network delays.
  // Otherwise it's not a true "unordered" test.
  virtual_socket_server()->set_delay_mean(20);
  virtual_socket_server()->set_delay_stddev(5);
  virtual_socket_server()->UpdateDelayDistribution();

  initializing_client()->Negotiate();
  ASSERT_TRUE(initializing_client()->data_channel() != nullptr);
  EXPECT_TRUE_WAIT(receiving_client()->data_channel() != nullptr, kMaxWaitMs);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(), kMaxWaitMs);

  static constexpr int kNumMessages = 100;
  // Deliberately chosen to be larger than the MTU so messages get fragmented.
  static constexpr size_t kMaxMessageSize = 4096;
  // Create and send random messages.
  std::vector<std::string> sent_messages;
  for (int i = 0; i < kNumMessages; ++i) {
    size_t length = (rand() % kMaxMessageSize) + 1;
    std::string message;
    ASSERT_TRUE(rtc::CreateRandomString(length, &message));
    initializing_client()->data_channel()->Send(DataBuffer(message));
    receiving_client()->data_channel()->Send(DataBuffer(message));
    sent_messages.push_back(message);
  }

  EXPECT_EQ_WAIT(
      kNumMessages,
      initializing_client()->data_observer()->received_message_count(),
      kMaxWaitMs);
  EXPECT_EQ_WAIT(kNumMessages,
                 receiving_client()->data_observer()->received_message_count(),
                 kMaxWaitMs);

  // Sort and compare to make sure none of the messages were corrupted.
  std::vector<std::string> initializing_client_received_messages =
      initializing_client()->data_observer()->messages();
  std::vector<std::string> receiving_client_received_messages =
      receiving_client()->data_observer()->messages();
  std::sort(sent_messages.begin(), sent_messages.end());
  std::sort(initializing_client_received_messages.begin(),
            initializing_client_received_messages.end());
  std::sort(receiving_client_received_messages.begin(),
            receiving_client_received_messages.end());
  EXPECT_EQ(sent_messages, initializing_client_received_messages);
  EXPECT_EQ(sent_messages, receiving_client_received_messages);

  receiving_client()->data_channel()->Close();
  EXPECT_TRUE_WAIT(!initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(!receiving_client()->data_observer()->IsOpen(), kMaxWaitMs);
}
#endif  // HAVE_SCTP

// This test sets up a call between two parties and creates a data channel.
// The test tests that received data is buffered unless an observer has been
// registered.
// Rtp data channels can receive data before the underlying
// transport has detected that a channel is writable and thus data can be
// received before the data channel state changes to open. That is hard to test
// but the same buffering is used in that case.
TEST_F(P2PTestConductor, RegisterDataChannelObserver) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  initializing_client()->CreateDataChannel();
  initializing_client()->Negotiate();

  ASSERT_TRUE(initializing_client()->data_channel() != nullptr);
  ASSERT_TRUE(receiving_client()->data_channel() != nullptr);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_EQ_WAIT(DataChannelInterface::kOpen,
                 receiving_client()->data_channel()->state(), kMaxWaitMs);

  // Unregister the existing observer.
  receiving_client()->data_channel()->UnregisterObserver();

  std::string data = "hello world";
  SendRtpData(initializing_client()->data_channel(), data);

  // Wait a while to allow the sent data to arrive before an observer is
  // registered..
  rtc::Thread::Current()->ProcessMessages(100);

  MockDataChannelObserver new_observer(receiving_client()->data_channel());
  EXPECT_EQ_WAIT(data, new_observer.last_message(), kMaxWaitMs);
}

// This test sets up a call between two parties with audio, video and but only
// the initiating client support data.
TEST_F(P2PTestConductor, LocalP2PTestReceiverDoesntSupportData) {
  FakeConstraints setup_constraints_1;
  setup_constraints_1.SetAllowRtpDataChannels();
  // Must disable DTLS to make negotiation succeed.
  setup_constraints_1.SetMandatory(
      MediaConstraintsInterface::kEnableDtlsSrtp, false);
  FakeConstraints setup_constraints_2;
  setup_constraints_2.SetMandatory(
      MediaConstraintsInterface::kEnableDtlsSrtp, false);
  ASSERT_TRUE(CreateTestClients(&setup_constraints_1, &setup_constraints_2));
  initializing_client()->CreateDataChannel();
  LocalP2PTest();
  EXPECT_TRUE(initializing_client()->data_channel() != nullptr);
  EXPECT_FALSE(receiving_client()->data_channel());
  EXPECT_FALSE(initializing_client()->data_observer()->IsOpen());
}

// This test sets up a call between two parties with audio, video. When audio
// and video is setup and flowing and data channel is negotiated.
TEST_F(P2PTestConductor, AddDataChannelAfterRenegotiation) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreateTestClients(&setup_constraints, &setup_constraints));
  LocalP2PTest();
  initializing_client()->CreateDataChannel();
  // Send new offer and answer.
  initializing_client()->Negotiate();
  ASSERT_TRUE(initializing_client()->data_channel() != nullptr);
  ASSERT_TRUE(receiving_client()->data_channel() != nullptr);
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
}

// This test sets up a Jsep call with SCTP DataChannel and verifies the
// negotiation is completed without error.
#ifdef HAVE_SCTP
TEST_F(P2PTestConductor, CreateOfferWithSctpDataChannel) {
  FakeConstraints constraints;
  constraints.SetMandatory(
      MediaConstraintsInterface::kEnableDtlsSrtp, true);
  ASSERT_TRUE(CreateTestClients(&constraints, &constraints));
  initializing_client()->CreateDataChannel();
  initializing_client()->Negotiate(false, false);
}
#endif

// This test sets up a call between two parties with audio, and video.
// During the call, the initializing side restart ice and the test verifies that
// new ice candidates are generated and audio and video still can flow.
TEST_F(P2PTestConductor, IceRestart) {
  ASSERT_TRUE(CreateTestClients());

  // Negotiate and wait for ice completion and make sure audio and video plays.
  LocalP2PTest();

  // Create a SDP string of the first audio candidate for both clients.
  const webrtc::IceCandidateCollection* audio_candidates_initiator =
      initializing_client()->pc()->local_description()->candidates(0);
  const webrtc::IceCandidateCollection* audio_candidates_receiver =
      receiving_client()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_initiator->count(), 0u);
  ASSERT_GT(audio_candidates_receiver->count(), 0u);
  std::string initiator_candidate;
  EXPECT_TRUE(
      audio_candidates_initiator->at(0)->ToString(&initiator_candidate));
  std::string receiver_candidate;
  EXPECT_TRUE(audio_candidates_receiver->at(0)->ToString(&receiver_candidate));

  // Restart ice on the initializing client.
  receiving_client()->SetExpectIceRestart(true);
  initializing_client()->IceRestart();

  // Negotiate and wait for ice completion again and make sure audio and video
  // plays.
  LocalP2PTest();

  // Create a SDP string of the first audio candidate for both clients again.
  const webrtc::IceCandidateCollection* audio_candidates_initiator_restart =
      initializing_client()->pc()->local_description()->candidates(0);
  const webrtc::IceCandidateCollection* audio_candidates_reciever_restart =
      receiving_client()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_initiator_restart->count(), 0u);
  ASSERT_GT(audio_candidates_reciever_restart->count(), 0u);
  std::string initiator_candidate_restart;
  EXPECT_TRUE(audio_candidates_initiator_restart->at(0)->ToString(
      &initiator_candidate_restart));
  std::string receiver_candidate_restart;
  EXPECT_TRUE(audio_candidates_reciever_restart->at(0)->ToString(
      &receiver_candidate_restart));

  // Verify that the first candidates in the local session descriptions has
  // changed.
  EXPECT_NE(initiator_candidate, initiator_candidate_restart);
  EXPECT_NE(receiver_candidate, receiver_candidate_restart);
}

TEST_F(P2PTestConductor, IceRenominationDisabled) {
  PeerConnectionInterface::RTCConfiguration config;
  config.enable_ice_renomination = false;
  ASSERT_TRUE(CreateTestClients(config, config));
  LocalP2PTest();

  initializing_client()->VerifyLocalIceRenomination();
  receiving_client()->VerifyLocalIceRenomination();
  initializing_client()->VerifyRemoteIceRenomination();
  receiving_client()->VerifyRemoteIceRenomination();
}

TEST_F(P2PTestConductor, IceRenominationEnabled) {
  PeerConnectionInterface::RTCConfiguration config;
  config.enable_ice_renomination = true;
  ASSERT_TRUE(CreateTestClients(config, config));
  initializing_client()->SetExpectIceRenomination(true);
  initializing_client()->SetExpectRemoteIceRenomination(true);
  receiving_client()->SetExpectIceRenomination(true);
  receiving_client()->SetExpectRemoteIceRenomination(true);
  LocalP2PTest();

  initializing_client()->VerifyLocalIceRenomination();
  receiving_client()->VerifyLocalIceRenomination();
  initializing_client()->VerifyRemoteIceRenomination();
  receiving_client()->VerifyRemoteIceRenomination();
}

// This test sets up a call between two parties with audio, and video.
// It then renegotiates setting the video m-line to "port 0", then later
// renegotiates again, enabling video.
TEST_F(P2PTestConductor, LocalP2PTestVideoDisableEnable) {
  ASSERT_TRUE(CreateTestClients());

  // Do initial negotiation. Will result in video and audio sendonly m-lines.
  receiving_client()->set_auto_add_stream(false);
  initializing_client()->AddMediaStream(true, true);
  initializing_client()->Negotiate();

  // Negotiate again, disabling the video m-line (receiving client will
  // set port to 0 due to mandatory "OfferToReceiveVideo: false" constraint).
  receiving_client()->SetReceiveVideo(false);
  initializing_client()->Negotiate();

  // Enable video and do negotiation again, making sure video is received
  // end-to-end.
  receiving_client()->SetReceiveVideo(true);
  receiving_client()->AddMediaStream(true, true);
  LocalP2PTest();
}

// This test sets up a Jsep call between two parties with external
// VideoDecoderFactory.
// TODO(holmer): Disabled due to sometimes crashing on buildbots.
// See issue webrtc/2378.
TEST_F(P2PTestConductor, DISABLED_LocalP2PTestWithVideoDecoderFactory) {
  ASSERT_TRUE(CreateTestClients());
  EnableVideoDecoderFactory();
  LocalP2PTest();
}

// This tests that if we negotiate after calling CreateSender but before we
// have a track, then set a track later, frames from the newly-set track are
// received end-to-end.
TEST_F(P2PTestConductor, EarlyWarmupTest) {
  ASSERT_TRUE(CreateTestClients());
  auto audio_sender =
      initializing_client()->pc()->CreateSender("audio", "stream_id");
  auto video_sender =
      initializing_client()->pc()->CreateSender("video", "stream_id");
  initializing_client()->Negotiate();
  // Wait for ICE connection to complete, without any tracks.
  // Note that the receiving client WILL (in HandleIncomingOffer) create
  // tracks, so it's only the initiator here that's doing early warmup.
  ASSERT_TRUE_WAIT(SessionActive(), kMaxWaitForActivationMs);
  VerifySessionDescriptions();
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 initializing_client()->ice_connection_state(),
                 kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 receiving_client()->ice_connection_state(),
                 kMaxWaitForFramesMs);
  // Now set the tracks, and expect frames to immediately start flowing.
  EXPECT_TRUE(
      audio_sender->SetTrack(initializing_client()->CreateLocalAudioTrack("")));
  EXPECT_TRUE(
      video_sender->SetTrack(initializing_client()->CreateLocalVideoTrack("")));
  EXPECT_TRUE_WAIT(FramesHaveArrived(kEndAudioFrameCount, kEndVideoFrameCount),
                   kMaxWaitForFramesMs);
}

#ifdef HAVE_QUIC
// This test sets up a call between two parties using QUIC instead of DTLS for
// audio and video, and a QUIC data channel.
TEST_F(P2PTestConductor, LocalP2PTestQuicDataChannel) {
  PeerConnectionInterface::RTCConfiguration quic_config;
  quic_config.enable_quic = true;
  ASSERT_TRUE(CreateTestClients(quic_config, quic_config));
  webrtc::DataChannelInit init;
  init.ordered = false;
  init.reliable = true;
  init.id = 1;
  initializing_client()->CreateDataChannel(&init);
  receiving_client()->CreateDataChannel(&init);
  LocalP2PTest();
  ASSERT_NE(nullptr, initializing_client()->data_channel());
  ASSERT_NE(nullptr, receiving_client()->data_channel());
  EXPECT_TRUE_WAIT(initializing_client()->data_observer()->IsOpen(),
                   kMaxWaitMs);
  EXPECT_TRUE_WAIT(receiving_client()->data_observer()->IsOpen(), kMaxWaitMs);

  std::string data = "hello world";

  initializing_client()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, receiving_client()->data_observer()->last_message(),
                 kMaxWaitMs);

  receiving_client()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, initializing_client()->data_observer()->last_message(),
                 kMaxWaitMs);
}

// Tests that negotiation of QUIC data channels is completed without error.
TEST_F(P2PTestConductor, NegotiateQuicDataChannel) {
  PeerConnectionInterface::RTCConfiguration quic_config;
  quic_config.enable_quic = true;
  ASSERT_TRUE(CreateTestClients(quic_config, quic_config));
  FakeConstraints constraints;
  constraints.SetMandatory(MediaConstraintsInterface::kEnableDtlsSrtp, true);
  ASSERT_TRUE(CreateTestClients(&constraints, &constraints));
  webrtc::DataChannelInit init;
  init.ordered = false;
  init.reliable = true;
  init.id = 1;
  initializing_client()->CreateDataChannel(&init);
  initializing_client()->Negotiate(false, false);
}

// This test sets up a JSEP call using QUIC. The callee only receives video.
TEST_F(P2PTestConductor, LocalP2PTestVideoOnlyWithQuic) {
  PeerConnectionInterface::RTCConfiguration quic_config;
  quic_config.enable_quic = true;
  ASSERT_TRUE(CreateTestClients(quic_config, quic_config));
  receiving_client()->SetReceiveAudioVideo(false, true);
  LocalP2PTest();
}

// This test sets up a JSEP call using QUIC. The callee only receives audio.
TEST_F(P2PTestConductor, LocalP2PTestAudioOnlyWithQuic) {
  PeerConnectionInterface::RTCConfiguration quic_config;
  quic_config.enable_quic = true;
  ASSERT_TRUE(CreateTestClients(quic_config, quic_config));
  receiving_client()->SetReceiveAudioVideo(true, false);
  LocalP2PTest();
}

// This test sets up a JSEP call using QUIC. The callee rejects both audio and
// video.
TEST_F(P2PTestConductor, LocalP2PTestNoVideoAudioWithQuic) {
  PeerConnectionInterface::RTCConfiguration quic_config;
  quic_config.enable_quic = true;
  ASSERT_TRUE(CreateTestClients(quic_config, quic_config));
  receiving_client()->SetReceiveAudioVideo(false, false);
  LocalP2PTest();
}

#endif  // HAVE_QUIC

TEST_F(P2PTestConductor, ForwardVideoOnlyStream) {
  ASSERT_TRUE(CreateTestClients());
  // One-way stream
  receiving_client()->set_auto_add_stream(false);
  // Video only, audio forwarding not expected to work.
  initializing_client()->AddMediaStream(false, true);
  initializing_client()->Negotiate();

  ASSERT_TRUE_WAIT(SessionActive(), kMaxWaitForActivationMs);
  VerifySessionDescriptions();

  ASSERT_TRUE(initializing_client()->can_receive_video());
  ASSERT_TRUE(receiving_client()->can_receive_video());

  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 initializing_client()->ice_connection_state(),
                 kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 receiving_client()->ice_connection_state(),
                 kMaxWaitForFramesMs);

  ASSERT_TRUE(receiving_client()->remote_streams()->count() == 1);

  // Echo the stream back.
  receiving_client()->pc()->AddStream(
      receiving_client()->remote_streams()->at(0));
  receiving_client()->Negotiate();

  EXPECT_TRUE_WAIT(
      initializing_client()->VideoFramesReceivedCheck(kEndVideoFrameCount),
      kMaxWaitForFramesMs);
}

// Test that we achieve the expected end-to-end connection time, using a
// fake clock and simulated latency on the media and signaling paths.
// We use a TURN<->TURN connection because this is usually the quickest to
// set up initially, especially when we're confident the connection will work
// and can start sending media before we get a STUN response.
//
// With various optimizations enabled, here are the network delays we expect to
// be on the critical path:
// 1. 2 signaling trips: Signaling offer and offerer's TURN candidate, then
//                       signaling answer (with DTLS fingerprint).
// 2. 9 media hops: Rest of the DTLS handshake. 3 hops in each direction when
//                  using TURN<->TURN pair, and DTLS exchange is 4 packets,
//                  the first of which should have arrived before the answer.
TEST_F(P2PTestConductor, EndToEndConnectionTimeWithTurnTurnPair) {
  rtc::ScopedFakeClock fake_clock;
  // Some things use a time of "0" as a special value, so we need to start out
  // the fake clock at a nonzero time.
  // TODO(deadbeef): Fix this.
  fake_clock.AdvanceTime(rtc::TimeDelta::FromSeconds(1));

  static constexpr int media_hop_delay_ms = 50;
  static constexpr int signaling_trip_delay_ms = 500;
  // For explanation of these values, see comment above.
  static constexpr int required_media_hops = 9;
  static constexpr int required_signaling_trips = 2;
  // For internal delays (such as posting an event asychronously).
  static constexpr int allowed_internal_delay_ms = 20;
  static constexpr int total_connection_time_ms =
      media_hop_delay_ms * required_media_hops +
      signaling_trip_delay_ms * required_signaling_trips +
      allowed_internal_delay_ms;

  static const rtc::SocketAddress turn_server_1_internal_address{"88.88.88.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_1_external_address{"88.88.88.1",
                                                                 0};
  static const rtc::SocketAddress turn_server_2_internal_address{"99.99.99.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_2_external_address{"99.99.99.1",
                                                                 0};
  cricket::TestTurnServer turn_server_1(network_thread(),
                                        turn_server_1_internal_address,
                                        turn_server_1_external_address);
  cricket::TestTurnServer turn_server_2(network_thread(),
                                        turn_server_2_internal_address,
                                        turn_server_2_external_address);
  // Bypass permission check on received packets so media can be sent before
  // the candidate is signaled.
  turn_server_1.set_enable_permission_checks(false);
  turn_server_2.set_enable_permission_checks(false);

  PeerConnectionInterface::RTCConfiguration client_1_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_1;
  ice_server_1.urls.push_back("turn:88.88.88.0:3478");
  ice_server_1.username = "test";
  ice_server_1.password = "test";
  client_1_config.servers.push_back(ice_server_1);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;
  client_1_config.presume_writable_when_fully_relayed = true;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_2;
  ice_server_2.urls.push_back("turn:99.99.99.0:3478");
  ice_server_2.username = "test";
  ice_server_2.password = "test";
  client_2_config.servers.push_back(ice_server_2);
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;
  client_2_config.presume_writable_when_fully_relayed = true;

  ASSERT_TRUE(CreateTestClients(client_1_config, client_2_config));
  // Set up the simulated delays.
  SetSignalingDelayMs(signaling_trip_delay_ms);
  virtual_socket_server()->set_delay_mean(media_hop_delay_ms);
  virtual_socket_server()->UpdateDelayDistribution();

  initializing_client()->SetOfferToReceiveAudioVideo(true, true);
  initializing_client()->Negotiate();
  // TODO(deadbeef): kIceConnectionConnected currently means both ICE and DTLS
  // are connected. This is an important distinction. Once we have separate ICE
  // and DTLS state, this check needs to use the DTLS state.
  EXPECT_TRUE_SIMULATED_WAIT(
      (receiving_client()->ice_connection_state() ==
           webrtc::PeerConnectionInterface::kIceConnectionConnected ||
       receiving_client()->ice_connection_state() ==
           webrtc::PeerConnectionInterface::kIceConnectionCompleted) &&
          (initializing_client()->ice_connection_state() ==
               webrtc::PeerConnectionInterface::kIceConnectionConnected ||
           initializing_client()->ice_connection_state() ==
               webrtc::PeerConnectionInterface::kIceConnectionCompleted),
      total_connection_time_ms, fake_clock);
  // Need to free the clients here since they're using things we created on
  // the stack.
  delete set_initializing_client(nullptr);
  delete set_receiving_client(nullptr);
}

class IceServerParsingTest : public testing::Test {
 public:
  // Convenience for parsing a single URL.
  bool ParseUrl(const std::string& url) {
    return ParseUrl(url, std::string(), std::string());
  }

  bool ParseTurnUrl(const std::string& url) {
    return ParseUrl(url, "username", "password");
  }

  bool ParseUrl(const std::string& url,
                const std::string& username,
                const std::string& password) {
    return ParseUrl(
        url, username, password,
        PeerConnectionInterface::TlsCertPolicy::kTlsCertPolicySecure);
  }

  bool ParseUrl(const std::string& url,
                const std::string& username,
                const std::string& password,
                PeerConnectionInterface::TlsCertPolicy tls_certificate_policy) {
    PeerConnectionInterface::IceServers servers;
    PeerConnectionInterface::IceServer server;
    server.urls.push_back(url);
    server.username = username;
    server.password = password;
    server.tls_cert_policy = tls_certificate_policy;
    servers.push_back(server);
    return webrtc::ParseIceServers(servers, &stun_servers_, &turn_servers_) ==
           webrtc::RTCErrorType::NONE;
  }

 protected:
  cricket::ServerAddresses stun_servers_;
  std::vector<cricket::RelayServerConfig> turn_servers_;
};

// Make sure all STUN/TURN prefixes are parsed correctly.
TEST_F(IceServerParsingTest, ParseStunPrefixes) {
  EXPECT_TRUE(ParseUrl("stun:hostname"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ(0U, turn_servers_.size());
  stun_servers_.clear();

  EXPECT_TRUE(ParseUrl("stuns:hostname"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ(0U, turn_servers_.size());
  stun_servers_.clear();

  EXPECT_TRUE(ParseTurnUrl("turn:hostname"));
  EXPECT_EQ(0U, stun_servers_.size());
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ(cricket::PROTO_UDP, turn_servers_[0].ports[0].proto);
  turn_servers_.clear();

  EXPECT_TRUE(ParseTurnUrl("turns:hostname"));
  EXPECT_EQ(0U, stun_servers_.size());
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ(cricket::PROTO_TLS, turn_servers_[0].ports[0].proto);
  EXPECT_TRUE(turn_servers_[0].tls_cert_policy ==
              cricket::TlsCertPolicy::TLS_CERT_POLICY_SECURE);
  turn_servers_.clear();

  EXPECT_TRUE(ParseUrl(
      "turns:hostname", "username", "password",
      PeerConnectionInterface::TlsCertPolicy::kTlsCertPolicyInsecureNoCheck));
  EXPECT_EQ(0U, stun_servers_.size());
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_TRUE(turn_servers_[0].tls_cert_policy ==
              cricket::TlsCertPolicy::TLS_CERT_POLICY_INSECURE_NO_CHECK);
  EXPECT_EQ(cricket::PROTO_TLS, turn_servers_[0].ports[0].proto);
  turn_servers_.clear();

  // invalid prefixes
  EXPECT_FALSE(ParseUrl("stunn:hostname"));
  EXPECT_FALSE(ParseUrl(":hostname"));
  EXPECT_FALSE(ParseUrl(":"));
  EXPECT_FALSE(ParseUrl(""));
}

TEST_F(IceServerParsingTest, VerifyDefaults) {
  // TURNS defaults
  EXPECT_TRUE(ParseTurnUrl("turns:hostname"));
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ(5349, turn_servers_[0].ports[0].address.port());
  EXPECT_EQ(cricket::PROTO_TLS, turn_servers_[0].ports[0].proto);
  turn_servers_.clear();

  // TURN defaults
  EXPECT_TRUE(ParseTurnUrl("turn:hostname"));
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ(3478, turn_servers_[0].ports[0].address.port());
  EXPECT_EQ(cricket::PROTO_UDP, turn_servers_[0].ports[0].proto);
  turn_servers_.clear();

  // STUN defaults
  EXPECT_TRUE(ParseUrl("stun:hostname"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ(3478, stun_servers_.begin()->port());
  stun_servers_.clear();
}

// Check that the 6 combinations of IPv4/IPv6/hostname and with/without port
// can be parsed correctly.
TEST_F(IceServerParsingTest, ParseHostnameAndPort) {
  EXPECT_TRUE(ParseUrl("stun:1.2.3.4:1234"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ("1.2.3.4", stun_servers_.begin()->hostname());
  EXPECT_EQ(1234, stun_servers_.begin()->port());
  stun_servers_.clear();

  EXPECT_TRUE(ParseUrl("stun:[1:2:3:4:5:6:7:8]:4321"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ("1:2:3:4:5:6:7:8", stun_servers_.begin()->hostname());
  EXPECT_EQ(4321, stun_servers_.begin()->port());
  stun_servers_.clear();

  EXPECT_TRUE(ParseUrl("stun:hostname:9999"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ("hostname", stun_servers_.begin()->hostname());
  EXPECT_EQ(9999, stun_servers_.begin()->port());
  stun_servers_.clear();

  EXPECT_TRUE(ParseUrl("stun:1.2.3.4"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ("1.2.3.4", stun_servers_.begin()->hostname());
  EXPECT_EQ(3478, stun_servers_.begin()->port());
  stun_servers_.clear();

  EXPECT_TRUE(ParseUrl("stun:[1:2:3:4:5:6:7:8]"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ("1:2:3:4:5:6:7:8", stun_servers_.begin()->hostname());
  EXPECT_EQ(3478, stun_servers_.begin()->port());
  stun_servers_.clear();

  EXPECT_TRUE(ParseUrl("stun:hostname"));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ("hostname", stun_servers_.begin()->hostname());
  EXPECT_EQ(3478, stun_servers_.begin()->port());
  stun_servers_.clear();

  // Try some invalid hostname:port strings.
  EXPECT_FALSE(ParseUrl("stun:hostname:99a99"));
  EXPECT_FALSE(ParseUrl("stun:hostname:-1"));
  EXPECT_FALSE(ParseUrl("stun:hostname:port:more"));
  EXPECT_FALSE(ParseUrl("stun:hostname:port more"));
  EXPECT_FALSE(ParseUrl("stun:hostname:"));
  EXPECT_FALSE(ParseUrl("stun:[1:2:3:4:5:6:7:8]junk:1000"));
  EXPECT_FALSE(ParseUrl("stun::5555"));
  EXPECT_FALSE(ParseUrl("stun:"));
}

// Test parsing the "?transport=xxx" part of the URL.
TEST_F(IceServerParsingTest, ParseTransport) {
  EXPECT_TRUE(ParseTurnUrl("turn:hostname:1234?transport=tcp"));
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ(cricket::PROTO_TCP, turn_servers_[0].ports[0].proto);
  turn_servers_.clear();

  EXPECT_TRUE(ParseTurnUrl("turn:hostname?transport=udp"));
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ(cricket::PROTO_UDP, turn_servers_[0].ports[0].proto);
  turn_servers_.clear();

  EXPECT_FALSE(ParseTurnUrl("turn:hostname?transport=invalid"));
  EXPECT_FALSE(ParseTurnUrl("turn:hostname?transport="));
  EXPECT_FALSE(ParseTurnUrl("turn:hostname?="));
  EXPECT_FALSE(ParseTurnUrl("turn:hostname?"));
  EXPECT_FALSE(ParseTurnUrl("?"));
}

// Test parsing ICE username contained in URL.
TEST_F(IceServerParsingTest, ParseUsername) {
  EXPECT_TRUE(ParseTurnUrl("turn:user@hostname"));
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ("user", turn_servers_[0].credentials.username);
  turn_servers_.clear();

  EXPECT_FALSE(ParseTurnUrl("turn:@hostname"));
  EXPECT_FALSE(ParseTurnUrl("turn:username@"));
  EXPECT_FALSE(ParseTurnUrl("turn:@"));
  EXPECT_FALSE(ParseTurnUrl("turn:user@name@hostname"));
}

// Test that username and password from IceServer is copied into the resulting
// RelayServerConfig.
TEST_F(IceServerParsingTest, CopyUsernameAndPasswordFromIceServer) {
  EXPECT_TRUE(ParseUrl("turn:hostname", "username", "password"));
  EXPECT_EQ(1U, turn_servers_.size());
  EXPECT_EQ("username", turn_servers_[0].credentials.username);
  EXPECT_EQ("password", turn_servers_[0].credentials.password);
}

// Ensure that if a server has multiple URLs, each one is parsed.
TEST_F(IceServerParsingTest, ParseMultipleUrls) {
  PeerConnectionInterface::IceServers servers;
  PeerConnectionInterface::IceServer server;
  server.urls.push_back("stun:hostname");
  server.urls.push_back("turn:hostname");
  server.username = "foo";
  server.password = "bar";
  servers.push_back(server);
  EXPECT_EQ(webrtc::RTCErrorType::NONE,
            webrtc::ParseIceServers(servers, &stun_servers_, &turn_servers_));
  EXPECT_EQ(1U, stun_servers_.size());
  EXPECT_EQ(1U, turn_servers_.size());
}

// Ensure that TURN servers are given unique priorities,
// so that their resulting candidates have unique priorities.
TEST_F(IceServerParsingTest, TurnServerPrioritiesUnique) {
  PeerConnectionInterface::IceServers servers;
  PeerConnectionInterface::IceServer server;
  server.urls.push_back("turn:hostname");
  server.urls.push_back("turn:hostname2");
  server.username = "foo";
  server.password = "bar";
  servers.push_back(server);
  EXPECT_EQ(webrtc::RTCErrorType::NONE,
            webrtc::ParseIceServers(servers, &stun_servers_, &turn_servers_));
  EXPECT_EQ(2U, turn_servers_.size());
  EXPECT_NE(turn_servers_[0].priority, turn_servers_[1].priority);
}

#endif // if !defined(THREAD_SANITIZER)

}  // namespace
