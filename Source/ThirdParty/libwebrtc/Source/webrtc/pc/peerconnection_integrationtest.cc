/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
#if !defined(THREAD_SANITIZER)

#include <stdio.h>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/fakemetricsobserver.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "api/peerconnectionproxy.h"
#include "api/rtpreceiverinterface.h"
#include "api/test/fakeconstraints.h"
#include "media/engine/fakewebrtcvideoengine.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/portinterface.h"
#include "p2p/base/teststunserver.h"
#include "p2p/base/testturncustomizer.h"
#include "p2p/base/testturnserver.h"
#include "p2p/client/basicportallocator.h"
#include "pc/dtmfsender.h"
#include "pc/localaudiosource.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/sessiondescription.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakeperiodicvideocapturer.h"
#include "pc/test/fakertccertificategenerator.h"
#include "pc/test/fakevideotrackrenderer.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/firewallsocketserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

using cricket::ContentInfo;
using cricket::FakeWebRtcVideoDecoder;
using cricket::FakeWebRtcVideoDecoderFactory;
using cricket::FakeWebRtcVideoEncoder;
using cricket::FakeWebRtcVideoEncoderFactory;
using cricket::MediaContentDescription;
using rtc::SocketAddress;
using ::testing::ElementsAre;
using ::testing::Values;
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
using webrtc::PeerConnection;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionFactory;
using webrtc::PeerConnectionProxy;
using webrtc::RtpReceiverInterface;
using webrtc::SdpType;
using webrtc::SessionDescriptionInterface;
using webrtc::StreamCollectionInterface;

namespace {

static const int kDefaultTimeout = 10000;
static const int kMaxWaitForStatsMs = 3000;
static const int kMaxWaitForActivationMs = 5000;
static const int kMaxWaitForFramesMs = 10000;
// Default number of audio/video frames to wait for before considering a test
// successful.
static const int kDefaultExpectedAudioFrameCount = 3;
static const int kDefaultExpectedVideoFrameCount = 3;

static const char kDataChannelLabel[] = "data_channel";

// SRTP cipher name negotiated by the tests. This must be updated if the
// default changes.
static const int kDefaultSrtpCryptoSuite = rtc::SRTP_AES128_CM_SHA1_32;
static const int kDefaultSrtpCryptoSuiteGcm = rtc::SRTP_AEAD_AES_256_GCM;

static const SocketAddress kDefaultLocalAddress("192.168.1.1", 0);

// Helper function for constructing offer/answer options to initiate an ICE
// restart.
PeerConnectionInterface::RTCOfferAnswerOptions IceRestartOfferAnswerOptions() {
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.ice_restart = true;
  return options;
}

// Remove all stream information (SSRCs, track IDs, etc.) and "msid-semantic"
// attribute from received SDP, simulating a legacy endpoint.
void RemoveSsrcsAndMsids(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    content.media_description()->mutable_streams().clear();
  }
  desc->set_msid_supported(false);
}

int FindFirstMediaStatsIndexByKind(
    const std::string& kind,
    const std::vector<const webrtc::RTCMediaStreamTrackStats*>&
        media_stats_vec) {
  for (size_t i = 0; i < media_stats_vec.size(); i++) {
    if (media_stats_vec[i]->kind.ValueToString() == kind) {
      return i;
    }
  }
  return -1;
}

class SignalingMessageReceiver {
 public:
  virtual void ReceiveSdpMessage(SdpType type, const std::string& msg) = 0;
  virtual void ReceiveIceMessage(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& msg) = 0;

 protected:
  SignalingMessageReceiver() {}
  virtual ~SignalingMessageReceiver() {}
};

