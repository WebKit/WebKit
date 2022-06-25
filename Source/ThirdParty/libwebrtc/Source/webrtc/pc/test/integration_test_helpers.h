/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_INTEGRATION_TEST_HELPERS_H_
#define PC_TEST_INTEGRATION_TEST_HELPERS_H_

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/types/optional.h"
#include "api/audio_options.h"
#include "api/call/call_factory_interface.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/data_channel_interface.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/uma_metrics.h"
#include "api/video/video_rotation.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/call.h"
#include "logging/rtc_event_log/fake_rtc_event_log_factory.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "media/engine/fake_webrtc_video_engine.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_media_engine_defaults.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "p2p/base/fake_ice_transport.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/mock_async_resolver.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/test_stun_server.h"
#include "p2p/base/test_turn_customizer.h"
#include "p2p/base/test_turn_server.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/dtmf_sender.h"
#include "pc/local_audio_source.h"
#include "pc/media_session.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_factory.h"
#include "pc/peer_connection_proxy.h"
#include "pc/rtp_media_utils.h"
#include "pc/session_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_periodic_video_source.h"
#include "pc/test/fake_periodic_video_track_source.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "pc/test/fake_video_track_renderer.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_mdns_responder.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/firewall_socket_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/mdns_responder_interface.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/test_certificate_verifier.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {

using ::cricket::ContentInfo;
using ::cricket::StreamParams;
using ::rtc::SocketAddress;
using ::testing::_;
using ::testing::Combine;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

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
static const int kDefaultSrtpCryptoSuite = rtc::kSrtpAes128CmSha1_80;
static const int kDefaultSrtpCryptoSuiteGcm = rtc::kSrtpAeadAes256Gcm;

static const SocketAddress kDefaultLocalAddress("192.168.1.1", 0);

// Helper function for constructing offer/answer options to initiate an ICE
// restart.
PeerConnectionInterface::RTCOfferAnswerOptions IceRestartOfferAnswerOptions();

// Remove all stream information (SSRCs, track IDs, etc.) and "msid-semantic"
// attribute from received SDP, simulating a legacy endpoint.
void RemoveSsrcsAndMsids(cricket::SessionDescription* desc);

// Removes all stream information besides the stream ids, simulating an
// endpoint that only signals a=msid lines to convey stream_ids.
void RemoveSsrcsAndKeepMsids(cricket::SessionDescription* desc);