class MockRtpReceiverObserver : public webrtc::RtpReceiverObserverInterface {
 public:
  explicit MockRtpReceiverObserver(cricket::MediaType media_type)
      : expected_media_type_(media_type) {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override {
    ASSERT_EQ(expected_media_type_, media_type);
    first_packet_received_ = true;
  }

  bool first_packet_received() const { return first_packet_received_; }

  virtual ~MockRtpReceiverObserver() {}

 private:
  bool first_packet_received_ = false;
  cricket::MediaType expected_media_type_;
};

// Helper class that wraps a peer connection, observes it, and can accept
// signaling messages from another wrapper.
//
// Uses a fake network, fake A/V capture, and optionally fake
// encoders/decoders, though they aren't used by default since they don't
// advertise support of any codecs.
// TODO(steveanton): See how this could become a subclass of
// PeerConnectionWrapper defined in peerconnectionwrapper.h .
class PeerConnectionWrapper : public webrtc::PeerConnectionObserver,
                              public SignalingMessageReceiver,
                              public ObserverInterface {
 public:
  // Different factory methods for convenience.
  // TODO(deadbeef): Could use the pattern of:
  //
  // PeerConnectionWrapper =
  //     WrapperBuilder.WithConfig(...).WithOptions(...).build();
  //
  // To reduce some code duplication.
  static PeerConnectionWrapper* CreateWithDtlsIdentityStore(
      const std::string& debug_name,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    PeerConnectionWrapper* client(new PeerConnectionWrapper(debug_name));
    if (!client->Init(nullptr, nullptr, nullptr, std::move(cert_generator),
                      network_thread, worker_thread)) {
      delete client;
      return nullptr;
    }
    return client;
  }

  static PeerConnectionWrapper* CreateWithConfig(
      const std::string& debug_name,
      const PeerConnectionInterface::RTCConfiguration& config,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    PeerConnectionWrapper* client(new PeerConnectionWrapper(debug_name));
    if (!client->Init(nullptr, nullptr, &config, std::move(cert_generator),
                      network_thread, worker_thread)) {
      delete client;
      return nullptr;
    }
    return client;
  }

  static PeerConnectionWrapper* CreateWithOptions(
      const std::string& debug_name,
      const PeerConnectionFactory::Options& options,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    PeerConnectionWrapper* client(new PeerConnectionWrapper(debug_name));
    if (!client->Init(nullptr, &options, nullptr, std::move(cert_generator),
                      network_thread, worker_thread)) {
      delete client;
      return nullptr;
    }
    return client;
  }

  static PeerConnectionWrapper* CreateWithConstraints(
      const std::string& debug_name,
      const MediaConstraintsInterface* constraints,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    PeerConnectionWrapper* client(new PeerConnectionWrapper(debug_name));
    if (!client->Init(constraints, nullptr, nullptr, std::move(cert_generator),
                      network_thread, worker_thread)) {
      delete client;
      return nullptr;
    }
    return client;
  }

  webrtc::PeerConnectionFactoryInterface* pc_factory() const {
    return peer_connection_factory_.get();
  }

  webrtc::PeerConnectionInterface* pc() const { return peer_connection_.get(); }

  // If a signaling message receiver is set (via ConnectFakeSignaling), this
  // will set the whole offer/answer exchange in motion. Just need to wait for
  // the signaling state to reach "stable".
  void CreateAndSetAndSignalOffer() {
    auto offer = CreateOffer();
    ASSERT_NE(nullptr, offer);
    EXPECT_TRUE(SetLocalDescriptionAndSendSdpMessage(std::move(offer)));
  }

  // Sets the options to be used when CreateAndSetAndSignalOffer is called, or
  // when a remote offer is received (via fake signaling) and an answer is
  // generated. By default, uses default options.
  void SetOfferAnswerOptions(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
    offer_answer_options_ = options;
  }

  // Set a callback to be invoked when SDP is received via the fake signaling
  // channel, which provides an opportunity to munge (modify) the SDP. This is
  // used to test SDP being applied that a PeerConnection would normally not
  // generate, but a non-JSEP endpoint might.
  void SetReceivedSdpMunger(
      std::function<void(cricket::SessionDescription*)> munger) {
    received_sdp_munger_ = std::move(munger);
  }

  // Similar to the above, but this is run on SDP immediately after it's
  // generated.
  void SetGeneratedSdpMunger(
      std::function<void(cricket::SessionDescription*)> munger) {
    generated_sdp_munger_ = std::move(munger);
  }

  // Every ICE connection state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceConnectionState>
  ice_connection_state_history() const {
    return ice_connection_state_history_;
  }
  void clear_ice_connection_state_history() {
    ice_connection_state_history_.clear();
  }

  // Every ICE gathering state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceGatheringState>
  ice_gathering_state_history() const {
    return ice_gathering_state_history_;
  }

  // TODO(deadbeef): Switch the majority of these tests to use AddTrack instead
  // of AddStream since AddStream is deprecated.
  void AddAudioVideoMediaStream() {
    AddMediaStreamFromTracks(CreateLocalAudioTrack(), CreateLocalVideoTrack());
  }

  void AddAudioOnlyMediaStream() {
    AddMediaStreamFromTracks(CreateLocalAudioTrack(), nullptr);
  }

  void AddVideoOnlyMediaStream() {
    AddMediaStreamFromTracks(nullptr, CreateLocalVideoTrack());
  }

  rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateLocalAudioTrack() {
    FakeConstraints constraints;
    // Disable highpass filter so that we can get all the test audio frames.
    constraints.AddMandatory(MediaConstraintsInterface::kHighpassFilter, false);
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(&constraints);
    // TODO(perkj): Test audio source when it is implemented. Currently audio
    // always use the default input.
    return peer_connection_factory_->CreateAudioTrack(rtc::CreateRandomUuid(),
                                                      source);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack() {
    return CreateLocalVideoTrackInternal(FakeConstraints(),
                                         webrtc::kVideoRotation_0);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackWithConstraints(const FakeConstraints& constraints) {
    return CreateLocalVideoTrackInternal(constraints, webrtc::kVideoRotation_0);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackWithRotation(webrtc::VideoRotation rotation) {
    return CreateLocalVideoTrackInternal(FakeConstraints(), rotation);
  }

  void AddMediaStreamFromTracks(
      const rtc::scoped_refptr<webrtc::AudioTrackInterface>& audio,
      const rtc::scoped_refptr<webrtc::VideoTrackInterface>& video) {
    rtc::scoped_refptr<MediaStreamInterface> stream =
        peer_connection_factory_->CreateLocalMediaStream(
            rtc::CreateRandomUuid());
    if (audio) {
      stream->AddTrack(audio);
    }
    if (video) {
      stream->AddTrack(video);
    }
    EXPECT_TRUE(pc()->AddStream(stream));
  }

  bool SignalingStateStable() {
    return pc()->signaling_state() == webrtc::PeerConnectionInterface::kStable;
  }

  void CreateDataChannel() { CreateDataChannel(nullptr); }

  void CreateDataChannel(const webrtc::DataChannelInit* init) {
    CreateDataChannel(kDataChannelLabel, init);
  }

  void CreateDataChannel(const std::string& label,
                         const webrtc::DataChannelInit* init) {
    data_channel_ = pc()->CreateDataChannel(label, init);
    ASSERT_TRUE(data_channel_.get() != nullptr);
    data_observer_.reset(new MockDataChannelObserver(data_channel_));
  }

  DataChannelInterface* data_channel() { return data_channel_; }
  const MockDataChannelObserver* data_observer() const {
    return data_observer_.get();
  }

  int audio_frames_received() const {
    return fake_audio_capture_module_->frames_received();
  }

  // Takes minimum of video frames received for each track.
  //
  // Can be used like:
  // EXPECT_GE(expected_frames, min_video_frames_received_per_track());
  //
  // To ensure that all video tracks received at least a certain number of
  // frames.
  int min_video_frames_received_per_track() const {
    int min_frames = INT_MAX;
    if (video_decoder_factory_enabled_) {
      const std::vector<FakeWebRtcVideoDecoder*>& decoders =
          fake_video_decoder_factory_->decoders();
      if (decoders.empty()) {
        return 0;
      }
      for (FakeWebRtcVideoDecoder* decoder : decoders) {
        min_frames = std::min(min_frames, decoder->GetNumFramesReceived());
      }
      return min_frames;
    } else {
      if (fake_video_renderers_.empty()) {
        return 0;
      }

      for (const auto& pair : fake_video_renderers_) {
        min_frames = std::min(min_frames, pair.second->num_rendered_frames());
      }
      return min_frames;
    }
  }

  // In contrast to the above, sums the video frames received for all tracks.
  // Can be used to verify that no video frames were received, or that the
  // counts didn't increase.
  int total_video_frames_received() const {
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

  // Returns a MockStatsObserver in a state after stats gathering finished,
  // which can be used to access the gathered stats.
  rtc::scoped_refptr<MockStatsObserver> OldGetStatsForTrack(
      webrtc::MediaStreamTrackInterface* track) {
    rtc::scoped_refptr<MockStatsObserver> observer(
        new rtc::RefCountedObject<MockStatsObserver>());
    EXPECT_TRUE(peer_connection_->GetStats(
        observer, nullptr, PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer;
  }

  // Version that doesn't take a track "filter", and gathers all stats.
  rtc::scoped_refptr<MockStatsObserver> OldGetStats() {
    return OldGetStatsForTrack(nullptr);
  }

  // Synchronously gets stats and returns them. If it times out, fails the test
  // and returns null.
  rtc::scoped_refptr<const webrtc::RTCStatsReport> NewGetStats() {
    rtc::scoped_refptr<webrtc::MockRTCStatsCollectorCallback> callback(
        new rtc::RefCountedObject<webrtc::MockRTCStatsCollectorCallback>());
    peer_connection_->GetStats(callback);
    EXPECT_TRUE_WAIT(callback->called(), kDefaultTimeout);
    return callback->report();
  }

  int rendered_width() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? 0
               : fake_video_renderers_.begin()->second->width();
  }

  int rendered_height() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? 0
               : fake_video_renderers_.begin()->second->height();
  }

  double rendered_aspect_ratio() {
    if (rendered_height() == 0) {
      return 0.0;
    }
    return static_cast<double>(rendered_width()) / rendered_height();
  }

  webrtc::VideoRotation rendered_rotation() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? webrtc::kVideoRotation_0
               : fake_video_renderers_.begin()->second->rotation();
  }

  int local_rendered_width() {
    return local_video_renderer_ ? local_video_renderer_->width() : 0;
  }

  int local_rendered_height() {
    return local_video_renderer_ ? local_video_renderer_->height() : 0;
  }

  double local_rendered_aspect_ratio() {
    if (local_rendered_height() == 0) {
      return 0.0;
    }
    return static_cast<double>(local_rendered_width()) /
           local_rendered_height();
  }

  size_t number_of_remote_streams() {
    if (!pc()) {
      return 0;
    }
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

  webrtc::PeerConnectionInterface::SignalingState signaling_state() {
    return pc()->signaling_state();
  }

  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state() {
    return pc()->ice_connection_state();
  }

  webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state() {
    return pc()->ice_gathering_state();
  }

  // Returns a MockRtpReceiverObserver for each RtpReceiver returned by
  // GetReceivers. They're updated automatically when a remote offer/answer
  // from the fake signaling channel is applied, or when
  // ResetRtpReceiverObservers below is called.
  const std::vector<std::unique_ptr<MockRtpReceiverObserver>>&
  rtp_receiver_observers() {
    return rtp_receiver_observers_;
  }

  void ResetRtpReceiverObservers() {
    rtp_receiver_observers_.clear();
    for (const rtc::scoped_refptr<RtpReceiverInterface>& receiver :
         pc()->GetReceivers()) {
      std::unique_ptr<MockRtpReceiverObserver> observer(
          new MockRtpReceiverObserver(receiver->media_type()));
      receiver->SetObserver(observer.get());
      rtp_receiver_observers_.push_back(std::move(observer));
    }
  }

  rtc::FakeNetworkManager* network() const {
    return fake_network_manager_.get();
  }
  cricket::PortAllocator* port_allocator() const { return port_allocator_; }

 private:
  explicit PeerConnectionWrapper(const std::string& debug_name)
      : debug_name_(debug_name) {}

  bool Init(
      const MediaConstraintsInterface* constraints,
      const PeerConnectionFactory::Options* options,
      const PeerConnectionInterface::RTCConfiguration* config,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread) {
    // There's an error in this test code if Init ends up being called twice.
    RTC_DCHECK(!peer_connection_);
    RTC_DCHECK(!peer_connection_factory_);

    fake_network_manager_.reset(new rtc::FakeNetworkManager());
    fake_network_manager_->AddInterface(kDefaultLocalAddress);

    std::unique_ptr<cricket::PortAllocator> port_allocator(
        new cricket::BasicPortAllocator(fake_network_manager_.get()));
    port_allocator_ = port_allocator.get();
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create();
    if (!fake_audio_capture_module_) {
      return false;
    }
    // Note that these factories don't end up getting used unless supported
    // codecs are added to them.
    fake_video_decoder_factory_ = new FakeWebRtcVideoDecoderFactory();
    fake_video_encoder_factory_ = new FakeWebRtcVideoEncoderFactory();
    rtc::Thread* const signaling_thread = rtc::Thread::Current();
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread, worker_thread, signaling_thread,
        fake_audio_capture_module_, webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(), fake_video_encoder_factory_,
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
    PeerConnectionInterface::RTCConfiguration modified_config;
    // If |config| is null, this will result in a default configuration being
    // used.
    if (config) {
      modified_config = *config;
    }
    // Disable resolution adaptation; we don't want it interfering with the
    // test results.
    // TODO(deadbeef): Do something more robust. Since we're testing for aspect
    // ratios and not specific resolutions, is this even necessary?
    modified_config.set_cpu_adaptation(false);

    return peer_connection_factory_->CreatePeerConnection(
        modified_config, constraints, std::move(port_allocator),
        std::move(cert_generator), this);
  }

  void set_signaling_message_receiver(
      SignalingMessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  void set_signaling_delay_ms(int delay_ms) { signaling_delay_ms_ = delay_ms; }

  void set_signal_ice_candidates(bool signal) {
    signal_ice_candidates_ = signal;
  }

  void EnableVideoDecoderFactory() {
    video_decoder_factory_enabled_ = true;
    fake_video_decoder_factory_->AddSupportedVideoCodecType(
        webrtc::kVideoCodecVP8);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrackInternal(
      const FakeConstraints& constraints,
      webrtc::VideoRotation rotation) {
    // Set max frame rate to 10fps to reduce the risk of test flakiness.
    // TODO(deadbeef): Do something more robust.
    FakeConstraints source_constraints = constraints;
    source_constraints.SetMandatoryMaxFrameRate(10);

    cricket::FakeVideoCapturer* fake_capturer =
        new webrtc::FakePeriodicVideoCapturer();
    fake_capturer->SetRotation(rotation);
    video_capturers_.push_back(fake_capturer);
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source =
        peer_connection_factory_->CreateVideoSource(fake_capturer,
                                                    &source_constraints);
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track(
        peer_connection_factory_->CreateVideoTrack(rtc::CreateRandomUuid(),
                                                   source));
    if (!local_video_renderer_) {
      local_video_renderer_.reset(new webrtc::FakeVideoTrackRenderer(track));
    }
    return track;
  }

  void HandleIncomingOffer(const std::string& msg) {
    RTC_LOG(LS_INFO) << debug_name_ << ": HandleIncomingOffer";
    std::unique_ptr<SessionDescriptionInterface> desc =
        webrtc::CreateSessionDescription(SdpType::kOffer, msg);
    if (received_sdp_munger_) {
      received_sdp_munger_(desc->description());
    }

    EXPECT_TRUE(SetRemoteDescription(std::move(desc)));
    // Setting a remote description may have changed the number of receivers,
    // so reset the receiver observers.
    ResetRtpReceiverObservers();
    auto answer = CreateAnswer();
    ASSERT_NE(nullptr, answer);
    EXPECT_TRUE(SetLocalDescriptionAndSendSdpMessage(std::move(answer)));
  }

  void HandleIncomingAnswer(const std::string& msg) {
    RTC_LOG(LS_INFO) << debug_name_ << ": HandleIncomingAnswer";
    std::unique_ptr<SessionDescriptionInterface> desc =
        webrtc::CreateSessionDescription(SdpType::kAnswer, msg);
    if (received_sdp_munger_) {
      received_sdp_munger_(desc->description());
    }

    EXPECT_TRUE(SetRemoteDescription(std::move(desc)));
    // Set the RtpReceiverObserver after receivers are created.
    ResetRtpReceiverObservers();
  }

  // Returns null on failure.
  std::unique_ptr<SessionDescriptionInterface> CreateOffer() {
    rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
    pc()->CreateOffer(observer, offer_answer_options_);
    return WaitForDescriptionFromObserver(observer);
  }

  // Returns null on failure.
  std::unique_ptr<SessionDescriptionInterface> CreateAnswer() {
    rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
    pc()->CreateAnswer(observer, offer_answer_options_);
    return WaitForDescriptionFromObserver(observer);
  }

  std::unique_ptr<SessionDescriptionInterface> WaitForDescriptionFromObserver(
      MockCreateSessionDescriptionObserver* observer) {
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout);
    if (!observer->result()) {
      return nullptr;
    }
    auto description = observer->MoveDescription();
    if (generated_sdp_munger_) {
      generated_sdp_munger_(description->description());
    }
    return description;
  }

  // Setting the local description and sending the SDP message over the fake
  // signaling channel are combined into the same method because the SDP
  // message needs to be sent as soon as SetLocalDescription finishes, without
  // waiting for the observer to be called. This ensures that ICE candidates
  // don't outrace the description.
  bool SetLocalDescriptionAndSendSdpMessage(
      std::unique_ptr<SessionDescriptionInterface> desc) {
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
    RTC_LOG(LS_INFO) << debug_name_ << ": SetLocalDescriptionAndSendSdpMessage";
    SdpType type = desc->GetType();
    std::string sdp;
    EXPECT_TRUE(desc->ToString(&sdp));
    pc()->SetLocalDescription(observer, desc.release());
    // As mentioned above, we need to send the message immediately after
    // SetLocalDescription.
    SendSdpMessage(type, sdp);
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return true;
  }

  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc) {
    rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
    RTC_LOG(LS_INFO) << debug_name_ << ": SetRemoteDescription";
    pc()->SetRemoteDescription(observer, desc.release());
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer->result();
  }

  // Simulate sending a blob of SDP with delay |signaling_delay_ms_| (0 by
  // default).
  void SendSdpMessage(SdpType type, const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelaySdpMessageIfReceiverExists(type, msg);
    } else {
      invoker_.AsyncInvokeDelayed<void>(
          RTC_FROM_HERE, rtc::Thread::Current(),
          rtc::Bind(&PeerConnectionWrapper::RelaySdpMessageIfReceiverExists,
                    this, type, msg),
          signaling_delay_ms_);
    }
  }

  void RelaySdpMessageIfReceiverExists(SdpType type, const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveSdpMessage(type, msg);
    }
  }

  // Simulate trickling an ICE candidate with delay |signaling_delay_ms_| (0 by
  // default).
  void SendIceMessage(const std::string& sdp_mid,
                      int sdp_mline_index,
                      const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelayIceMessageIfReceiverExists(sdp_mid, sdp_mline_index, msg);
    } else {
      invoker_.AsyncInvokeDelayed<void>(
          RTC_FROM_HERE, rtc::Thread::Current(),
          rtc::Bind(&PeerConnectionWrapper::RelayIceMessageIfReceiverExists,
                    this, sdp_mid, sdp_mline_index, msg),
          signaling_delay_ms_);
    }
  }

  void RelayIceMessageIfReceiverExists(const std::string& sdp_mid,
                                       int sdp_mline_index,
                                       const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveIceMessage(sdp_mid, sdp_mline_index,
                                                     msg);
    }
  }

  // SignalingMessageReceiver callbacks.
  void ReceiveSdpMessage(SdpType type, const std::string& msg) override {
    if (type == SdpType::kOffer) {
      HandleIncomingOffer(msg);
    } else {
      HandleIncomingAnswer(msg);
    }
  }

  void ReceiveIceMessage(const std::string& sdp_mid,
                         int sdp_mline_index,
                         const std::string& msg) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": ReceiveIceMessage";
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
    ice_connection_state_history_.push_back(new_state);
  }
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    EXPECT_EQ(pc()->ice_gathering_state(), new_state);
    ice_gathering_state_history_.push_back(new_state);
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnIceCandidate";

    std::string ice_sdp;
    EXPECT_TRUE(candidate->ToString(&ice_sdp));
    if (signaling_message_receiver_ == nullptr || !signal_ice_candidates_) {
      // Remote party may be deleted.
      return;
    }
    SendIceMessage(candidate->sdp_mid(), candidate->sdp_mline_index(), ice_sdp);
  }
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnDataChannel";
    data_channel_ = data_channel;
    data_observer_.reset(new MockDataChannelObserver(data_channel));
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

  std::string debug_name_;

  std::unique_ptr<rtc::FakeNetworkManager> fake_network_manager_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;

  cricket::PortAllocator* port_allocator_;
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

  // For remote peer communication.
  SignalingMessageReceiver* signaling_message_receiver_ = nullptr;
  int signaling_delay_ms_ = 0;
  bool signal_ice_candidates_ = true;

  // Store references to the video capturers we've created, so that we can stop
  // them, if required.
  std::vector<cricket::FakeVideoCapturer*> video_capturers_;
  // |local_video_renderer_| attached to the first created local video track.
  std::unique_ptr<webrtc::FakeVideoTrackRenderer> local_video_renderer_;

  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options_;
  std::function<void(cricket::SessionDescription*)> received_sdp_munger_;
  std::function<void(cricket::SessionDescription*)> generated_sdp_munger_;

  rtc::scoped_refptr<DataChannelInterface> data_channel_;
  std::unique_ptr<MockDataChannelObserver> data_observer_;

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> rtp_receiver_observers_;

  std::vector<PeerConnectionInterface::IceConnectionState>
      ice_connection_state_history_;
  std::vector<PeerConnectionInterface::IceGatheringState>
      ice_gathering_state_history_;

  rtc::AsyncInvoker invoker_;

  friend class PeerConnectionIntegrationTest;
};

class MockRtcEventLogOutput : public webrtc::RtcEventLogOutput {
 public:
  virtual ~MockRtcEventLogOutput() = default;
  MOCK_CONST_METHOD0(IsActive, bool());
  MOCK_METHOD1(Write, bool(const std::string&));
};

// Tests two PeerConnections connecting to each other end-to-end, using a
// virtual network, fake A/V capture and fake encoder/decoders. The
// PeerConnections share the threads/socket servers, but use separate versions
// of everything else (including "PeerConnectionFactory"s).
class PeerConnectionIntegrationTest : public testing::Test {
 public:
  PeerConnectionIntegrationTest()
      : ss_(new rtc::VirtualSocketServer()),
        fss_(new rtc::FirewallSocketServer(ss_.get())),
        network_thread_(new rtc::Thread(fss_.get())),
        worker_thread_(rtc::Thread::Create()) {
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());
  }

  ~PeerConnectionIntegrationTest() {
    if (caller_) {
      caller_->set_signaling_message_receiver(nullptr);
    }
    if (callee_) {
      callee_->set_signaling_message_receiver(nullptr);
    }
  }

  bool SignalingStateStable() {
    return caller_->SignalingStateStable() && callee_->SignalingStateStable();
  }

  bool DtlsConnected() {
    // TODO(deadbeef): kIceConnectionConnected currently means both ICE and DTLS
    // are connected. This is an important distinction. Once we have separate
    // ICE and DTLS state, this check needs to use the DTLS state.
    return (callee()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionConnected ||
            callee()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionCompleted) &&
           (caller()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionConnected ||
            caller()->ice_connection_state() ==
                webrtc::PeerConnectionInterface::kIceConnectionCompleted);
  }

  bool CreatePeerConnectionWrappers() {
    return CreatePeerConnectionWrappersWithConfig(
        PeerConnectionInterface::RTCConfiguration(),
        PeerConnectionInterface::RTCConfiguration());
  }

  bool CreatePeerConnectionWrappersWithConstraints(
      MediaConstraintsInterface* caller_constraints,
      MediaConstraintsInterface* callee_constraints) {
    caller_.reset(PeerConnectionWrapper::CreateWithConstraints(
        "Caller", caller_constraints, network_thread_.get(),
        worker_thread_.get()));
    callee_.reset(PeerConnectionWrapper::CreateWithConstraints(
        "Callee", callee_constraints, network_thread_.get(),
        worker_thread_.get()));
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfig(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      const PeerConnectionInterface::RTCConfiguration& callee_config) {
    caller_.reset(PeerConnectionWrapper::CreateWithConfig(
        "Caller", caller_config, network_thread_.get(), worker_thread_.get()));
    callee_.reset(PeerConnectionWrapper::CreateWithConfig(
        "Callee", callee_config, network_thread_.get(), worker_thread_.get()));
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithOptions(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options) {
    caller_.reset(PeerConnectionWrapper::CreateWithOptions(
        "Caller", caller_options, network_thread_.get(), worker_thread_.get()));
    callee_.reset(PeerConnectionWrapper::CreateWithOptions(
        "Callee", callee_options, network_thread_.get(), worker_thread_.get()));
    return caller_ && callee_;
  }

  PeerConnectionWrapper* CreatePeerConnectionWrapperWithAlternateKey() {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    cert_generator->use_alternate_key();

    // Make sure the new client is using a different certificate.
    return PeerConnectionWrapper::CreateWithDtlsIdentityStore(
        "New Peer", std::move(cert_generator), network_thread_.get(),
        worker_thread_.get());
  }

  // Once called, SDP blobs and ICE candidates will be automatically signaled
  // between PeerConnections.
  void ConnectFakeSignaling() {
    caller_->set_signaling_message_receiver(callee_.get());
    callee_->set_signaling_message_receiver(caller_.get());
  }

  // Once called, SDP blobs will be automatically signaled between
  // PeerConnections. Note that ICE candidates will not be signaled unless they
  // are in the exchanged SDP blobs.
  void ConnectFakeSignalingForSdpOnly() {
    ConnectFakeSignaling();
    SetSignalIceCandidates(false);
  }

  void SetSignalingDelayMs(int delay_ms) {
    caller_->set_signaling_delay_ms(delay_ms);
    callee_->set_signaling_delay_ms(delay_ms);
  }

  void SetSignalIceCandidates(bool signal) {
    caller_->set_signal_ice_candidates(signal);
    callee_->set_signal_ice_candidates(signal);
  }

  void EnableVideoDecoderFactory() {
    caller_->EnableVideoDecoderFactory();
    callee_->EnableVideoDecoderFactory();
  }

  // Messages may get lost on the unreliable DataChannel, so we send multiple
  // times to avoid test flakiness.
  void SendRtpDataWithRetries(webrtc::DataChannelInterface* dc,
                              const std::string& data,
                              int retries) {
    for (int i = 0; i < retries; ++i) {
      dc->Send(DataBuffer(data));
    }
  }

  rtc::Thread* network_thread() { return network_thread_.get(); }

  rtc::VirtualSocketServer* virtual_socket_server() { return ss_.get(); }

  PeerConnectionWrapper* caller() { return caller_.get(); }

  // Set the |caller_| to the |wrapper| passed in and return the
  // original |caller_|.
  PeerConnectionWrapper* SetCallerPcWrapperAndReturnCurrent(
      PeerConnectionWrapper* wrapper) {
    PeerConnectionWrapper* old = caller_.release();
    caller_.reset(wrapper);
    return old;
  }

  PeerConnectionWrapper* callee() { return callee_.get(); }

  // Set the |callee_| to the |wrapper| passed in and return the
  // original |callee_|.
  PeerConnectionWrapper* SetCalleePcWrapperAndReturnCurrent(
      PeerConnectionWrapper* wrapper) {
    PeerConnectionWrapper* old = callee_.release();
    callee_.reset(wrapper);
    return old;
  }

  rtc::FirewallSocketServer* firewall() const { return fss_.get(); }

  // Expects the provided number of new frames to be received within |wait_ms|.
  // "New frames" meaning that it waits for the current frame counts to
  // *increase* by the provided values. For video, uses
  // RecievedVideoFramesForEachTrack for the case of multiple video tracks
  // being received.
  void ExpectNewFramesReceivedWithWait(
      int expected_caller_received_audio_frames,
      int expected_caller_received_video_frames,
      int expected_callee_received_audio_frames,
      int expected_callee_received_video_frames,
      int wait_ms) {
    // Add current frame counts to the provided values, in order to wait for
    // the frame count to increase.
    expected_caller_received_audio_frames += caller()->audio_frames_received();
    expected_caller_received_video_frames +=
        caller()->min_video_frames_received_per_track();
    expected_callee_received_audio_frames += callee()->audio_frames_received();
    expected_callee_received_video_frames +=
        callee()->min_video_frames_received_per_track();

    EXPECT_TRUE_WAIT(caller()->audio_frames_received() >=
                             expected_caller_received_audio_frames &&
                         caller()->min_video_frames_received_per_track() >=
                             expected_caller_received_video_frames &&
                         callee()->audio_frames_received() >=
                             expected_callee_received_audio_frames &&
                         callee()->min_video_frames_received_per_track() >=
                             expected_callee_received_video_frames,
                     wait_ms);

    // After the combined wait, do an "expect" for each individual count, to
    // print out a more detailed message upon failure.
    EXPECT_GE(caller()->audio_frames_received(),
              expected_caller_received_audio_frames);
    EXPECT_GE(caller()->min_video_frames_received_per_track(),
              expected_caller_received_video_frames);
    EXPECT_GE(callee()->audio_frames_received(),
              expected_callee_received_audio_frames);
    EXPECT_GE(callee()->min_video_frames_received_per_track(),
              expected_callee_received_video_frames);
  }

  void TestGcmNegotiationUsesCipherSuite(bool local_gcm_enabled,
                                         bool remote_gcm_enabled,
                                         int expected_cipher_suite) {
    PeerConnectionFactory::Options caller_options;
    caller_options.crypto_options.enable_gcm_crypto_suites = local_gcm_enabled;
    PeerConnectionFactory::Options callee_options;
    callee_options.crypto_options.enable_gcm_crypto_suites = remote_gcm_enabled;
    ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(caller_options,
                                                        callee_options));
    rtc::scoped_refptr<webrtc::FakeMetricsObserver> caller_observer =
        new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
    caller()->pc()->RegisterUMAObserver(caller_observer);
    ConnectFakeSignaling();
    caller()->AddAudioVideoMediaStream();
    callee()->AddAudioVideoMediaStream();
    caller()->CreateAndSetAndSignalOffer();
    ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
    EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(expected_cipher_suite),
                   caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
    EXPECT_EQ(
        1, caller_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                           expected_cipher_suite));
    caller()->pc()->RegisterUMAObserver(nullptr);
  }

 private:
  // |ss_| is used by |network_thread_| so it must be destroyed later.
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  std::unique_ptr<rtc::FirewallSocketServer> fss_;
  // |network_thread_| and |worker_thread_| are used by both
  // |caller_| and |callee_| so they must be destroyed
  // later.
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<PeerConnectionWrapper> caller_;
  std::unique_ptr<PeerConnectionWrapper> callee_;
};

// Test the OnFirstPacketReceived callback from audio/video RtpReceivers.  This
// includes testing that the callback is invoked if an observer is connected
// after the first packet has already been received.
TEST_F(PeerConnectionIntegrationTest,
       RtpReceiverObserverOnFirstPacketReceived) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  // Start offer/answer exchange and wait for it to complete.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Should be one receiver each for audio/video.
  EXPECT_EQ(2, caller()->rtp_receiver_observers().size());
  EXPECT_EQ(2, callee()->rtp_receiver_observers().size());
  // Wait for all "first packet received" callbacks to be fired.
  EXPECT_TRUE_WAIT(
      std::all_of(caller()->rtp_receiver_observers().begin(),
                  caller()->rtp_receiver_observers().end(),
                  [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                    return o->first_packet_received();
                  }),
      kMaxWaitForFramesMs);
  EXPECT_TRUE_WAIT(
      std::all_of(callee()->rtp_receiver_observers().begin(),
                  callee()->rtp_receiver_observers().end(),
                  [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                    return o->first_packet_received();
                  }),
      kMaxWaitForFramesMs);
  // If new observers are set after the first packet was already received, the
  // callback should still be invoked.
  caller()->ResetRtpReceiverObservers();
  callee()->ResetRtpReceiverObservers();
  EXPECT_EQ(2, caller()->rtp_receiver_observers().size());
  EXPECT_EQ(2, callee()->rtp_receiver_observers().size());
  EXPECT_TRUE(
      std::all_of(caller()->rtp_receiver_observers().begin(),
                  caller()->rtp_receiver_observers().end(),
                  [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                    return o->first_packet_received();
                  }));
  EXPECT_TRUE(
      std::all_of(callee()->rtp_receiver_observers().begin(),
                  callee()->rtp_receiver_observers().end(),
                  [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                    return o->first_packet_received();
                  }));
}

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

  const std::vector<std::string>& tones() const { return tones_; }
  bool completed() const { return completed_; }

 private:
  bool completed_;
  std::vector<std::string> tones_;
};

// Assumes |sender| already has an audio track added and the offer/answer
// exchange is done.
void TestDtmfFromSenderToReceiver(PeerConnectionWrapper* sender,
                                  PeerConnectionWrapper* receiver) {
  DummyDtmfObserver observer;
  rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender;

  // We should be able to create a DTMF sender from a local track.
  webrtc::AudioTrackInterface* localtrack =
      sender->local_streams()->at(0)->GetAudioTracks()[0];
  dtmf_sender = sender->pc()->CreateDtmfSender(localtrack);
  ASSERT_NE(nullptr, dtmf_sender.get());
  dtmf_sender->RegisterObserver(&observer);

  // Test the DtmfSender object just created.
  EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
  EXPECT_TRUE(dtmf_sender->InsertDtmf("1a", 100, 50));

  EXPECT_TRUE_WAIT(observer.completed(), kDefaultTimeout);
  std::vector<std::string> tones = {"1", "a", ""};
  EXPECT_EQ(tones, observer.tones());
  dtmf_sender->UnregisterObserver();
  // TODO(deadbeef): Verify the tones were actually received end-to-end.
}

// Verifies the DtmfSenderObserver callbacks for a DtmfSender (one in each
// direction).
TEST_F(PeerConnectionIntegrationTest, DtmfSenderObserver) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Only need audio for DTMF.
  caller()->AddAudioOnlyMediaStream();
  callee()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // DTLS must finish before the DTMF sender can be used reliably.
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
  TestDtmfFromSenderToReceiver(caller(), callee());
  TestDtmfFromSenderToReceiver(callee(), caller());
}

// Basic end-to-end test, verifying media can be encoded/transmitted/decoded
// between two connections, using DTLS-SRTP.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithDtls) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Uses SDES instead of DTLS for key agreement.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithSdes) {
  PeerConnectionInterface::RTCConfiguration sdes_config;
  sdes_config.enable_dtls_srtp.emplace(false);
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(sdes_config, sdes_config));
  ConnectFakeSignaling();

  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Tests that the GetRemoteAudioSSLCertificate method returns the remote DTLS
// certificate once the DTLS handshake has finished.
TEST_F(PeerConnectionIntegrationTest,
       GetRemoteAudioSSLCertificateReturnsExchangedCertificate) {
  auto GetRemoteAudioSSLCertificate = [](PeerConnectionWrapper* wrapper) {
    auto pci = reinterpret_cast<PeerConnectionProxy*>(wrapper->pc());
    auto pc = reinterpret_cast<PeerConnection*>(pci->internal());
    return pc->GetRemoteAudioSSLCertificate();
  };

  auto caller_cert = rtc::RTCCertificate::FromPEM(kRsaPems[0]);
  auto callee_cert = rtc::RTCCertificate::FromPEM(kRsaPems[1]);

  // Configure each side with a known certificate so they can be compared later.
  PeerConnectionInterface::RTCConfiguration caller_config;
  caller_config.enable_dtls_srtp.emplace(true);
  caller_config.certificates.push_back(caller_cert);
  PeerConnectionInterface::RTCConfiguration callee_config;
  callee_config.enable_dtls_srtp.emplace(true);
  callee_config.certificates.push_back(callee_cert);
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(caller_config, callee_config));
  ConnectFakeSignaling();

  // When first initialized, there should not be a remote SSL certificate (and
  // calling this method should not crash).
  EXPECT_EQ(nullptr, GetRemoteAudioSSLCertificate(caller()));
  EXPECT_EQ(nullptr, GetRemoteAudioSSLCertificate(callee()));

  caller()->AddAudioOnlyMediaStream();
  callee()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);

  // Once DTLS has been connected, each side should return the other's SSL
  // certificate when calling GetRemoteAudioSSLCertificate.

  auto caller_remote_cert = GetRemoteAudioSSLCertificate(caller());
  ASSERT_TRUE(caller_remote_cert);
  EXPECT_EQ(callee_cert->ssl_certificate().ToPEMString(),
            caller_remote_cert->ToPEMString());

  auto callee_remote_cert = GetRemoteAudioSSLCertificate(callee());
  ASSERT_TRUE(callee_remote_cert);
  EXPECT_EQ(caller_cert->ssl_certificate().ToPEMString(),
            callee_remote_cert->ToPEMString());
}