int FindFirstMediaStatsIndexByKind(
    const std::string& kind,
    const std::vector<const webrtc::RTCMediaStreamTrackStats*>&
        media_stats_vec);

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
// PeerConnectionWrapper defined in peerconnectionwrapper.h.
class PeerConnectionIntegrationWrapper : public webrtc::PeerConnectionObserver,
                                         public SignalingMessageReceiver {
 public:
  webrtc::PeerConnectionFactoryInterface* pc_factory() const {
    return peer_connection_factory_.get();
  }

  webrtc::PeerConnectionInterface* pc() const { return peer_connection_.get(); }

  // If a signaling message receiver is set (via ConnectFakeSignaling), this
  // will set the whole offer/answer exchange in motion. Just need to wait for
  // the signaling state to reach "stable".
  void CreateAndSetAndSignalOffer() {
    auto offer = CreateOfferAndWait();
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

  // Set a callback to be invoked when a remote offer is received via the fake
  // signaling channel. This provides an opportunity to change the
  // PeerConnection state before an answer is created and sent to the caller.
  void SetRemoteOfferHandler(std::function<void()> handler) {
    remote_offer_handler_ = std::move(handler);
  }

  void SetRemoteAsyncResolver(rtc::MockAsyncResolver* resolver) {
    remote_async_resolver_ = resolver;
  }

  // Every ICE connection state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceConnectionState>
  ice_connection_state_history() const {
    return ice_connection_state_history_;
  }
  void clear_ice_connection_state_history() {
    ice_connection_state_history_.clear();
  }

  // Every standardized ICE connection state in order that has been seen by the
  // observer.
  std::vector<PeerConnectionInterface::IceConnectionState>
  standardized_ice_connection_state_history() const {
    return standardized_ice_connection_state_history_;
  }

  // Every PeerConnection state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::PeerConnectionState>
  peer_connection_state_history() const {
    return peer_connection_state_history_;
  }

  // Every ICE gathering state in order that has been seen by the observer.
  std::vector<PeerConnectionInterface::IceGatheringState>
  ice_gathering_state_history() const {
    return ice_gathering_state_history_;
  }
  std::vector<cricket::CandidatePairChangeEvent>
  ice_candidate_pair_change_history() const {
    return ice_candidate_pair_change_history_;
  }

  // Every PeerConnection signaling state in order that has been seen by the
  // observer.
  std::vector<PeerConnectionInterface::SignalingState>
  peer_connection_signaling_state_history() const {
    return peer_connection_signaling_state_history_;
  }

  void AddAudioVideoTracks() {
    AddAudioTrack();
    AddVideoTrack();
  }

  rtc::scoped_refptr<RtpSenderInterface> AddAudioTrack() {
    return AddTrack(CreateLocalAudioTrack());
  }

  rtc::scoped_refptr<RtpSenderInterface> AddVideoTrack() {
    return AddTrack(CreateLocalVideoTrack());
  }

  rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateLocalAudioTrack() {
    cricket::AudioOptions options;
    // Disable highpass filter so that we can get all the test audio frames.
    options.highpass_filter = false;
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(options);
    // TODO(perkj): Test audio source when it is implemented. Currently audio
    // always use the default input.
    return peer_connection_factory_->CreateAudioTrack(rtc::CreateRandomUuid(),
                                                      source);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack() {
    webrtc::FakePeriodicVideoSource::Config config;
    config.timestamp_offset_ms = rtc::TimeMillis();
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackWithConfig(
      webrtc::FakePeriodicVideoSource::Config config) {
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface>
  CreateLocalVideoTrackWithRotation(webrtc::VideoRotation rotation) {
    webrtc::FakePeriodicVideoSource::Config config;
    config.rotation = rotation;
    config.timestamp_offset_ms = rtc::TimeMillis();
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids = {}) {
    auto result = pc()->AddTrack(track, stream_ids);
    EXPECT_EQ(RTCErrorType::NONE, result.error().type());
    return result.MoveValue();
  }

  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceiversOfType(
      cricket::MediaType media_type) {
    std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers;
    for (const auto& receiver : pc()->GetReceivers()) {
      if (receiver->media_type() == media_type) {
        receivers.push_back(receiver);
      }
    }
    return receivers;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> GetFirstTransceiverOfType(
      cricket::MediaType media_type) {
    for (auto transceiver : pc()->GetTransceivers()) {
      if (transceiver->receiver()->media_type() == media_type) {
        return transceiver;
      }
    }
    return nullptr;
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
    if (fake_video_renderers_.empty()) {
      return 0;
    }

    for (const auto& pair : fake_video_renderers_) {
      min_frames = std::min(min_frames, pair.second->num_rendered_frames());
    }
    return min_frames;
  }

  // Returns a MockStatsObserver in a state after stats gathering finished,
  // which can be used to access the gathered stats.
  rtc::scoped_refptr<MockStatsObserver> OldGetStatsForTrack(
      webrtc::MediaStreamTrackInterface* track) {
    auto observer = rtc::make_ref_counted<MockStatsObserver>();
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
    auto callback =
        rtc::make_ref_counted<webrtc::MockRTCStatsCollectorCallback>();
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

  webrtc::PeerConnectionInterface::IceConnectionState
  standardized_ice_connection_state() {
    return pc()->standardized_ice_connection_state();
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

  rtc::FakeNetworkManager* network_manager() const {
    return fake_network_manager_.get();
  }
  cricket::PortAllocator* port_allocator() const { return port_allocator_; }

  webrtc::FakeRtcEventLogFactory* event_log_factory() const {
    return event_log_factory_;
  }

  const cricket::Candidate& last_candidate_gathered() const {
    return last_candidate_gathered_;
  }
  const cricket::IceCandidateErrorEvent& error_event() const {
    return error_event_;
  }

  // Sets the mDNS responder for the owned fake network manager and keeps a
  // reference to the responder.
  void SetMdnsResponder(
      std::unique_ptr<webrtc::FakeMdnsResponder> mdns_responder) {
    RTC_DCHECK(mdns_responder != nullptr);
    mdns_responder_ = mdns_responder.get();
    network_manager()->set_mdns_responder(std::move(mdns_responder));
  }

  // Returns null on failure.
  std::unique_ptr<SessionDescriptionInterface> CreateOfferAndWait() {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc()->CreateOffer(observer, offer_answer_options_);
    return WaitForDescriptionFromObserver(observer);
  }
  bool Rollback() {
    return SetRemoteDescription(
        webrtc::CreateSessionDescription(SdpType::kRollback, ""));
  }

  // Functions for querying stats.
  void StartWatchingDelayStats() {
    // Get the baseline numbers for audio_packets and audio_delay.
    auto received_stats = NewGetStats();
    auto track_stats =
        received_stats->GetStatsOfType<webrtc::RTCMediaStreamTrackStats>()[0];
    ASSERT_TRUE(track_stats->relative_packet_arrival_delay.is_defined());
    auto rtp_stats =
        received_stats->GetStatsOfType<webrtc::RTCInboundRTPStreamStats>()[0];
    ASSERT_TRUE(rtp_stats->packets_received.is_defined());
    ASSERT_TRUE(rtp_stats->track_id.is_defined());
    audio_track_stats_id_ = track_stats->id();
    ASSERT_TRUE(received_stats->Get(audio_track_stats_id_));
    rtp_stats_id_ = rtp_stats->id();
    ASSERT_EQ(audio_track_stats_id_, *rtp_stats->track_id);
    audio_packets_stat_ = *rtp_stats->packets_received;
    audio_delay_stat_ = *track_stats->relative_packet_arrival_delay;
    audio_samples_stat_ = *track_stats->total_samples_received;
    audio_concealed_stat_ = *track_stats->concealed_samples;
  }

  void UpdateDelayStats(std::string tag, int desc_size) {
    auto report = NewGetStats();
    auto track_stats =
        report->GetAs<webrtc::RTCMediaStreamTrackStats>(audio_track_stats_id_);
    ASSERT_TRUE(track_stats);
    auto rtp_stats =
        report->GetAs<webrtc::RTCInboundRTPStreamStats>(rtp_stats_id_);
    ASSERT_TRUE(rtp_stats);
    auto delta_packets = *rtp_stats->packets_received - audio_packets_stat_;
    auto delta_rpad =
        *track_stats->relative_packet_arrival_delay - audio_delay_stat_;
    auto recent_delay = delta_packets > 0 ? delta_rpad / delta_packets : -1;
    // The purpose of these checks is to sound the alarm early if we introduce
    // serious regressions. The numbers are not acceptable for production, but
    // occur on slow bots.
    //
    // An average relative packet arrival delay over the renegotiation of
    // > 100 ms indicates that something is dramatically wrong, and will impact
    // quality for sure.
    // Worst bots:
    // linux_x86_dbg at 0.206
#if !defined(NDEBUG)
    EXPECT_GT(0.25, recent_delay) << tag << " size " << desc_size;
#else
    EXPECT_GT(0.1, recent_delay) << tag << " size " << desc_size;
#endif
    auto delta_samples =
        *track_stats->total_samples_received - audio_samples_stat_;
    auto delta_concealed =
        *track_stats->concealed_samples - audio_concealed_stat_;
    // These limits should be adjusted down as we improve:
    //
    // Concealing more than 4000 samples during a renegotiation is unacceptable.
    // But some bots are slow.

    // Worst bots:
    // linux_more_configs bot at conceal count 5184
    // android_arm_rel at conceal count 9241
    // linux_x86_dbg at 15174
#if !defined(NDEBUG)
    EXPECT_GT(18000U, delta_concealed) << "Concealed " << delta_concealed
                                       << " of " << delta_samples << " samples";
#else
    EXPECT_GT(15000U, delta_concealed) << "Concealed " << delta_concealed
                                       << " of " << delta_samples << " samples";
#endif
    // Concealing more than 20% of samples during a renegotiation is
    // unacceptable.
    // Worst bots:
    // linux_more_configs bot at conceal rate 0.516
    // linux_x86_dbg bot at conceal rate 0.854
    if (delta_samples > 0) {
#if !defined(NDEBUG)
      EXPECT_GT(0.95, 1.0 * delta_concealed / delta_samples)
          << "Concealed " << delta_concealed << " of " << delta_samples
          << " samples";
#else
      EXPECT_GT(0.6, 1.0 * delta_concealed / delta_samples)
          << "Concealed " << delta_concealed << " of " << delta_samples
          << " samples";
#endif
    }
    // Increment trailing counters
    audio_packets_stat_ = *rtp_stats->packets_received;
    audio_delay_stat_ = *track_stats->relative_packet_arrival_delay;
    audio_samples_stat_ = *track_stats->total_samples_received;
    audio_concealed_stat_ = *track_stats->concealed_samples;
  }

  // Sets number of candidates expected
  void ExpectCandidates(int candidate_count) {
    candidates_expected_ = candidate_count;
  }

 private:
  // Constructor used by friend class PeerConnectionIntegrationBaseTest.
  explicit PeerConnectionIntegrationWrapper(const std::string& debug_name)
      : debug_name_(debug_name) {}

  bool Init(const PeerConnectionFactory::Options* options,
            const PeerConnectionInterface::RTCConfiguration* config,
            webrtc::PeerConnectionDependencies dependencies,
            rtc::Thread* network_thread,
            rtc::Thread* worker_thread,
            std::unique_ptr<webrtc::FakeRtcEventLogFactory> event_log_factory,
            bool reset_encoder_factory,
            bool reset_decoder_factory) {
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
    rtc::Thread* const signaling_thread = rtc::Thread::Current();

    webrtc::PeerConnectionFactoryDependencies pc_factory_dependencies;
    pc_factory_dependencies.network_thread = network_thread;
    pc_factory_dependencies.worker_thread = worker_thread;
    pc_factory_dependencies.signaling_thread = signaling_thread;
    pc_factory_dependencies.task_queue_factory =
        webrtc::CreateDefaultTaskQueueFactory();
    pc_factory_dependencies.trials = std::make_unique<FieldTrialBasedConfig>();
    cricket::MediaEngineDependencies media_deps;
    media_deps.task_queue_factory =
        pc_factory_dependencies.task_queue_factory.get();
    media_deps.adm = fake_audio_capture_module_;
    webrtc::SetMediaEngineDefaults(&media_deps);

    if (reset_encoder_factory) {
      media_deps.video_encoder_factory.reset();
    }
    if (reset_decoder_factory) {
      media_deps.video_decoder_factory.reset();
    }

    if (!media_deps.audio_processing) {
      // If the standard Creation method for APM returns a null pointer, instead
      // use the builder for testing to create an APM object.
      media_deps.audio_processing = AudioProcessingBuilderForTesting().Create();
    }

    media_deps.trials = pc_factory_dependencies.trials.get();

    pc_factory_dependencies.media_engine =
        cricket::CreateMediaEngine(std::move(media_deps));
    pc_factory_dependencies.call_factory = webrtc::CreateCallFactory();
    if (event_log_factory) {
      event_log_factory_ = event_log_factory.get();
      pc_factory_dependencies.event_log_factory = std::move(event_log_factory);
    } else {
      pc_factory_dependencies.event_log_factory =
          std::make_unique<webrtc::RtcEventLogFactory>(
              pc_factory_dependencies.task_queue_factory.get());
    }
    peer_connection_factory_ = webrtc::CreateModularPeerConnectionFactory(
        std::move(pc_factory_dependencies));

    if (!peer_connection_factory_) {
      return false;
    }
    if (options) {
      peer_connection_factory_->SetOptions(*options);
    }
    if (config) {
      sdp_semantics_ = config->sdp_semantics;
    }

    dependencies.allocator = std::move(port_allocator);
    peer_connection_ = CreatePeerConnection(config, std::move(dependencies));
    return peer_connection_.get() != nullptr;
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies) {
    PeerConnectionInterface::RTCConfiguration modified_config;
    // If `config` is null, this will result in a default configuration being
    // used.
    if (config) {
      modified_config = *config;
    }
    // Disable resolution adaptation; we don't want it interfering with the
    // test results.
    // TODO(deadbeef): Do something more robust. Since we're testing for aspect
    // ratios and not specific resolutions, is this even necessary?
    modified_config.set_cpu_adaptation(false);

    dependencies.observer = this;
    return peer_connection_factory_->CreatePeerConnection(
        modified_config, std::move(dependencies));
  }

  void set_signaling_message_receiver(
      SignalingMessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  void set_signaling_delay_ms(int delay_ms) { signaling_delay_ms_ = delay_ms; }

  void set_signal_ice_candidates(bool signal) {
    signal_ice_candidates_ = signal;
  }

  rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrackInternal(
      webrtc::FakePeriodicVideoSource::Config config) {
    // Set max frame rate to 10fps to reduce the risk of test flakiness.
    // TODO(deadbeef): Do something more robust.
    config.frame_interval_ms = 100;

    video_track_sources_.emplace_back(
        rtc::make_ref_counted<webrtc::FakePeriodicVideoTrackSource>(
            config, false /* remote */));
    rtc::scoped_refptr<webrtc::VideoTrackInterface> track(
        peer_connection_factory_->CreateVideoTrack(
            rtc::CreateRandomUuid(), video_track_sources_.back()));
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
    if (remote_offer_handler_) {
      remote_offer_handler_();
    }
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
  std::unique_ptr<SessionDescriptionInterface> CreateAnswer() {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
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
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    RTC_LOG(LS_INFO) << debug_name_ << ": SetLocalDescriptionAndSendSdpMessage";
    SdpType type = desc->GetType();
    std::string sdp;
    EXPECT_TRUE(desc->ToString(&sdp));
    RTC_LOG(LS_INFO) << debug_name_ << ": local SDP contents=\n" << sdp;
    pc()->SetLocalDescription(observer, desc.release());
    RemoveUnusedVideoRenderers();
    // As mentioned above, we need to send the message immediately after
    // SetLocalDescription.
    SendSdpMessage(type, sdp);
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return true;
  }

  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    RTC_LOG(LS_INFO) << debug_name_ << ": SetRemoteDescription";
    pc()->SetRemoteDescription(observer, desc.release());
    RemoveUnusedVideoRenderers();
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer->result();
  }

  // This is a work around to remove unused fake_video_renderers from
  // transceivers that have either stopped or are no longer receiving.
  void RemoveUnusedVideoRenderers() {
    if (sdp_semantics_ != SdpSemantics::kUnifiedPlan) {
      return;
    }
    auto transceivers = pc()->GetTransceivers();
    std::set<std::string> active_renderers;
    for (auto& transceiver : transceivers) {
      // Note - we don't check for direction here. This function is called
      // before direction is set, and in that case, we should not remove
      // the renderer.
      if (transceiver->receiver()->media_type() == cricket::MEDIA_TYPE_VIDEO) {
        active_renderers.insert(transceiver->receiver()->track()->id());
      }
    }
    for (auto it = fake_video_renderers_.begin();
         it != fake_video_renderers_.end();) {
      // Remove fake video renderers belonging to any non-active transceivers.
      if (!active_renderers.count(it->first)) {
        it = fake_video_renderers_.erase(it);
      } else {
        it++;
      }
    }
  }

  // Simulate sending a blob of SDP with delay `signaling_delay_ms_` (0 by
  // default).
  void SendSdpMessage(SdpType type, const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelaySdpMessageIfReceiverExists(type, msg);
    } else {
      rtc::Thread::Current()->PostDelayedTask(
          ToQueuedTask(task_safety_.flag(),
                       [this, type, msg] {
                         RelaySdpMessageIfReceiverExists(type, msg);
                       }),
          signaling_delay_ms_);
    }
  }

  void RelaySdpMessageIfReceiverExists(SdpType type, const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveSdpMessage(type, msg);
    }
  }

  // Simulate trickling an ICE candidate with delay `signaling_delay_ms_` (0 by
  // default).
  void SendIceMessage(const std::string& sdp_mid,
                      int sdp_mline_index,
                      const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelayIceMessageIfReceiverExists(sdp_mid, sdp_mline_index, msg);
    } else {
      rtc::Thread::Current()->PostDelayedTask(
          ToQueuedTask(task_safety_.flag(),
                       [this, sdp_mid, sdp_mline_index, msg] {
                         RelayIceMessageIfReceiverExists(sdp_mid,
                                                         sdp_mline_index, msg);
                       }),
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
    peer_connection_signaling_state_history_.push_back(new_state);
  }
  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override {
    if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      rtc::scoped_refptr<VideoTrackInterface> video_track(
          static_cast<VideoTrackInterface*>(receiver->track().get()));
      ASSERT_TRUE(fake_video_renderers_.find(video_track->id()) ==
                  fake_video_renderers_.end());
      fake_video_renderers_[video_track->id()] =
          std::make_unique<FakeVideoTrackRenderer>(video_track);
    }
  }
  void OnRemoveTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver) override {
    if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      auto it = fake_video_renderers_.find(receiver->track()->id());
      if (it != fake_video_renderers_.end()) {
        fake_video_renderers_.erase(it);
      } else {
        RTC_LOG(LS_ERROR) << "OnRemoveTrack called for non-active renderer";
      }
    }
  }
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    EXPECT_EQ(pc()->ice_connection_state(), new_state);
    ice_connection_state_history_.push_back(new_state);
  }
  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    standardized_ice_connection_state_history_.push_back(new_state);
  }
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override {
    peer_connection_state_history_.push_back(new_state);
  }

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    EXPECT_EQ(pc()->ice_gathering_state(), new_state);
    ice_gathering_state_history_.push_back(new_state);
  }

  void OnIceSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& event) {
    ice_candidate_pair_change_history_.push_back(event);
  }

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnIceCandidate";

    if (remote_async_resolver_) {
      const auto& local_candidate = candidate->candidate();
      if (local_candidate.address().IsUnresolvedIP()) {
        RTC_DCHECK(local_candidate.type() == cricket::LOCAL_PORT_TYPE);
        rtc::SocketAddress resolved_addr(local_candidate.address());
        const auto resolved_ip = mdns_responder_->GetMappedAddressForName(
            local_candidate.address().hostname());
        RTC_DCHECK(!resolved_ip.IsNil());
        resolved_addr.SetResolvedIP(resolved_ip);
        EXPECT_CALL(*remote_async_resolver_, GetResolvedAddress(_, _))
            .WillOnce(DoAll(SetArgPointee<1>(resolved_addr), Return(true)));
        EXPECT_CALL(*remote_async_resolver_, Destroy(_));
      }
    }

    // Check if we expected to have a candidate.
    EXPECT_GT(candidates_expected_, 1);
    candidates_expected_--;
    std::string ice_sdp;
    EXPECT_TRUE(candidate->ToString(&ice_sdp));
    if (signaling_message_receiver_ == nullptr || !signal_ice_candidates_) {
      // Remote party may be deleted.
      return;
    }
    SendIceMessage(candidate->sdp_mid(), candidate->sdp_mline_index(), ice_sdp);
    last_candidate_gathered_ = candidate->candidate();
  }
  void OnIceCandidateError(const std::string& address,
                           int port,
                           const std::string& url,
                           int error_code,
                           const std::string& error_text) override {
    error_event_ = cricket::IceCandidateErrorEvent(address, port, url,
                                                   error_code, error_text);
  }
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnDataChannel";
    data_channel_ = data_channel;
    data_observer_.reset(new MockDataChannelObserver(data_channel));
  }

  std::string debug_name_;

  std::unique_ptr<rtc::FakeNetworkManager> fake_network_manager_;
  // Reference to the mDNS responder owned by `fake_network_manager_` after set.
  webrtc::FakeMdnsResponder* mdns_responder_ = nullptr;

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

  // For remote peer communication.
  SignalingMessageReceiver* signaling_message_receiver_ = nullptr;
  int signaling_delay_ms_ = 0;
  bool signal_ice_candidates_ = true;
  cricket::Candidate last_candidate_gathered_;
  cricket::IceCandidateErrorEvent error_event_;

  // Store references to the video sources we've created, so that we can stop
  // them, if required.
  std::vector<rtc::scoped_refptr<webrtc::VideoTrackSource>>
      video_track_sources_;
  // `local_video_renderer_` attached to the first created local video track.
  std::unique_ptr<webrtc::FakeVideoTrackRenderer> local_video_renderer_;

  SdpSemantics sdp_semantics_;
  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options_;
  std::function<void(cricket::SessionDescription*)> received_sdp_munger_;
  std::function<void(cricket::SessionDescription*)> generated_sdp_munger_;
  std::function<void()> remote_offer_handler_;
  rtc::MockAsyncResolver* remote_async_resolver_ = nullptr;
  rtc::scoped_refptr<DataChannelInterface> data_channel_;
  std::unique_ptr<MockDataChannelObserver> data_observer_;

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> rtp_receiver_observers_;

  std::vector<PeerConnectionInterface::IceConnectionState>
      ice_connection_state_history_;
  std::vector<PeerConnectionInterface::IceConnectionState>
      standardized_ice_connection_state_history_;
  std::vector<PeerConnectionInterface::PeerConnectionState>
      peer_connection_state_history_;
  std::vector<PeerConnectionInterface::IceGatheringState>
      ice_gathering_state_history_;
  std::vector<cricket::CandidatePairChangeEvent>
      ice_candidate_pair_change_history_;
  std::vector<PeerConnectionInterface::SignalingState>
      peer_connection_signaling_state_history_;
  webrtc::FakeRtcEventLogFactory* event_log_factory_;

  // Number of ICE candidates expected. The default is no limit.
  int candidates_expected_ = std::numeric_limits<int>::max();

  // Variables for tracking delay stats on an audio track
  int audio_packets_stat_ = 0;
  double audio_delay_stat_ = 0.0;
  uint64_t audio_samples_stat_ = 0;
  uint64_t audio_concealed_stat_ = 0;
  std::string rtp_stats_id_;
  std::string audio_track_stats_id_;

  ScopedTaskSafety task_safety_;

  friend class PeerConnectionIntegrationBaseTest;
};