// This test sets up a call between two parties (using DTLS) and tests that we
// can get a video aspect ratio of 16:9.
TEST_F(PeerConnectionIntegrationTest, SendAndReceive16To9AspectRatio) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Add video tracks with 16:9 constraint.
  FakeConstraints constraints;
  double requested_ratio = 16.0 / 9;
  constraints.SetMandatoryMinAspectRatio(requested_ratio);
  caller()->AddMediaStreamFromTracks(
      nullptr, caller()->CreateLocalVideoTrackWithConstraints(constraints));
  callee()->AddMediaStreamFromTracks(
      nullptr, callee()->CreateLocalVideoTrackWithConstraints(constraints));

  // Do normal offer/answer and wait for at least one frame to be received in
  // each direction.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  // Check rendered aspect ratio.
  EXPECT_EQ(requested_ratio, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(requested_ratio, caller()->rendered_aspect_ratio());
  EXPECT_EQ(requested_ratio, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(requested_ratio, callee()->rendered_aspect_ratio());
}

// This test sets up a call between two parties with a source resolution of
// 1280x720 and verifies that a 16:9 aspect ratio is received.
TEST_F(PeerConnectionIntegrationTest,
       Send1280By720ResolutionAndReceive16To9AspectRatio) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Similar to above test, but uses MandatoryMin[Width/Height] constraint
  // instead of aspect ratio constraint.
  FakeConstraints constraints;
  constraints.SetMandatoryMinWidth(1280);
  constraints.SetMandatoryMinHeight(720);
  caller()->AddMediaStreamFromTracks(
      nullptr, caller()->CreateLocalVideoTrackWithConstraints(constraints));
  callee()->AddMediaStreamFromTracks(
      nullptr, callee()->CreateLocalVideoTrackWithConstraints(constraints));

  // Do normal offer/answer and wait for at least one frame to be received in
  // each direction.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  // Check rendered aspect ratio.
  EXPECT_EQ(16.0 / 9, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, caller()->rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, callee()->rendered_aspect_ratio());
}

// This test sets up an one-way call, with media only from caller to
// callee.
TEST_F(PeerConnectionIntegrationTest, OneWayMediaCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  int caller_received_frames = 0;
  ExpectNewFramesReceivedWithWait(
      caller_received_frames, caller_received_frames,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test sets up a audio call initially, with the callee rejecting video
// initially. Then later the callee decides to upgrade to audio/video, and
// initiates a new offer/answer exchange.
TEST_F(PeerConnectionIntegrationTest, AudioToVideoUpgrade) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Initially, offer an audio/video stream from the caller, but refuse to
  // send/receive video on the callee side.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioOnlyMediaStream();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_video = 0;
  callee()->SetOfferAnswerOptions(options);
  // Do offer/answer and make sure audio is still received end-to-end.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(kDefaultExpectedAudioFrameCount, 0,
                                  kDefaultExpectedAudioFrameCount, 0,
                                  kMaxWaitForFramesMs);
  // Sanity check that the callee's description has a rejected video section.
  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_video_content =
      GetFirstVideoContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
  // Now negotiate with video and ensure negotiation succeeds, with video
  // frames and additional audio frames being received.
  callee()->AddVideoOnlyMediaStream();
  options.offer_to_receive_video = 1;
  callee()->SetOfferAnswerOptions(options);
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Expect additional audio frames to be received after the upgrade.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Simpler than the above test; just add an audio track to an established
// video-only connection.
TEST_F(PeerConnectionIntegrationTest, AddAudioToVideoOnlyCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do initial offer/answer with just a video track.
  caller()->AddVideoOnlyMediaStream();
  callee()->AddVideoOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Now add an audio track and do another offer/answer.
  caller()->AddAudioOnlyMediaStream();
  callee()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Ensure both audio and video frames are received end-to-end.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test sets up a call that's transferred to a new caller with a different
// DTLS fingerprint.
TEST_F(PeerConnectionIntegrationTest, CallTransferredForCallee) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Keep the original peer around which will still send packets to the
  // receiving client. These SRTP packets will be dropped.
  std::unique_ptr<PeerConnectionWrapper> original_peer(
      SetCallerPcWrapperAndReturnCurrent(
          CreatePeerConnectionWrapperWithAlternateKey()));
  // TODO(deadbeef): Why do we call Close here? That goes against the comment
  // directly above.
  original_peer->pc()->Close();

  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for some additional frames to be transmitted end-to-end.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test sets up a call that's transferred to a new callee with a different
// DTLS fingerprint.
TEST_F(PeerConnectionIntegrationTest, CallTransferredForCaller) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Keep the original peer around which will still send packets to the
  // receiving client. These SRTP packets will be dropped.
  std::unique_ptr<PeerConnectionWrapper> original_peer(
      SetCalleePcWrapperAndReturnCurrent(
          CreatePeerConnectionWrapperWithAlternateKey()));
  // TODO(deadbeef): Why do we call Close here? That goes against the comment
  // directly above.
  original_peer->pc()->Close();

  ConnectFakeSignaling();
  callee()->AddAudioVideoMediaStream();
  caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for some additional frames to be transmitted end-to-end.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test sets up a non-bundled call and negotiates bundling at the same
// time as starting an ICE restart. When bundling is in effect in the restart,
// the DTLS-SRTP context should be successfully reset.
TEST_F(PeerConnectionIntegrationTest, BundlingEnabledWhileIceRestartOccurs) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  // Remove the bundle group from the SDP received by the callee.
  callee()->SetReceivedSdpMunger([](cricket::SessionDescription* desc) {
    desc->RemoveGroupByName("BUNDLE");
  });
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);

  // Now stop removing the BUNDLE group, and trigger an ICE restart.
  callee()->SetReceivedSdpMunger(nullptr);
  caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Expect additional frames to be received after the ICE restart.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test CVO (Coordination of Video Orientation). If a video source is rotated
// and both peers support the CVO RTP header extension, the actual video frames
// don't need to be encoded in different resolutions, since the rotation is
// communicated through the RTP header extension.
TEST_F(PeerConnectionIntegrationTest, RotatedVideoWithCVOExtension) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Add rotated video tracks.
  caller()->AddMediaStreamFromTracks(
      nullptr,
      caller()->CreateLocalVideoTrackWithRotation(webrtc::kVideoRotation_90));
  callee()->AddMediaStreamFromTracks(
      nullptr,
      callee()->CreateLocalVideoTrackWithRotation(webrtc::kVideoRotation_270));

  // Wait for video frames to be received by both sides.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  // Ensure that the aspect ratio is unmodified.
  // TODO(deadbeef): Where does 4:3 come from? Should be explicit in the test,
  // not just assumed.
  EXPECT_EQ(4.0 / 3, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, caller()->rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, callee()->rendered_aspect_ratio());
  // Ensure that the CVO bits were surfaced to the renderer.
  EXPECT_EQ(webrtc::kVideoRotation_270, caller()->rendered_rotation());
  EXPECT_EQ(webrtc::kVideoRotation_90, callee()->rendered_rotation());
}

// Test that when the CVO extension isn't supported, video is rotated the
// old-fashioned way, by encoding rotated frames.
TEST_F(PeerConnectionIntegrationTest, RotatedVideoWithoutCVOExtension) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Add rotated video tracks.
  caller()->AddMediaStreamFromTracks(
      nullptr,
      caller()->CreateLocalVideoTrackWithRotation(webrtc::kVideoRotation_90));
  callee()->AddMediaStreamFromTracks(
      nullptr,
      callee()->CreateLocalVideoTrackWithRotation(webrtc::kVideoRotation_270));

  // Remove the CVO extension from the offered SDP.
  callee()->SetReceivedSdpMunger([](cricket::SessionDescription* desc) {
    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(desc);
    video->ClearRtpHeaderExtensions();
  });
  // Wait for video frames to be received by both sides.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  // Expect that the aspect ratio is inversed to account for the 90/270 degree
  // rotation.
  // TODO(deadbeef): Where does 4:3 come from? Should be explicit in the test,
  // not just assumed.
  EXPECT_EQ(3.0 / 4, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, caller()->rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, callee()->rendered_aspect_ratio());
  // Expect that each endpoint is unaware of the rotation of the other endpoint.
  EXPECT_EQ(webrtc::kVideoRotation_0, caller()->rendered_rotation());
  EXPECT_EQ(webrtc::kVideoRotation_0, callee()->rendered_rotation());
}

// TODO(deadbeef): The tests below rely on RTCOfferAnswerOptions to reject an
// m= section. When we implement Unified Plan SDP, the right way to do this
// would be by stopping an RtpTransceiver.

// Test that if the answerer rejects the audio m= section, no audio is sent or
// received, but video still can be.
TEST_F(PeerConnectionIntegrationTest, AnswererRejectsAudioSection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  // Only add video track for callee, and set offer_to_receive_audio to 0, so
  // it will reject the audio m= section completely.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  callee()->SetOfferAnswerOptions(options);
  callee()->AddMediaStreamFromTracks(nullptr,
                                     callee()->CreateLocalVideoTrack());
  // Do offer/answer and wait for successful end-to-end video frames.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(0, kDefaultExpectedVideoFrameCount, 0,
                                  kDefaultExpectedVideoFrameCount,
                                  kMaxWaitForFramesMs);
  // Shouldn't have received audio frames at any point.
  EXPECT_EQ(0, caller()->audio_frames_received());
  EXPECT_EQ(0, callee()->audio_frames_received());
  // Sanity check that the callee's description has a rejected audio section.
  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_audio_content =
      GetFirstAudioContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_audio_content);
  EXPECT_TRUE(callee_audio_content->rejected);
}

// Test that if the answerer rejects the video m= section, no video is sent or
// received, but audio still can be.
TEST_F(PeerConnectionIntegrationTest, AnswererRejectsVideoSection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  // Only add audio track for callee, and set offer_to_receive_video to 0, so
  // it will reject the video m= section completely.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_video = 0;
  callee()->SetOfferAnswerOptions(options);
  callee()->AddMediaStreamFromTracks(callee()->CreateLocalAudioTrack(),
                                     nullptr);
  // Do offer/answer and wait for successful end-to-end audio frames.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(kDefaultExpectedAudioFrameCount, 0,
                                  kDefaultExpectedAudioFrameCount, 0,
                                  kMaxWaitForFramesMs);
  // Shouldn't have received video frames at any point.
  EXPECT_EQ(0, caller()->total_video_frames_received());
  EXPECT_EQ(0, callee()->total_video_frames_received());
  // Sanity check that the callee's description has a rejected video section.
  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_video_content =
      GetFirstVideoContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
}

// Test that if the answerer rejects both audio and video m= sections, nothing
// bad happens.
// TODO(deadbeef): Test that a data channel still works. Currently this doesn't
// test anything but the fact that negotiation succeeds, which doesn't mean
// much.
TEST_F(PeerConnectionIntegrationTest, AnswererRejectsAudioAndVideoSections) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  // Don't give the callee any tracks, and set offer_to_receive_X to 0, so it
  // will reject both audio and video m= sections.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  options.offer_to_receive_video = 0;
  callee()->SetOfferAnswerOptions(options);
  // Do offer/answer and wait for stable signaling state.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Sanity check that the callee's description has rejected m= sections.
  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_audio_content =
      GetFirstAudioContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_audio_content);
  EXPECT_TRUE(callee_audio_content->rejected);
  const ContentInfo* callee_video_content =
      GetFirstVideoContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
}

// This test sets up an audio and video call between two parties. After the
// call runs for a while, the caller sends an updated offer with video being
// rejected. Once the re-negotiation is done, the video flow should stop and
// the audio flow should continue.
TEST_F(PeerConnectionIntegrationTest, VideoRejectedInSubsequentOffer) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);

  // Renegotiate, rejecting the video m= section.
  // TODO(deadbeef): When an RtpTransceiver API is available, use that to
  // reject the video m= section.
  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* description) {
    for (cricket::ContentInfo& content : description->contents()) {
      if (cricket::IsVideoContent(&content)) {
        content.rejected = true;
      }
    }
  });
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);

  // Sanity check that the caller's description has a rejected video section.
  ASSERT_NE(nullptr, caller()->pc()->local_description());
  const ContentInfo* caller_video_content =
      GetFirstVideoContent(caller()->pc()->local_description()->description());
  ASSERT_NE(nullptr, caller_video_content);
  EXPECT_TRUE(caller_video_content->rejected);

  int caller_video_received = caller()->total_video_frames_received();
  int callee_video_received = callee()->total_video_frames_received();

  // Wait for some additional audio frames to be received.
  ExpectNewFramesReceivedWithWait(kDefaultExpectedAudioFrameCount, 0,
                                  kDefaultExpectedAudioFrameCount, 0,
                                  kMaxWaitForFramesMs);

  // During this time, we shouldn't have received any additional video frames
  // for the rejected video tracks.
  EXPECT_EQ(caller_video_received, caller()->total_video_frames_received());
  EXPECT_EQ(callee_video_received, callee()->total_video_frames_received());
}

// Basic end-to-end test, but without SSRC/MSID signaling. This functionality
// is needed to support legacy endpoints.
// TODO(deadbeef): When we support the MID extension and demuxing on MID, also
// add a test for an end-to-end test without MID signaling either (basically,
// the minimum acceptable SDP).
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithoutSsrcOrMsidSignaling) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Add audio and video, testing that packets can be demuxed on payload type.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  // Remove SSRCs and MSIDs from the received offer SDP.
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test that if two video tracks are sent (from caller to callee, in this test),
// they're transmitted correctly end-to-end.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithTwoVideoTracks) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Add one audio/video stream, and one video-only stream.
  caller()->AddAudioVideoMediaStream();
  caller()->AddVideoOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(2u, callee()->number_of_remote_streams());
  int expected_callee_received_frames = kDefaultExpectedVideoFrameCount;
  ExpectNewFramesReceivedWithWait(0, 0, 0, expected_callee_received_frames,
                                  kMaxWaitForFramesMs);
}

static void MakeSpecCompliantMaxBundleOffer(cricket::SessionDescription* desc) {
  bool first = true;
  for (cricket::ContentInfo& content : desc->contents()) {
    if (first) {
      first = false;
      continue;
    }
    content.bundle_only = true;
  }
  first = true;
  for (cricket::TransportInfo& transport : desc->transport_infos()) {
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

// Test that if applying a true "max bundle" offer, which uses ports of 0,
// "a=bundle-only", omitting "a=fingerprint", "a=setup", "a=ice-ufrag" and
// "a=ice-pwd" for all but the audio "m=" section, negotiation still completes
// successfully and media flows.
// TODO(deadbeef): Update this test to also omit "a=rtcp-mux", once that works.
// TODO(deadbeef): Won't need this test once we start generating actual
// standards-compliant SDP.
TEST_F(PeerConnectionIntegrationTest,
       EndToEndCallWithSpecCompliantMaxBundleOffer) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  // Do the equivalent of setting the port to 0, adding a=bundle-only, and
  // removing a=ice-ufrag, a=ice-pwd, a=fingerprint and a=setup from all
  // but the first m= section.
  callee()->SetReceivedSdpMunger(MakeSpecCompliantMaxBundleOffer);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test that we can receive the audio output level from a remote audio track.
// TODO(deadbeef): Use a fake audio source and verify that the output level is
// exactly what the source on the other side was configured with.
TEST_F(PeerConnectionIntegrationTest, GetAudioOutputLevelStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Just add an audio track.
  caller()->AddMediaStreamFromTracks(caller()->CreateLocalAudioTrack(),
                                     nullptr);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Get the audio output level stats. Note that the level is not available
  // until an RTCP packet has been received.
  EXPECT_TRUE_WAIT(callee()->OldGetStats()->AudioOutputLevel() > 0,
                   kMaxWaitForFramesMs);
}

// Test that an audio input level is reported.
// TODO(deadbeef): Use a fake audio source and verify that the input level is
// exactly what the source was configured with.
TEST_F(PeerConnectionIntegrationTest, GetAudioInputLevelStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Just add an audio track.
  caller()->AddMediaStreamFromTracks(caller()->CreateLocalAudioTrack(),
                                     nullptr);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Get the audio input level stats. The level should be available very
  // soon after the test starts.
  EXPECT_TRUE_WAIT(caller()->OldGetStats()->AudioInputLevel() > 0,
                   kMaxWaitForStatsMs);
}

// Test that we can get incoming byte counts from both audio and video tracks.
TEST_F(PeerConnectionIntegrationTest, GetBytesReceivedStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  // Do offer/answer, wait for the callee to receive some frames.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  int expected_caller_received_frames = 0;
  ExpectNewFramesReceivedWithWait(
      expected_caller_received_frames, expected_caller_received_frames,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);

  // Get a handle to the remote tracks created, so they can be used as GetStats
  // filters.
  StreamCollectionInterface* remote_streams = callee()->remote_streams();
  ASSERT_EQ(1u, remote_streams->count());
  ASSERT_EQ(1u, remote_streams->at(0)->GetAudioTracks().size());
  ASSERT_EQ(1u, remote_streams->at(0)->GetVideoTracks().size());
  MediaStreamTrackInterface* remote_audio_track =
      remote_streams->at(0)->GetAudioTracks()[0];
  MediaStreamTrackInterface* remote_video_track =
      remote_streams->at(0)->GetVideoTracks()[0];

  // We received frames, so we definitely should have nonzero "received bytes"
  // stats at this point.
  EXPECT_GT(callee()->OldGetStatsForTrack(remote_audio_track)->BytesReceived(),
            0);
  EXPECT_GT(callee()->OldGetStatsForTrack(remote_video_track)->BytesReceived(),
            0);
}

// Test that we can get outgoing byte counts from both audio and video tracks.
TEST_F(PeerConnectionIntegrationTest, GetBytesSentStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto audio_track = caller()->CreateLocalAudioTrack();
  auto video_track = caller()->CreateLocalVideoTrack();
  caller()->AddMediaStreamFromTracks(audio_track, video_track);
  // Do offer/answer, wait for the callee to receive some frames.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  int expected_caller_received_frames = 0;
  ExpectNewFramesReceivedWithWait(
      expected_caller_received_frames, expected_caller_received_frames,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);

  // The callee received frames, so we definitely should have nonzero "sent
  // bytes" stats at this point.
  EXPECT_GT(caller()->OldGetStatsForTrack(audio_track)->BytesSent(), 0);
  EXPECT_GT(caller()->OldGetStatsForTrack(video_track)->BytesSent(), 0);
}

// Test that we can get capture start ntp time.
TEST_F(PeerConnectionIntegrationTest, GetCaptureStartNtpTimeWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioOnlyMediaStream();

  auto audio_track = callee()->CreateLocalAudioTrack();
  callee()->AddMediaStreamFromTracks(audio_track, nullptr);

  // Do offer/answer, wait for the callee to receive some frames.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Get the remote audio track created on the receiver, so they can be used as
  // GetStats filters.
  StreamCollectionInterface* remote_streams = callee()->remote_streams();
  ASSERT_EQ(1u, remote_streams->count());
  ASSERT_EQ(1u, remote_streams->at(0)->GetAudioTracks().size());
  MediaStreamTrackInterface* remote_audio_track =
      remote_streams->at(0)->GetAudioTracks()[0];

  // Get the audio output level stats. Note that the level is not available
  // until an RTCP packet has been received.
  EXPECT_TRUE_WAIT(callee()->OldGetStatsForTrack(remote_audio_track)->
                   CaptureStartNtpTime() > 0, 2 * kMaxWaitForFramesMs);
}

// Test that we can get stats (using the new stats implemnetation) for
// unsignaled streams. Meaning when SSRCs/MSIDs aren't signaled explicitly in
// SDP.
TEST_F(PeerConnectionIntegrationTest,
       GetStatsForUnsignaledStreamWithNewStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioOnlyMediaStream();
  // Remove SSRCs and MSIDs from the received offer SDP.
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for one audio frame to be received by the callee.
  ExpectNewFramesReceivedWithWait(0, 0, 1, 0, kMaxWaitForFramesMs);

  // We received a frame, so we should have nonzero "bytes received" stats for
  // the unsignaled stream, if stats are working for it.
  rtc::scoped_refptr<const webrtc::RTCStatsReport> report =
      callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  auto inbound_stream_stats =
      report->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>();
  ASSERT_EQ(1U, inbound_stream_stats.size());
  ASSERT_TRUE(inbound_stream_stats[0]->bytes_received.is_defined());
  ASSERT_GT(*inbound_stream_stats[0]->bytes_received, 0U);
  ASSERT_TRUE(inbound_stream_stats[0]->track_id.is_defined());
}

// Test that we can successfully get the media related stats (audio level
// etc.) for the unsignaled stream.
TEST_F(PeerConnectionIntegrationTest,
       GetMediaStatsForUnsignaledStreamWithNewStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  // Remove SSRCs and MSIDs from the received offer SDP.
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for one audio frame to be received by the callee.
  ExpectNewFramesReceivedWithWait(0, 0, 1, 1, kMaxWaitForFramesMs);

  rtc::scoped_refptr<const webrtc::RTCStatsReport> report =
      callee()->NewGetStats();
  ASSERT_NE(nullptr, report);

  auto media_stats = report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  auto audio_index = FindFirstMediaStatsIndexByKind("audio", media_stats);
  ASSERT_GE(audio_index, 0);
  EXPECT_TRUE(media_stats[audio_index]->audio_level.is_defined());
}

// Helper for test below.
void ModifySsrcs(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    for (cricket::StreamParams& stream :
         content.media_description()->mutable_streams()) {
      for (uint32_t& ssrc : stream.ssrcs) {
        ssrc = rtc::CreateRandomId();
      }
    }
  }
}

// Test that the "RTCMediaSteamTrackStats"  object is updated correctly when
// SSRCs are unsignaled, and the SSRC of the received (audio) stream changes.
// This should result in two "RTCInboundRTPStreamStats", but only one
// "RTCMediaStreamTrackStats", whose counters go up continuously rather than
// being reset to 0 once the SSRC change occurs.
//
// Regression test for this bug:
// https://bugs.chromium.org/p/webrtc/issues/detail?id=8158
//
// The bug causes the track stats to only represent one of the two streams:
// whichever one has the higher SSRC. So with this bug, there was a 50% chance
// that the track stat counters would reset to 0 when the new stream is
// received, and a 50% chance that they'll stop updating (while
// "concealed_samples" continues increasing, due to silence being generated for
// the inactive stream).
TEST_F(PeerConnectionIntegrationTest,
       TrackStatsUpdatedCorrectlyWhenUnsignaledSsrcChanges) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioOnlyMediaStream();
  // Remove SSRCs and MSIDs from the received offer SDP, simulating an endpoint
  // that doesn't signal SSRCs (from the callee's perspective).
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for 50 audio frames (500ms of audio) to be received by the callee.
  ExpectNewFramesReceivedWithWait(0, 0, 25, 0, kMaxWaitForFramesMs);

  // Some audio frames were received, so we should have nonzero "samples
  // received" for the track.
  rtc::scoped_refptr<const webrtc::RTCStatsReport> report =
      callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  auto track_stats = report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, track_stats.size());
  ASSERT_TRUE(track_stats[0]->total_samples_received.is_defined());
  ASSERT_GT(*track_stats[0]->total_samples_received, 0U);
  // uint64_t prev_samples_received = *track_stats[0]->total_samples_received;

  // Create a new offer and munge it to cause the caller to use a new SSRC.
  caller()->SetGeneratedSdpMunger(ModifySsrcs);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for 25 more audio frames (250ms of audio) to be received, from the new
  // SSRC.
  ExpectNewFramesReceivedWithWait(0, 0, 25, 0, kMaxWaitForFramesMs);

  report = callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  track_stats = report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  ASSERT_EQ(1U, track_stats.size());
  ASSERT_TRUE(track_stats[0]->total_samples_received.is_defined());
  // The "total samples received" stat should only be greater than it was
  // before.
  // TODO(deadbeef): Uncomment this assertion once the bug is completely fixed.
  // Right now, the new SSRC will cause the counters to reset to 0.
  // EXPECT_GT(*track_stats[0]->total_samples_received, prev_samples_received);

  // Additionally, the percentage of concealed samples (samples generated to
  // conceal packet loss) should be less than 50%. If it's greater, that's a
  // good sign that we're seeing stats from the old stream that's no longer
  // receiving packets, and is generating concealed samples of silence.
  constexpr double kAcceptableConcealedSamplesPercentage = 0.50;
  ASSERT_TRUE(track_stats[0]->concealed_samples.is_defined());
  EXPECT_LT(*track_stats[0]->concealed_samples,
            *track_stats[0]->total_samples_received *
                kAcceptableConcealedSamplesPercentage);

  // Also ensure that we have two "RTCInboundRTPStreamStats" as expected, as a
  // sanity check that the SSRC really changed.
  // TODO(deadbeef): This isn't working right now, because we're not returning
  // *any* stats for the inactive stream. Uncomment when the bug is completely
  // fixed.
  // auto inbound_stream_stats =
  //     report->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>();
  // ASSERT_EQ(2U, inbound_stream_stats.size());
}

// Test that DTLS 1.0 is used if both sides only support DTLS 1.0.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithDtls10) {
  PeerConnectionFactory::Options dtls_10_options;
  dtls_10_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(dtls_10_options,
                                                      dtls_10_options));
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test getting cipher stats and UMA metrics when DTLS 1.0 is negotiated.
TEST_F(PeerConnectionIntegrationTest, Dtls10CipherStatsAndUmaMetrics) {
  PeerConnectionFactory::Options dtls_10_options;
  dtls_10_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(dtls_10_options,
                                                      dtls_10_options));
  ConnectFakeSignaling();
  // Register UMA observer before signaling begins.
  rtc::scoped_refptr<webrtc::FakeMetricsObserver> caller_observer =
      new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  caller()->pc()->RegisterUMAObserver(caller_observer);
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(rtc::SSLStreamAdapter::IsAcceptableCipher(
                       caller()->OldGetStats()->DtlsCipher(), rtc::KT_DEFAULT),
                   kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
  EXPECT_EQ(1,
            caller_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                            kDefaultSrtpCryptoSuite));
}

// Test getting cipher stats and UMA metrics when DTLS 1.2 is negotiated.
TEST_F(PeerConnectionIntegrationTest, Dtls12CipherStatsAndUmaMetrics) {
  PeerConnectionFactory::Options dtls_12_options;
  dtls_12_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(dtls_12_options,
                                                      dtls_12_options));
  ConnectFakeSignaling();
  // Register UMA observer before signaling begins.
  rtc::scoped_refptr<webrtc::FakeMetricsObserver> caller_observer =
      new rtc::RefCountedObject<webrtc::FakeMetricsObserver>();
  caller()->pc()->RegisterUMAObserver(caller_observer);
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(rtc::SSLStreamAdapter::IsAcceptableCipher(
                       caller()->OldGetStats()->DtlsCipher(), rtc::KT_DEFAULT),
                   kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
  EXPECT_EQ(1,
            caller_observer->GetEnumCounter(webrtc::kEnumCounterAudioSrtpCipher,
                                            kDefaultSrtpCryptoSuite));
}