class MockRtcEventLogOutput : public webrtc::RtcEventLogOutput {
 public:
  virtual ~MockRtcEventLogOutput() = default;
  MOCK_METHOD(bool, IsActive, (), (const, override));
  MOCK_METHOD(bool, Write, (const std::string&), (override));
};

// This helper object is used for both specifying how many audio/video frames
// are expected to be received for a caller/callee. It provides helper functions
// to specify these expectations. The object initially starts in a state of no
// expectations.
class MediaExpectations {
 public:
  enum ExpectFrames {
    kExpectSomeFrames,
    kExpectNoFrames,
    kNoExpectation,
  };

  void ExpectBidirectionalAudioAndVideo() {
    ExpectBidirectionalAudio();
    ExpectBidirectionalVideo();
  }

  void ExpectBidirectionalAudio() {
    CallerExpectsSomeAudio();
    CalleeExpectsSomeAudio();
  }

  void ExpectNoAudio() {
    CallerExpectsNoAudio();
    CalleeExpectsNoAudio();
  }

  void ExpectBidirectionalVideo() {
    CallerExpectsSomeVideo();
    CalleeExpectsSomeVideo();
  }

  void ExpectNoVideo() {
    CallerExpectsNoVideo();
    CalleeExpectsNoVideo();
  }