// Test that DTLS 1.0 can be used if the caller supports DTLS 1.2 and the
// callee only supports 1.0.
TEST_F(PeerConnectionIntegrationTest, CallerDtls12ToCalleeDtls10) {
  PeerConnectionFactory::Options caller_options;
  caller_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  PeerConnectionFactory::Options callee_options;
  callee_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithOptions(caller_options, callee_options));
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test that DTLS 1.0 can be used if the caller only supports DTLS 1.0 and the
// callee supports 1.2.
TEST_F(PeerConnectionIntegrationTest, CallerDtls10ToCalleeDtls12) {
  PeerConnectionFactory::Options caller_options;
  caller_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  PeerConnectionFactory::Options callee_options;
  callee_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithOptions(caller_options, callee_options));
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test that a non-GCM cipher is used if both sides only support non-GCM.
TEST_F(PeerConnectionIntegrationTest, NonGcmCipherUsedWhenGcmNotSupported) {
  bool local_gcm_enabled = false;
  bool remote_gcm_enabled = false;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  TestGcmNegotiationUsesCipherSuite(local_gcm_enabled, remote_gcm_enabled,
                                    expected_cipher_suite);
}

// Test that a GCM cipher is used if both ends support it.
TEST_F(PeerConnectionIntegrationTest, GcmCipherUsedWhenGcmSupported) {
  bool local_gcm_enabled = true;
  bool remote_gcm_enabled = true;
  int expected_cipher_suite = kDefaultSrtpCryptoSuiteGcm;
  TestGcmNegotiationUsesCipherSuite(local_gcm_enabled, remote_gcm_enabled,
                                    expected_cipher_suite);
}

// Test that GCM isn't used if only the offerer supports it.
TEST_F(PeerConnectionIntegrationTest,
       NonGcmCipherUsedWhenOnlyCallerSupportsGcm) {
  bool local_gcm_enabled = true;
  bool remote_gcm_enabled = false;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  TestGcmNegotiationUsesCipherSuite(local_gcm_enabled, remote_gcm_enabled,
                                    expected_cipher_suite);
}

// Test that GCM isn't used if only the answerer supports it.
TEST_F(PeerConnectionIntegrationTest,
       NonGcmCipherUsedWhenOnlyCalleeSupportsGcm) {
  bool local_gcm_enabled = false;
  bool remote_gcm_enabled = true;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  TestGcmNegotiationUsesCipherSuite(local_gcm_enabled, remote_gcm_enabled,
                                    expected_cipher_suite);
}

// Verify that media can be transmitted end-to-end when GCM crypto suites are
// enabled. Note that the above tests, such as GcmCipherUsedWhenGcmSupported,
// only verify that a GCM cipher is negotiated, and not necessarily that SRTP
// works with it.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithGcmCipher) {
  PeerConnectionFactory::Options gcm_options;
  gcm_options.crypto_options.enable_gcm_crypto_suites = true;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithOptions(gcm_options, gcm_options));
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test sets up a call between two parties with audio, video and an RTP
// data channel.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithRtpDataChannel) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConstraints(&setup_constraints,
                                                          &setup_constraints));
  ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  caller()->CreateDataChannel();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Ensure the existence of the RTP data channel didn't impede audio/video.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_NE(nullptr, callee()->data_channel());
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  SendRtpDataWithRetries(caller()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  SendRtpDataWithRetries(callee()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Ensure that an RTP data channel is signaled as closed for the caller when
// the callee rejects it in a subsequent offer.
TEST_F(PeerConnectionIntegrationTest,
       RtpDataChannelSignaledClosedInCalleeOffer) {
  // Same procedure as above test.
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConstraints(&setup_constraints,
                                                          &setup_constraints));
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_NE(nullptr, callee()->data_channel());
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Close the data channel on the callee, and do an updated offer/answer.
  callee()->data_channel()->Close();
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_FALSE(caller()->data_observer()->IsOpen());
  EXPECT_FALSE(callee()->data_observer()->IsOpen());
}

// Tests that data is buffered in an RTP data channel until an observer is
// registered for it.
//
// NOTE: RTP data channels can receive data before the underlying
// transport has detected that a channel is writable and thus data can be
// received before the data channel state changes to open. That is hard to test
// but the same buffering is expected to be used in that case.
TEST_F(PeerConnectionIntegrationTest,
       DataBufferedUntilRtpDataChannelObserverRegistered) {
  // Use fake clock and simulated network delay so that we predictably can wait
  // until an SCTP message has been delivered without "sleep()"ing.
  rtc::ScopedFakeClock fake_clock;
  // Some things use a time of "0" as a special value, so we need to start out
  // the fake clock at a nonzero time.
  // TODO(deadbeef): Fix this.
  fake_clock.AdvanceTime(rtc::TimeDelta::FromSeconds(1));
  virtual_socket_server()->set_delay_mean(5);  // 5 ms per hop.
  virtual_socket_server()->UpdateDelayDistribution();

  FakeConstraints constraints;
  constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConstraints(&constraints, &constraints));
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE(caller()->data_channel() != nullptr);
  ASSERT_TRUE_SIMULATED_WAIT(callee()->data_channel() != nullptr,
                             kDefaultTimeout, fake_clock);
  ASSERT_TRUE_SIMULATED_WAIT(caller()->data_observer()->IsOpen(),
                             kDefaultTimeout, fake_clock);
  ASSERT_EQ_SIMULATED_WAIT(DataChannelInterface::kOpen,
                           callee()->data_channel()->state(), kDefaultTimeout,
                           fake_clock);

  // Unregister the observer which is normally automatically registered.
  callee()->data_channel()->UnregisterObserver();
  // Send data and advance fake clock until it should have been received.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  SIMULATED_WAIT(false, 50, fake_clock);

  // Attach data channel and expect data to be received immediately. Note that
  // EXPECT_EQ_WAIT is used, such that the simulated clock is not advanced any
  // further, but data can be received even if the callback is asynchronous.
  MockDataChannelObserver new_observer(callee()->data_channel());
  EXPECT_EQ_SIMULATED_WAIT(data, new_observer.last_message(), kDefaultTimeout,
                           fake_clock);
}

// This test sets up a call between two parties with audio, video and but only
// the caller client supports RTP data channels.
TEST_F(PeerConnectionIntegrationTest, RtpDataChannelsRejectedByCallee) {
  FakeConstraints setup_constraints_1;
  setup_constraints_1.SetAllowRtpDataChannels();
  // Must disable DTLS to make negotiation succeed.
  setup_constraints_1.SetMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                   false);
  FakeConstraints setup_constraints_2;
  setup_constraints_2.SetMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                                   false);
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConstraints(
      &setup_constraints_1, &setup_constraints_2));
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // The caller should still have a data channel, but it should be closed, and
  // one should ever have been created for the callee.
  EXPECT_TRUE(caller()->data_channel() != nullptr);
  EXPECT_FALSE(caller()->data_observer()->IsOpen());
  EXPECT_EQ(nullptr, callee()->data_channel());
}

// This test sets up a call between two parties with audio, and video. When
// audio and video is setup and flowing, an RTP data channel is negotiated.
TEST_F(PeerConnectionIntegrationTest, AddRtpDataChannelInSubsequentOffer) {
  FakeConstraints setup_constraints;
  setup_constraints.SetAllowRtpDataChannels();
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConstraints(&setup_constraints,
                                                          &setup_constraints));
  ConnectFakeSignaling();
  // Do initial offer/answer with audio/video.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Create data channel and do new offer and answer.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_NE(nullptr, callee()->data_channel());
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  SendRtpDataWithRetries(caller()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  SendRtpDataWithRetries(callee()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#ifdef HAVE_SCTP

// This test sets up a call between two parties with audio, video and an SCTP
// data channel.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithSctpDataChannel) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  caller()->CreateDataChannel();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Ensure the existence of the SCTP data channel didn't impede audio/video.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Ensure that when the callee closes an SCTP data channel, the closing
// procedure results in the data channel being closed for the caller as well.
TEST_F(PeerConnectionIntegrationTest, CalleeClosesSctpDataChannel) {
  // Same procedure as above test.
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Close the data channel on the callee side, and wait for it to reach the
  // "closed" state on both sides.
  callee()->data_channel()->Close();
  EXPECT_TRUE_WAIT(!caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(!callee()->data_observer()->IsOpen(), kDefaultTimeout);
}

TEST_F(PeerConnectionIntegrationTest, SctpDataChannelConfigSentToOtherSide) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  webrtc::DataChannelInit init;
  init.id = 53;
  init.maxRetransmits = 52;
  caller()->CreateDataChannel("data-channel", &init);
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_EQ(init.id, callee()->data_channel()->id());
  EXPECT_EQ("data-channel", callee()->data_channel()->label());
  EXPECT_EQ(init.maxRetransmits, callee()->data_channel()->maxRetransmits());
  EXPECT_FALSE(callee()->data_channel()->negotiated());
}

// Test usrsctp's ability to process unordered data stream, where data actually
// arrives out of order using simulated delays. Previously there have been some
// bugs in this area.
TEST_F(PeerConnectionIntegrationTest, StressTestUnorderedSctpDataChannel) {
  // Introduce random network delays.
  // Otherwise it's not a true "unordered" test.
  virtual_socket_server()->set_delay_mean(20);
  virtual_socket_server()->set_delay_stddev(5);
  virtual_socket_server()->UpdateDelayDistribution();
  // Normal procedure, but with unordered data channel config.
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  webrtc::DataChannelInit init;
  init.ordered = false;
  caller()->CreateDataChannel(&init);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  static constexpr int kNumMessages = 100;
  // Deliberately chosen to be larger than the MTU so messages get fragmented.
  static constexpr size_t kMaxMessageSize = 4096;
  // Create and send random messages.
  std::vector<std::string> sent_messages;
  for (int i = 0; i < kNumMessages; ++i) {
    size_t length =
        (rand() % kMaxMessageSize) + 1;  // NOLINT (rand_r instead of rand)
    std::string message;
    ASSERT_TRUE(rtc::CreateRandomString(length, &message));
    caller()->data_channel()->Send(DataBuffer(message));
    callee()->data_channel()->Send(DataBuffer(message));
    sent_messages.push_back(message);
  }

  // Wait for all messages to be received.
  EXPECT_EQ_WAIT(kNumMessages,
                 caller()->data_observer()->received_message_count(),
                 kDefaultTimeout);
  EXPECT_EQ_WAIT(kNumMessages,
                 callee()->data_observer()->received_message_count(),
                 kDefaultTimeout);

  // Sort and compare to make sure none of the messages were corrupted.
  std::vector<std::string> caller_received_messages =
      caller()->data_observer()->messages();
  std::vector<std::string> callee_received_messages =
      callee()->data_observer()->messages();
  std::sort(sent_messages.begin(), sent_messages.end());
  std::sort(caller_received_messages.begin(), caller_received_messages.end());
  std::sort(callee_received_messages.begin(), callee_received_messages.end());
  EXPECT_EQ(sent_messages, caller_received_messages);
  EXPECT_EQ(sent_messages, callee_received_messages);
}

// This test sets up a call between two parties with audio, and video. When
// audio and video are setup and flowing, an SCTP data channel is negotiated.
TEST_F(PeerConnectionIntegrationTest, AddSctpDataChannelInSubsequentOffer) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do initial offer/answer with audio/video.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Create data channel and do new offer and answer.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Set up a connection initially just using SCTP data channels, later upgrading
// to audio/video, ensuring frames are received end-to-end. Effectively the
// inverse of the test above.
// This was broken in M57; see https://crbug.com/711243
TEST_F(PeerConnectionIntegrationTest, SctpDataChannelToAudioVideoUpgrade) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do initial offer/answer with just data channel.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait until data can be sent over the data channel.
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Do subsequent offer/answer with two-way audio and video. Audio and video
  // should end up bundled on the DTLS/ICE transport already used for data.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

static void MakeSpecCompliantSctpOffer(cricket::SessionDescription* desc) {
  cricket::DataContentDescription* dcd_offer =
      GetFirstDataContentDescription(desc);
  ASSERT_TRUE(dcd_offer);
  dcd_offer->set_use_sctpmap(false);
  dcd_offer->set_protocol("UDP/DTLS/SCTP");
}

// Test that the data channel works when a spec-compliant SCTP m= section is
// offered (using "a=sctp-port" instead of "a=sctpmap", and using
// "UDP/DTLS/SCTP" as the protocol).
TEST_F(PeerConnectionIntegrationTest,
       DataChannelWorksWhenSpecCompliantSctpOfferReceived) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->SetGeneratedSdpMunger(MakeSpecCompliantSctpOffer);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#endif  // HAVE_SCTP

// Test that the ICE connection and gathering states eventually reach
// "complete".
TEST_F(PeerConnectionIntegrationTest, IceStatesReachCompletion) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do normal offer/answer.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceGatheringComplete,
                 caller()->ice_gathering_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceGatheringComplete,
                 callee()->ice_gathering_state(), kMaxWaitForFramesMs);
  // After the best candidate pair is selected and all candidates are signaled,
  // the ICE connection state should reach "complete".
  // TODO(deadbeef): Currently, the ICE "controlled" agent (the
  // answerer/"callee" by default) only reaches "connected". When this is
  // fixed, this test should be updated.
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kDefaultTimeout);
}

// Test that firewalling the ICE connection causes the clients to identify the
// disconnected state and then removing the firewall causes them to reconnect.
class PeerConnectionIntegrationIceStatesTest
    : public PeerConnectionIntegrationTest,
      public ::testing::WithParamInterface<std::tuple<std::string, uint32_t>> {
 protected:
  PeerConnectionIntegrationIceStatesTest() {
    port_allocator_flags_ = std::get<1>(GetParam());
  }

  void StartStunServer(const SocketAddress& server_address) {
    stun_server_.reset(
        cricket::TestStunServer::Create(network_thread(), server_address));
  }

  bool TestIPv6() {
    return (port_allocator_flags_ & cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  void SetPortAllocatorFlags() {
    caller()->port_allocator()->set_flags(port_allocator_flags_);
    callee()->port_allocator()->set_flags(port_allocator_flags_);
  }

  std::vector<SocketAddress> CallerAddresses() {
    std::vector<SocketAddress> addresses;
    addresses.push_back(SocketAddress("1.1.1.1", 0));
    if (TestIPv6()) {
      addresses.push_back(SocketAddress("1111:0:a:b:c:d:e:f", 0));
    }
    return addresses;
  }

  std::vector<SocketAddress> CalleeAddresses() {
    std::vector<SocketAddress> addresses;
    addresses.push_back(SocketAddress("2.2.2.2", 0));
    if (TestIPv6()) {
      addresses.push_back(SocketAddress("2222:0:a:b:c:d:e:f", 0));
    }
    return addresses;
  }

  void SetUpNetworkInterfaces() {
    // Remove the default interfaces added by the test infrastructure.
    caller()->network()->RemoveInterface(kDefaultLocalAddress);
    callee()->network()->RemoveInterface(kDefaultLocalAddress);

    // Add network addresses for test.
    for (const auto& caller_address : CallerAddresses()) {
      caller()->network()->AddInterface(caller_address);
    }
    for (const auto& callee_address : CalleeAddresses()) {
      callee()->network()->AddInterface(callee_address);
    }
  }

 private:
  uint32_t port_allocator_flags_;
  std::unique_ptr<cricket::TestStunServer> stun_server_;
};

// Tests that the PeerConnection goes through all the ICE gathering/connection
// states over the duration of the call. This includes Disconnected and Failed
// states, induced by putting a firewall between the peers and waiting for them
// to time out.
TEST_P(PeerConnectionIntegrationIceStatesTest, VerifyIceStates) {
  // TODO(bugs.webrtc.org/8295): When using a ScopedFakeClock, this test will
  // sometimes hit a DCHECK in platform_thread.cc about the PacerThread being
  // too busy. For now, revert to running without a fake clock.

  const SocketAddress kStunServerAddress =
      SocketAddress("99.99.99.1", cricket::STUN_SERVER_PORT);
  StartStunServer(kStunServerAddress);

  PeerConnectionInterface::RTCConfiguration config;
  PeerConnectionInterface::IceServer ice_stun_server;
  ice_stun_server.urls.push_back(
      "stun:" + kStunServerAddress.HostAsURIString() + ":" +
      kStunServerAddress.PortAsString());
  config.servers.push_back(ice_stun_server);

  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  SetPortAllocatorFlags();
  SetUpNetworkInterfaces();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();

  // Initial state before anything happens.
  ASSERT_EQ(PeerConnectionInterface::kIceGatheringNew,
            caller()->ice_gathering_state());
  ASSERT_EQ(PeerConnectionInterface::kIceConnectionNew,
            caller()->ice_connection_state());

  // Start the call by creating the offer, setting it as the local description,
  // then sending it to the peer who will respond with an answer. This happens
  // asynchronously so that we can watch the states as it runs in the
  // background.
  caller()->CreateAndSetAndSignalOffer();

  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);

  // Verify that the observer was notified of the intermediate transitions.
  EXPECT_THAT(caller()->ice_connection_state_history(),
              ElementsAre(PeerConnectionInterface::kIceConnectionChecking,
                          PeerConnectionInterface::kIceConnectionConnected,
                          PeerConnectionInterface::kIceConnectionCompleted));
  EXPECT_THAT(caller()->ice_gathering_state_history(),
              ElementsAre(PeerConnectionInterface::kIceGatheringGathering,
                          PeerConnectionInterface::kIceGatheringComplete));

  // Block connections to/from the caller and wait for ICE to become
  // disconnected.
  for (const auto& caller_address : CallerAddresses()) {
    firewall()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY, caller_address);
  }
  RTC_LOG(LS_INFO) << "Firewall rules applied";
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionDisconnected,
                 caller()->ice_connection_state(), kDefaultTimeout);

  // Let ICE re-establish by removing the firewall rules.
  firewall()->ClearRules();
  RTC_LOG(LS_INFO) << "Firewall rules cleared";
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);

  // According to RFC7675, if there is no response within 30 seconds then the
  // peer should consider the other side to have rejected the connection. This
  // is signaled by the state transitioning to "failed".
  constexpr int kConsentTimeout = 30000;
  for (const auto& caller_address : CallerAddresses()) {
    firewall()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY, caller_address);
  }
  RTC_LOG(LS_INFO) << "Firewall rules applied again";
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 caller()->ice_connection_state(), kConsentTimeout);
}

// Tests that the best connection is set to the appropriate IPv4/IPv6 connection
// and that the statistics in the metric observers are updated correctly.
TEST_P(PeerConnectionIntegrationIceStatesTest, VerifyBestConnection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  SetPortAllocatorFlags();
  SetUpNetworkInterfaces();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();

  rtc::scoped_refptr<webrtc::FakeMetricsObserver> metrics_observer(
      new rtc::RefCountedObject<webrtc::FakeMetricsObserver>());
  caller()->pc()->RegisterUMAObserver(metrics_observer.get());

  caller()->CreateAndSetAndSignalOffer();

  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  const int num_best_ipv4 = metrics_observer->GetEnumCounter(
      webrtc::kEnumCounterAddressFamily, webrtc::kBestConnections_IPv4);
  const int num_best_ipv6 = metrics_observer->GetEnumCounter(
      webrtc::kEnumCounterAddressFamily, webrtc::kBestConnections_IPv6);
  if (TestIPv6()) {
    // When IPv6 is enabled, we should prefer an IPv6 connection over an IPv4
    // connection.
    EXPECT_EQ(0u, num_best_ipv4);
    EXPECT_EQ(1u, num_best_ipv6);
  } else {
    EXPECT_EQ(1u, num_best_ipv4);
    EXPECT_EQ(0u, num_best_ipv6);
  }

  EXPECT_EQ(0u, metrics_observer->GetEnumCounter(
                    webrtc::kEnumCounterIceCandidatePairTypeUdp,
                    webrtc::kIceCandidatePairHostHost));
  EXPECT_EQ(1u, metrics_observer->GetEnumCounter(
                    webrtc::kEnumCounterIceCandidatePairTypeUdp,
                    webrtc::kIceCandidatePairHostPublicHostPublic));
}

constexpr uint32_t kFlagsIPv4NoStun = cricket::PORTALLOCATOR_DISABLE_TCP |
                                      cricket::PORTALLOCATOR_DISABLE_STUN |
                                      cricket::PORTALLOCATOR_DISABLE_RELAY;
constexpr uint32_t kFlagsIPv6NoStun =
    cricket::PORTALLOCATOR_DISABLE_TCP | cricket::PORTALLOCATOR_DISABLE_STUN |
    cricket::PORTALLOCATOR_ENABLE_IPV6 | cricket::PORTALLOCATOR_DISABLE_RELAY;
constexpr uint32_t kFlagsIPv4Stun =
    cricket::PORTALLOCATOR_DISABLE_TCP | cricket::PORTALLOCATOR_DISABLE_RELAY;

INSTANTIATE_TEST_CASE_P(PeerConnectionIntegrationTest,
                        PeerConnectionIntegrationIceStatesTest,
                        Values(std::make_pair("IPv4 no STUN", kFlagsIPv4NoStun),
                               std::make_pair("IPv6 no STUN", kFlagsIPv6NoStun),
                               std::make_pair("IPv4 with STUN",
                                              kFlagsIPv4Stun)));

// This test sets up a call between two parties with audio and video.
// During the call, the caller restarts ICE and the test verifies that
// new ICE candidates are generated and audio and video still can flow, and the
// ICE state reaches completed again.
TEST_F(PeerConnectionIntegrationTest, MediaContinuesFlowingAfterIceRestart) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for ICE to complete.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  // To verify that the ICE restart actually occurs, get
  // ufrag/password/candidates before and after restart.
  // Create an SDP string of the first audio candidate for both clients.
  const webrtc::IceCandidateCollection* audio_candidates_caller =
      caller()->pc()->local_description()->candidates(0);
  const webrtc::IceCandidateCollection* audio_candidates_callee =
      callee()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_caller->count(), 0u);
  ASSERT_GT(audio_candidates_callee->count(), 0u);
  std::string caller_candidate_pre_restart;
  ASSERT_TRUE(
      audio_candidates_caller->at(0)->ToString(&caller_candidate_pre_restart));
  std::string callee_candidate_pre_restart;
  ASSERT_TRUE(
      audio_candidates_callee->at(0)->ToString(&callee_candidate_pre_restart));
  const cricket::SessionDescription* desc =
      caller()->pc()->local_description()->description();
  std::string caller_ufrag_pre_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  desc = callee()->pc()->local_description()->description();
  std::string callee_ufrag_pre_restart =
      desc->transport_infos()[0].description.ice_ufrag;

  // Have the caller initiate an ICE restart.
  caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  // Grab the ufrags/candidates again.
  audio_candidates_caller = caller()->pc()->local_description()->candidates(0);
  audio_candidates_callee = callee()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_caller->count(), 0u);
  ASSERT_GT(audio_candidates_callee->count(), 0u);
  std::string caller_candidate_post_restart;
  ASSERT_TRUE(
      audio_candidates_caller->at(0)->ToString(&caller_candidate_post_restart));
  std::string callee_candidate_post_restart;
  ASSERT_TRUE(
      audio_candidates_callee->at(0)->ToString(&callee_candidate_post_restart));
  desc = caller()->pc()->local_description()->description();
  std::string caller_ufrag_post_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  desc = callee()->pc()->local_description()->description();
  std::string callee_ufrag_post_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  // Sanity check that an ICE restart was actually negotiated in SDP.
  ASSERT_NE(caller_candidate_pre_restart, caller_candidate_post_restart);
  ASSERT_NE(callee_candidate_pre_restart, callee_candidate_post_restart);
  ASSERT_NE(caller_ufrag_pre_restart, caller_ufrag_post_restart);
  ASSERT_NE(callee_ufrag_pre_restart, callee_ufrag_post_restart);

  // Ensure that additional frames are received after the ICE restart.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Verify that audio/video can be received end-to-end when ICE renomination is
// enabled.
TEST_F(PeerConnectionIntegrationTest, EndToEndCallWithIceRenomination) {
  PeerConnectionInterface::RTCConfiguration config;
  config.enable_ice_renomination = true;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  // Do normal offer/answer and wait for some frames to be received in each
  // direction.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Sanity check that ICE renomination was actually negotiated.
  const cricket::SessionDescription* desc =
      caller()->pc()->local_description()->description();
  for (const cricket::TransportInfo& info : desc->transport_infos()) {
    ASSERT_NE(
        info.description.transport_options.end(),
        std::find(info.description.transport_options.begin(),
                  info.description.transport_options.end(), "renomination"));
  }
  desc = callee()->pc()->local_description()->description();
  for (const cricket::TransportInfo& info : desc->transport_infos()) {
    ASSERT_NE(
        info.description.transport_options.end(),
        std::find(info.description.transport_options.begin(),
                  info.description.transport_options.end(), "renomination"));
  }
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// With a max bundle policy and RTCP muxing, adding a new media description to
// the connection should not affect ICE at all because the new media will use
// the existing connection.
TEST_F(PeerConnectionIntegrationTest,
       AddMediaToConnectedBundleDoesNotRestartIce) {
  PeerConnectionInterface::RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(
      config, PeerConnectionInterface::RTCConfiguration()));
  ConnectFakeSignaling();

  caller()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);

  caller()->clear_ice_connection_state_history();

  caller()->AddVideoOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_EQ(0u, caller()->ice_connection_state_history().size());
}

// This test sets up a call between two parties with audio and video. It then
// renegotiates setting the video m-line to "port 0", then later renegotiates
// again, enabling video.
TEST_F(PeerConnectionIntegrationTest,
       VideoFlowsAfterMediaSectionIsRejectedAndRecycled) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Do initial negotiation, only sending media from the caller. Will result in
  // video and audio recvonly "m=" sections.
  caller()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Negotiate again, disabling the video "m=" section (the callee will set the
  // port to 0 due to offer_to_receive_video = 0).
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_video = 0;
  callee()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Sanity check that video "m=" section was actually rejected.
  const ContentInfo* answer_video_content = cricket::GetFirstVideoContent(
      callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, answer_video_content);
  ASSERT_TRUE(answer_video_content->rejected);

  // Enable video and do negotiation again, making sure video is received
  // end-to-end, also adding media stream to callee.
  options.offer_to_receive_video = 1;
  callee()->SetOfferAnswerOptions(options);
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Verify the caller receives frames from the newly added stream, and the
  // callee receives additional frames from the re-enabled video m= section.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test sets up a Jsep call between two parties with external
// VideoDecoderFactory.
// TODO(holmer): Disabled due to sometimes crashing on buildbots.
// See issue webrtc/2378.
TEST_F(PeerConnectionIntegrationTest,
       DISABLED_EndToEndCallWithVideoDecoderFactory) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  EnableVideoDecoderFactory();
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This tests that if we negotiate after calling CreateSender but before we
// have a track, then set a track later, frames from the newly-set track are
// received end-to-end.
// TODO(deadbeef): Change this test to use AddTransceiver, once that's
// implemented.
TEST_F(PeerConnectionIntegrationTest,
       MediaFlowsAfterEarlyWarmupWithCreateSender) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto caller_audio_sender =
      caller()->pc()->CreateSender("audio", "caller_stream");
  auto caller_video_sender =
      caller()->pc()->CreateSender("video", "caller_stream");
  auto callee_audio_sender =
      callee()->pc()->CreateSender("audio", "callee_stream");
  auto callee_video_sender =
      callee()->pc()->CreateSender("video", "callee_stream");
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);
  // Wait for ICE to complete, without any tracks being set.
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(webrtc::PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);
  // Now set the tracks, and expect frames to immediately start flowing.
  EXPECT_TRUE(caller_audio_sender->SetTrack(caller()->CreateLocalAudioTrack()));
  EXPECT_TRUE(caller_video_sender->SetTrack(caller()->CreateLocalVideoTrack()));
  EXPECT_TRUE(callee_audio_sender->SetTrack(callee()->CreateLocalAudioTrack()));
  EXPECT_TRUE(callee_video_sender->SetTrack(callee()->CreateLocalVideoTrack()));
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// This test verifies that a remote video track can be added via AddStream,
// and sent end-to-end. For this particular test, it's simply echoed back
// from the caller to the callee, rather than being forwarded to a third
// PeerConnection.
TEST_F(PeerConnectionIntegrationTest, CanSendRemoteVideoTrack) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Just send a video track from the caller.
  caller()->AddMediaStreamFromTracks(nullptr,
                                     caller()->CreateLocalVideoTrack());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);
  ASSERT_EQ(1, callee()->remote_streams()->count());

  // Echo the stream back, and do a new offer/anwer (initiated by callee this
  // time).
  callee()->pc()->AddStream(callee()->remote_streams()->at(0));
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);

  int expected_caller_received_video_frames = kDefaultExpectedVideoFrameCount;
  ExpectNewFramesReceivedWithWait(0, expected_caller_received_video_frames, 0,
                                  0, kMaxWaitForFramesMs);
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
TEST_F(PeerConnectionIntegrationTest, EndToEndConnectionTimeWithTurnTurnPair) {
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

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(client_1_config, client_2_config));
  // Set up the simulated delays.
  SetSignalingDelayMs(signaling_trip_delay_ms);
  ConnectFakeSignaling();
  virtual_socket_server()->set_delay_mean(media_hop_delay_ms);
  virtual_socket_server()->UpdateDelayDistribution();

  // Set "offer to receive audio/video" without adding any tracks, so we just
  // set up ICE/DTLS with no media.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  EXPECT_TRUE_SIMULATED_WAIT(DtlsConnected(), total_connection_time_ms,
                             fake_clock);
  // Need to free the clients here since they're using things we created on
  // the stack.
  delete SetCallerPcWrapperAndReturnCurrent(nullptr);
  delete SetCalleePcWrapperAndReturnCurrent(nullptr);
}