  void CallerExpectsSomeAudioAndVideo() {
    CallerExpectsSomeAudio();
    CallerExpectsSomeVideo();
  }

  void CalleeExpectsSomeAudioAndVideo() {
    CalleeExpectsSomeAudio();
    CalleeExpectsSomeVideo();
  }

  // Caller's audio functions.
  void CallerExpectsSomeAudio(
      int expected_audio_frames = kDefaultExpectedAudioFrameCount) {
    caller_audio_expectation_ = kExpectSomeFrames;
    caller_audio_frames_expected_ = expected_audio_frames;
  }

  void CallerExpectsNoAudio() {
    caller_audio_expectation_ = kExpectNoFrames;
    caller_audio_frames_expected_ = 0;
  }

  // Caller's video functions.
  void CallerExpectsSomeVideo(
      int expected_video_frames = kDefaultExpectedVideoFrameCount) {
    caller_video_expectation_ = kExpectSomeFrames;
    caller_video_frames_expected_ = expected_video_frames;
  }

  void CallerExpectsNoVideo() {
    caller_video_expectation_ = kExpectNoFrames;
    caller_video_frames_expected_ = 0;
  }

  // Callee's audio functions.
  void CalleeExpectsSomeAudio(
      int expected_audio_frames = kDefaultExpectedAudioFrameCount) {
    callee_audio_expectation_ = kExpectSomeFrames;
    callee_audio_frames_expected_ = expected_audio_frames;
  }

  void CalleeExpectsNoAudio() {
    callee_audio_expectation_ = kExpectNoFrames;
    callee_audio_frames_expected_ = 0;
  }

  // Callee's video functions.
  void CalleeExpectsSomeVideo(
      int expected_video_frames = kDefaultExpectedVideoFrameCount) {
    callee_video_expectation_ = kExpectSomeFrames;
    callee_video_frames_expected_ = expected_video_frames;
  }

  void CalleeExpectsNoVideo() {
    callee_video_expectation_ = kExpectNoFrames;
    callee_video_frames_expected_ = 0;
  }

  ExpectFrames caller_audio_expectation_ = kNoExpectation;
  ExpectFrames caller_video_expectation_ = kNoExpectation;
  ExpectFrames callee_audio_expectation_ = kNoExpectation;
  ExpectFrames callee_video_expectation_ = kNoExpectation;
  int caller_audio_frames_expected_ = 0;
  int caller_video_frames_expected_ = 0;
  int callee_audio_frames_expected_ = 0;
  int callee_video_frames_expected_ = 0;
};

class MockIceTransport : public webrtc::IceTransportInterface {
 public:
  MockIceTransport(const std::string& name, int component)
      : internal_(std::make_unique<cricket::FakeIceTransport>(
            name,
            component,
            nullptr /* network_thread */)) {}
  ~MockIceTransport() = default;
  cricket::IceTransportInternal* internal() { return internal_.get(); }