// Verify that a TurnCustomizer passed in through RTCConfiguration
// is actually used by the underlying TURN candidate pair.
// Note that turnport_unittest.cc contains more detailed, lower-level tests.
TEST_F(PeerConnectionIntegrationTest,           \
       TurnCustomizerUsedForTurnConnections) {
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

  PeerConnectionInterface::RTCConfiguration client_1_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_1;
  ice_server_1.urls.push_back("turn:88.88.88.0:3478");
  ice_server_1.username = "test";
  ice_server_1.password = "test";
  client_1_config.servers.push_back(ice_server_1);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;
  auto customizer1 = rtc::MakeUnique<cricket::TestTurnCustomizer>();
  client_1_config.turn_customizer = customizer1.get();

  PeerConnectionInterface::RTCConfiguration client_2_config;
  webrtc::PeerConnectionInterface::IceServer ice_server_2;
  ice_server_2.urls.push_back("turn:99.99.99.0:3478");
  ice_server_2.username = "test";
  ice_server_2.password = "test";
  client_2_config.servers.push_back(ice_server_2);
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;
  auto customizer2 = rtc::MakeUnique<cricket::TestTurnCustomizer>();
  client_2_config.turn_customizer = customizer2.get();

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(client_1_config, client_2_config));
  ConnectFakeSignaling();

  // Set "offer to receive audio/video" without adding any tracks, so we just
  // set up ICE/DTLS with no media.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);

  EXPECT_GT(customizer1->allow_channel_data_cnt_, 0u);
  EXPECT_GT(customizer1->modify_cnt_, 0u);

  EXPECT_GT(customizer2->allow_channel_data_cnt_, 0u);
  EXPECT_GT(customizer2->modify_cnt_, 0u);

  // Need to free the clients here since they're using things we created on
  // the stack.
  delete SetCallerPcWrapperAndReturnCurrent(nullptr);
  delete SetCalleePcWrapperAndReturnCurrent(nullptr);
}

// Test that audio and video flow end-to-end when codec names don't use the
// expected casing, given that they're supposed to be case insensitive. To test
// this, all but one codec is removed from each media description, and its
// casing is changed.
//
// In the past, this has regressed and caused crashes/black video, due to the
// fact that code at some layers was doing case-insensitive comparisons and
// code at other layers was not.
TEST_F(PeerConnectionIntegrationTest, CodecNamesAreCaseInsensitive) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();

  // Remove all but one audio/video codec (opus and VP8), and change the
  // casing of the caller's generated offer.
  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* description) {
    cricket::AudioContentDescription* audio =
        GetFirstAudioContentDescription(description);
    ASSERT_NE(nullptr, audio);
    auto audio_codecs = audio->codecs();
    audio_codecs.erase(std::remove_if(audio_codecs.begin(), audio_codecs.end(),
                                      [](const cricket::AudioCodec& codec) {
                                        return codec.name != "opus";
                                      }),
                       audio_codecs.end());
    ASSERT_EQ(1u, audio_codecs.size());
    audio_codecs[0].name = "OpUs";
    audio->set_codecs(audio_codecs);

    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(description);
    ASSERT_NE(nullptr, video);
    auto video_codecs = video->codecs();
    video_codecs.erase(std::remove_if(video_codecs.begin(), video_codecs.end(),
                                      [](const cricket::VideoCodec& codec) {
                                        return codec.name != "VP8";
                                      }),
                       video_codecs.end());
    ASSERT_EQ(1u, video_codecs.size());
    video_codecs[0].name = "vP8";
    video->set_codecs(video_codecs);
  });

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Verify frames are still received end-to-end.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

TEST_F(PeerConnectionIntegrationTest, GetSources) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for one audio frame to be received by the callee.
  ExpectNewFramesReceivedWithWait(0, 0, 1, 0, kMaxWaitForFramesMs);
  ASSERT_GT(callee()->pc()->GetReceivers().size(), 0u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  ASSERT_EQ(receiver->media_type(), cricket::MEDIA_TYPE_AUDIO);

  auto contributing_sources = receiver->GetSources();
  ASSERT_GT(receiver->GetParameters().encodings.size(), 0u);
  EXPECT_EQ(receiver->GetParameters().encodings[0].ssrc,
            contributing_sources[0].source_id());
}

// Test that if a track is removed and added again with a different stream ID,
// the new stream ID is successfully communicated in SDP and media continues to
// flow end-to-end.
TEST_F(PeerConnectionIntegrationTest, RemoveAndAddTrackWithNewStreamId) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<MediaStreamInterface> stream_1 =
      caller()->pc_factory()->CreateLocalMediaStream("stream_1");
  rtc::scoped_refptr<MediaStreamInterface> stream_2 =
      caller()->pc_factory()->CreateLocalMediaStream("stream_2");

  // Add track using stream 1, do offer/answer.
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      caller()->CreateLocalAudioTrack();
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender =
      caller()->pc()->AddTrack(track, {stream_1.get()});
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for one audio frame to be received by the callee.
  ExpectNewFramesReceivedWithWait(0, 0, 1, 0, kMaxWaitForFramesMs);

  // Remove the sender, and create a new one with the new stream.
  caller()->pc()->RemoveTrack(sender);
  sender = caller()->pc()->AddTrack(track, {stream_2.get()});
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait for additional audio frames to be received by the callee.
  ExpectNewFramesReceivedWithWait(0, 0, kDefaultExpectedAudioFrameCount, 0,
                                  kMaxWaitForFramesMs);
}

TEST_F(PeerConnectionIntegrationTest, RtcEventLogOutputWriteCalled) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  auto output = rtc::MakeUnique<testing::NiceMock<MockRtcEventLogOutput>>();
  ON_CALL(*output, IsActive()).WillByDefault(testing::Return(true));
  ON_CALL(*output, Write(::testing::_)).WillByDefault(testing::Return(true));
  EXPECT_CALL(*output, Write(::testing::_)).Times(::testing::AtLeast(1));
  EXPECT_TRUE(caller()->pc()->StartRtcEventLog(
      std::move(output), webrtc::RtcEventLog::kImmediateOutput));

  caller()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
}

// Test that if candidates are only signaled by applying full session
// descriptions (instead of using AddIceCandidate), the peers can connect to
// each other and exchange media.
TEST_F(PeerConnectionIntegrationTest, MediaFlowsWhenCandidatesSetOnlyInSdp) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  // Each side will signal the session descriptions but not candidates.
  ConnectFakeSignalingForSdpOnly();

  // Add audio video track and exchange the initial offer/answer with media
  // information only. This will start ICE gathering on each side.
  caller()->AddAudioVideoMediaStream();
  callee()->AddAudioVideoMediaStream();
  caller()->CreateAndSetAndSignalOffer();

  // Wait for all candidates to be gathered on both the caller and callee.
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 caller()->ice_gathering_state(), kDefaultTimeout);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 callee()->ice_gathering_state(), kDefaultTimeout);

  // The candidates will now be included in the session description, so
  // signaling them will start the ICE connection.
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Ensure that media flows in both directions.
  ExpectNewFramesReceivedWithWait(
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kDefaultExpectedAudioFrameCount, kDefaultExpectedVideoFrameCount,
      kMaxWaitForFramesMs);
}

// Test that SetAudioPlayout can be used to disable audio playout from the
// start, then later enable it. This may be useful, for example, if the caller
// needs to play a local ringtone until some event occurs, after which it
// switches to playing the received audio.
TEST_F(PeerConnectionIntegrationTest, DisableAndEnableAudioPlayout) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Set up audio-only call where audio playout is disabled on caller's side.
  caller()->pc()->SetAudioPlayout(false);
  caller()->AddAudioOnlyMediaStream();
  callee()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Pump messages for a second.
  WAIT(false, 1000);
  // Since audio playout is disabled, the caller shouldn't have received
  // anything (at the playout level, at least).
  EXPECT_EQ(0, caller()->audio_frames_received());
  // As a sanity check, make sure the callee (for which playout isn't disabled)
  // did still see frames on its audio level.
  ASSERT_GT(callee()->audio_frames_received(), 0);

  // Enable playout again, and ensure audio starts flowing.
  caller()->pc()->SetAudioPlayout(true);
  ExpectNewFramesReceivedWithWait(kDefaultExpectedAudioFrameCount, 0,
                                  kDefaultExpectedAudioFrameCount, 0,
                                  kMaxWaitForFramesMs);
}

double GetAudioEnergyStat(PeerConnectionWrapper* pc) {
  auto report = pc->NewGetStats();
  auto track_stats_list =
      report->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>();
  const webrtc::RTCMediaStreamTrackStats* remote_track_stats = nullptr;
  for (const auto* track_stats : track_stats_list) {
    if (track_stats->remote_source.is_defined() &&
        *track_stats->remote_source) {
      remote_track_stats = track_stats;
      break;
    }
  }

  if (!remote_track_stats->total_audio_energy.is_defined()) {
    return 0.0;
  }
  return *remote_track_stats->total_audio_energy;
}

// Test that if audio playout is disabled via the SetAudioPlayout() method, then
// incoming audio is still processed and statistics are generated.
TEST_F(PeerConnectionIntegrationTest,
       DisableAudioPlayoutStillGeneratesAudioStats) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Set up audio-only call where playout is disabled but audio-processing is
  // still active.
  caller()->AddAudioOnlyMediaStream();
  callee()->AddAudioOnlyMediaStream();
  caller()->pc()->SetAudioPlayout(false);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Wait for the callee to receive audio stats.
  EXPECT_TRUE_WAIT(GetAudioEnergyStat(caller()) > 0, kMaxWaitForFramesMs);
}

// Test that SetAudioRecording can be used to disable audio recording from the
// start, then later enable it. This may be useful, for example, if the caller
// wants to ensure that no audio resources are active before a certain state
// is reached.
TEST_F(PeerConnectionIntegrationTest, DisableAndEnableAudioRecording) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  // Set up audio-only call where audio recording is disabled on caller's side.
  caller()->pc()->SetAudioRecording(false);
  caller()->AddAudioOnlyMediaStream();
  callee()->AddAudioOnlyMediaStream();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  // Pump messages for a second.
  WAIT(false, 1000);
  // Since caller has disabled audio recording, the callee shouldn't have
  // received anything.
  EXPECT_EQ(0, callee()->audio_frames_received());
  // As a sanity check, make sure the caller did still see frames on its
  // audio level since audio recording is enabled on the calle side.
  ASSERT_GT(caller()->audio_frames_received(), 0);

  // Enable audio recording again, and ensure audio starts flowing.
  caller()->pc()->SetAudioRecording(true);
  ExpectNewFramesReceivedWithWait(kDefaultExpectedAudioFrameCount, 0,
                                  kDefaultExpectedAudioFrameCount, 0,
                                  kMaxWaitForFramesMs);
}

// Test that after closing PeerConnections, they stop sending any packets (ICE,
// DTLS, RTP...).
TEST_F(PeerConnectionIntegrationTest, ClosingConnectionStopsPacketFlow) {
  // Set up audio/video/data, wait for some frames to be received.
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoMediaStream();
#ifdef HAVE_SCTP
  caller()->CreateDataChannel();
#endif
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ExpectNewFramesReceivedWithWait(0, 0, kDefaultExpectedAudioFrameCount,
                                  kDefaultExpectedAudioFrameCount,
                                  kMaxWaitForFramesMs);
  // Close PeerConnections.
  caller()->pc()->Close();
  callee()->pc()->Close();
  // Pump messages for a second, and ensure no new packets end up sent.
  uint32_t sent_packets_a = virtual_socket_server()->sent_packets();
  WAIT(false, 1000);
  uint32_t sent_packets_b = virtual_socket_server()->sent_packets();
  EXPECT_EQ(sent_packets_a, sent_packets_b);
}

}  // namespace

#endif  // if !defined(THREAD_SANITIZER)