 private:
  std::unique_ptr<cricket::FakeIceTransport> internal_;
};

class MockIceTransportFactory : public IceTransportFactory {
 public:
  ~MockIceTransportFactory() override = default;
  rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      int component,
      IceTransportInit init) {
    RecordIceTransportCreated();
    return rtc::make_ref_counted<MockIceTransport>(transport_name, component);
  }
  MOCK_METHOD(void, RecordIceTransportCreated, ());
};

// Tests two PeerConnections connecting to each other end-to-end, using a
// virtual network, fake A/V capture and fake encoder/decoders. The
// PeerConnections share the threads/socket servers, but use separate versions
// of everything else (including "PeerConnectionFactory"s).
class PeerConnectionIntegrationBaseTest : public ::testing::Test {
 public:
  PeerConnectionIntegrationBaseTest(
      SdpSemantics sdp_semantics,
      absl::optional<std::string> field_trials = absl::nullopt)
      : sdp_semantics_(sdp_semantics),
        ss_(new rtc::VirtualSocketServer()),
        fss_(new rtc::FirewallSocketServer(ss_.get())),
        network_thread_(new rtc::Thread(fss_.get())),
        worker_thread_(rtc::Thread::Create()),
        field_trials_(field_trials.has_value()
                          ? new test::ScopedFieldTrials(*field_trials)
                          : nullptr) {
    network_thread_->SetName("PCNetworkThread", this);
    worker_thread_->SetName("PCWorkerThread", this);
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());
    webrtc::metrics::Reset();
  }

  ~PeerConnectionIntegrationBaseTest() {
    // The PeerConnections should be deleted before the TurnCustomizers.
    // A TurnPort is created with a raw pointer to a TurnCustomizer. The
    // TurnPort has the same lifetime as the PeerConnection, so it's expected
    // that the TurnCustomizer outlives the life of the PeerConnection or else
    // when Send() is called it will hit a seg fault.
    if (caller_) {
      caller_->set_signaling_message_receiver(nullptr);
      caller_->pc()->Close();
      delete SetCallerPcWrapperAndReturnCurrent(nullptr);
    }
    if (callee_) {
      callee_->set_signaling_message_receiver(nullptr);
      callee_->pc()->Close();
      delete SetCalleePcWrapperAndReturnCurrent(nullptr);
    }

    // If turn servers were created for the test they need to be destroyed on
    // the network thread.
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
      turn_servers_.clear();
      turn_customizers_.clear();
    });
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

  // When `event_log_factory` is null, the default implementation of the event
  // log factory will be used.
  std::unique_ptr<PeerConnectionIntegrationWrapper> CreatePeerConnectionWrapper(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies,
      std::unique_ptr<webrtc::FakeRtcEventLogFactory> event_log_factory,
      bool reset_encoder_factory,
      bool reset_decoder_factory) {
    RTCConfiguration modified_config;
    if (config) {
      modified_config = *config;
    }
    modified_config.sdp_semantics = sdp_semantics_;
    if (!dependencies.cert_generator) {
      dependencies.cert_generator =
          std::make_unique<FakeRTCCertificateGenerator>();
    }
    std::unique_ptr<PeerConnectionIntegrationWrapper> client(
        new PeerConnectionIntegrationWrapper(debug_name));

    if (!client->Init(options, &modified_config, std::move(dependencies),
                      network_thread_.get(), worker_thread_.get(),
                      std::move(event_log_factory), reset_encoder_factory,
                      reset_decoder_factory)) {
      return nullptr;
    }
    return client;
  }

  std::unique_ptr<PeerConnectionIntegrationWrapper>
  CreatePeerConnectionWrapperWithFakeRtcEventLog(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      webrtc::PeerConnectionDependencies dependencies) {
    return CreatePeerConnectionWrapper(
        debug_name, options, config, std::move(dependencies),
        std::make_unique<webrtc::FakeRtcEventLogFactory>(),
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
  }

  bool CreatePeerConnectionWrappers() {
    return CreatePeerConnectionWrappersWithConfig(
        PeerConnectionInterface::RTCConfiguration(),
        PeerConnectionInterface::RTCConfiguration());
  }

  bool CreatePeerConnectionWrappersWithSdpSemantics(
      SdpSemantics caller_semantics,
      SdpSemantics callee_semantics) {
    // Can't specify the sdp_semantics in the passed-in configuration since it
    // will be overwritten by CreatePeerConnectionWrapper with whatever is
    // stored in sdp_semantics_. So get around this by modifying the instance
    // variable before calling CreatePeerConnectionWrapper for the caller and
    // callee PeerConnections.
    SdpSemantics original_semantics = sdp_semantics_;
    sdp_semantics_ = caller_semantics;
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, nullptr, webrtc::PeerConnectionDependencies(nullptr),
        nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
    sdp_semantics_ = callee_semantics;
    callee_ = CreatePeerConnectionWrapper(
        "Callee", nullptr, nullptr, webrtc::PeerConnectionDependencies(nullptr),
        nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
    sdp_semantics_ = original_semantics;
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfig(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      const PeerConnectionInterface::RTCConfiguration& callee_config) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, &caller_config,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
    callee_ = CreatePeerConnectionWrapper(
        "Callee", nullptr, &callee_config,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfigAndDeps(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      webrtc::PeerConnectionDependencies caller_dependencies,
      const PeerConnectionInterface::RTCConfiguration& callee_config,
      webrtc::PeerConnectionDependencies callee_dependencies) {
    caller_ =
        CreatePeerConnectionWrapper("Caller", nullptr, &caller_config,
                                    std::move(caller_dependencies), nullptr,
                                    /*reset_encoder_factory=*/false,
                                    /*reset_decoder_factory=*/false);
    callee_ =
        CreatePeerConnectionWrapper("Callee", nullptr, &callee_config,
                                    std::move(callee_dependencies), nullptr,
                                    /*reset_encoder_factory=*/false,
                                    /*reset_decoder_factory=*/false);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithOptions(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", &caller_options, nullptr,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
    callee_ = CreatePeerConnectionWrapper(
        "Callee", &callee_options, nullptr,
        webrtc::PeerConnectionDependencies(nullptr), nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithFakeRtcEventLog() {
    PeerConnectionInterface::RTCConfiguration default_config;
    caller_ = CreatePeerConnectionWrapperWithFakeRtcEventLog(
        "Caller", nullptr, &default_config,
        webrtc::PeerConnectionDependencies(nullptr));
    callee_ = CreatePeerConnectionWrapperWithFakeRtcEventLog(
        "Callee", nullptr, &default_config,
        webrtc::PeerConnectionDependencies(nullptr));
    return caller_ && callee_;
  }

  std::unique_ptr<PeerConnectionIntegrationWrapper>
  CreatePeerConnectionWrapperWithAlternateKey() {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    cert_generator->use_alternate_key();

    webrtc::PeerConnectionDependencies dependencies(nullptr);
    dependencies.cert_generator = std::move(cert_generator);
    return CreatePeerConnectionWrapper("New Peer", nullptr, nullptr,
                                       std::move(dependencies), nullptr,
                                       /*reset_encoder_factory=*/false,
                                       /*reset_decoder_factory=*/false);
  }

  bool CreateOneDirectionalPeerConnectionWrappers(bool caller_to_callee) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, nullptr, webrtc::PeerConnectionDependencies(nullptr),
        nullptr,
        /*reset_encoder_factory=*/!caller_to_callee,
        /*reset_decoder_factory=*/caller_to_callee);
    callee_ = CreatePeerConnectionWrapper(
        "Callee", nullptr, nullptr, webrtc::PeerConnectionDependencies(nullptr),
        nullptr,
        /*reset_encoder_factory=*/caller_to_callee,
        /*reset_decoder_factory=*/!caller_to_callee);
    return caller_ && callee_;
  }

  cricket::TestTurnServer* CreateTurnServer(
      rtc::SocketAddress internal_address,
      rtc::SocketAddress external_address,
      cricket::ProtocolType type = cricket::ProtocolType::PROTO_UDP,
      const std::string& common_name = "test turn server") {
    rtc::Thread* thread = network_thread();
    std::unique_ptr<cricket::TestTurnServer> turn_server =
        network_thread()->Invoke<std::unique_ptr<cricket::TestTurnServer>>(
            RTC_FROM_HERE,
            [thread, internal_address, external_address, type, common_name] {
              return std::make_unique<cricket::TestTurnServer>(
                  thread, internal_address, external_address, type,
                  /*ignore_bad_certs=*/true, common_name);
            });
    turn_servers_.push_back(std::move(turn_server));
    // Interactions with the turn server should be done on the network thread.
    return turn_servers_.back().get();
  }

  cricket::TestTurnCustomizer* CreateTurnCustomizer() {
    std::unique_ptr<cricket::TestTurnCustomizer> turn_customizer =
        network_thread()->Invoke<std::unique_ptr<cricket::TestTurnCustomizer>>(
            RTC_FROM_HERE,
            [] { return std::make_unique<cricket::TestTurnCustomizer>(); });
    turn_customizers_.push_back(std::move(turn_customizer));
    // Interactions with the turn customizer should be done on the network
    // thread.
    return turn_customizers_.back().get();
  }

  // Checks that the function counters for a TestTurnCustomizer are greater than
  // 0.
  void ExpectTurnCustomizerCountersIncremented(
      cricket::TestTurnCustomizer* turn_customizer) {
    unsigned int allow_channel_data_counter =
        network_thread()->Invoke<unsigned int>(
            RTC_FROM_HERE, [turn_customizer] {
              return turn_customizer->allow_channel_data_cnt_;
            });
    EXPECT_GT(allow_channel_data_counter, 0u);
    unsigned int modify_counter = network_thread()->Invoke<unsigned int>(
        RTC_FROM_HERE,
        [turn_customizer] { return turn_customizer->modify_cnt_; });
    EXPECT_GT(modify_counter, 0u);
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

  PeerConnectionIntegrationWrapper* caller() { return caller_.get(); }

  // Set the `caller_` to the `wrapper` passed in and return the
  // original `caller_`.
  PeerConnectionIntegrationWrapper* SetCallerPcWrapperAndReturnCurrent(
      PeerConnectionIntegrationWrapper* wrapper) {
    PeerConnectionIntegrationWrapper* old = caller_.release();
    caller_.reset(wrapper);
    return old;
  }

  PeerConnectionIntegrationWrapper* callee() { return callee_.get(); }

  // Set the `callee_` to the `wrapper` passed in and return the
  // original `callee_`.
  PeerConnectionIntegrationWrapper* SetCalleePcWrapperAndReturnCurrent(
      PeerConnectionIntegrationWrapper* wrapper) {
    PeerConnectionIntegrationWrapper* old = callee_.release();
    callee_.reset(wrapper);
    return old;
  }

  void SetPortAllocatorFlags(uint32_t caller_flags, uint32_t callee_flags) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, caller_flags] {
      caller()->port_allocator()->set_flags(caller_flags);
    });
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, callee_flags] {
      callee()->port_allocator()->set_flags(callee_flags);
    });
  }

  rtc::FirewallSocketServer* firewall() const { return fss_.get(); }

  // Expects the provided number of new frames to be received within
  // kMaxWaitForFramesMs. The new expected frames are specified in
  // `media_expectations`. Returns false if any of the expectations were
  // not met.
  bool ExpectNewFrames(const MediaExpectations& media_expectations) {
    // Make sure there are no bogus tracks confusing the issue.
    caller()->RemoveUnusedVideoRenderers();
    callee()->RemoveUnusedVideoRenderers();
    // First initialize the expected frame counts based upon the current
    // frame count.
    int total_caller_audio_frames_expected = caller()->audio_frames_received();
    if (media_expectations.caller_audio_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_caller_audio_frames_expected +=
          media_expectations.caller_audio_frames_expected_;
    }
    int total_caller_video_frames_expected =
        caller()->min_video_frames_received_per_track();
    if (media_expectations.caller_video_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_caller_video_frames_expected +=
          media_expectations.caller_video_frames_expected_;
    }
    int total_callee_audio_frames_expected = callee()->audio_frames_received();
    if (media_expectations.callee_audio_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_callee_audio_frames_expected +=
          media_expectations.callee_audio_frames_expected_;
    }
    int total_callee_video_frames_expected =
        callee()->min_video_frames_received_per_track();
    if (media_expectations.callee_video_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_callee_video_frames_expected +=
          media_expectations.callee_video_frames_expected_;
    }

    // Wait for the expected frames.
    EXPECT_TRUE_WAIT(caller()->audio_frames_received() >=
                             total_caller_audio_frames_expected &&
                         caller()->min_video_frames_received_per_track() >=
                             total_caller_video_frames_expected &&
                         callee()->audio_frames_received() >=
                             total_callee_audio_frames_expected &&
                         callee()->min_video_frames_received_per_track() >=
                             total_callee_video_frames_expected,
                     kMaxWaitForFramesMs);
    bool expectations_correct =
        caller()->audio_frames_received() >=
            total_caller_audio_frames_expected &&
        caller()->min_video_frames_received_per_track() >=
            total_caller_video_frames_expected &&
        callee()->audio_frames_received() >=
            total_callee_audio_frames_expected &&
        callee()->min_video_frames_received_per_track() >=
            total_callee_video_frames_expected;

    // After the combined wait, print out a more detailed message upon
    // failure.
    EXPECT_GE(caller()->audio_frames_received(),
              total_caller_audio_frames_expected);
    EXPECT_GE(caller()->min_video_frames_received_per_track(),
              total_caller_video_frames_expected);
    EXPECT_GE(callee()->audio_frames_received(),
              total_callee_audio_frames_expected);
    EXPECT_GE(callee()->min_video_frames_received_per_track(),
              total_callee_video_frames_expected);

    // We want to make sure nothing unexpected was received.
    if (media_expectations.caller_audio_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(caller()->audio_frames_received(),
                total_caller_audio_frames_expected);
      if (caller()->audio_frames_received() !=
          total_caller_audio_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.caller_video_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(caller()->min_video_frames_received_per_track(),
                total_caller_video_frames_expected);
      if (caller()->min_video_frames_received_per_track() !=
          total_caller_video_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.callee_audio_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(callee()->audio_frames_received(),
                total_callee_audio_frames_expected);
      if (callee()->audio_frames_received() !=
          total_callee_audio_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.callee_video_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(callee()->min_video_frames_received_per_track(),
                total_callee_video_frames_expected);
      if (callee()->min_video_frames_received_per_track() !=
          total_callee_video_frames_expected) {
        expectations_correct = false;
      }
    }
    return expectations_correct;
  }

  void ClosePeerConnections() {
    if (caller())
      caller()->pc()->Close();
    if (callee())
      callee()->pc()->Close();
  }

  void TestNegotiatedCipherSuite(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options,
      int expected_cipher_suite) {
    ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(caller_options,
                                                        callee_options));
    ConnectFakeSignaling();
    caller()->AddAudioVideoTracks();
    callee()->AddAudioVideoTracks();
    caller()->CreateAndSetAndSignalOffer();
    ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
    EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(expected_cipher_suite),
                   caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
    // TODO(bugs.webrtc.org/9456): Fix it.
    EXPECT_METRIC_EQ(1, webrtc::metrics::NumEvents(
                            "WebRTC.PeerConnection.SrtpCryptoSuite.Audio",
                            expected_cipher_suite));
  }

  void TestGcmNegotiationUsesCipherSuite(bool local_gcm_enabled,
                                         bool remote_gcm_enabled,
                                         bool aes_ctr_enabled,
                                         int expected_cipher_suite) {
    PeerConnectionFactory::Options caller_options;
    caller_options.crypto_options.srtp.enable_gcm_crypto_suites =
        local_gcm_enabled;
    caller_options.crypto_options.srtp.enable_aes128_sha1_80_crypto_cipher =
        aes_ctr_enabled;
    PeerConnectionFactory::Options callee_options;
    callee_options.crypto_options.srtp.enable_gcm_crypto_suites =
        remote_gcm_enabled;
    callee_options.crypto_options.srtp.enable_aes128_sha1_80_crypto_cipher =
        aes_ctr_enabled;
    TestNegotiatedCipherSuite(caller_options, callee_options,
                              expected_cipher_suite);
  }

 protected:
  SdpSemantics sdp_semantics_;

 private:
  // `ss_` is used by `network_thread_` so it must be destroyed later.
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  std::unique_ptr<rtc::FirewallSocketServer> fss_;
  // `network_thread_` and `worker_thread_` are used by both
  // `caller_` and `callee_` so they must be destroyed
  // later.
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  // The turn servers and turn customizers should be accessed & deleted on the
  // network thread to avoid a race with the socket read/write that occurs
  // on the network thread.
  std::vector<std::unique_ptr<cricket::TestTurnServer>> turn_servers_;
  std::vector<std::unique_ptr<cricket::TestTurnCustomizer>> turn_customizers_;
  std::unique_ptr<PeerConnectionIntegrationWrapper> caller_;
  std::unique_ptr<PeerConnectionIntegrationWrapper> callee_;
  std::unique_ptr<test::ScopedFieldTrials> field_trials_;
};

}  // namespace webrtc

#endif  // PC_TEST_INTEGRATION_TEST_HELPERS_H_
