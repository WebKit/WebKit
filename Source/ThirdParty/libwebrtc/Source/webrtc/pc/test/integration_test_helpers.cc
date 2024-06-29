/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/test/integration_test_helpers.h"

namespace webrtc {

PeerConnectionInterface::RTCOfferAnswerOptions IceRestartOfferAnswerOptions() {
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.ice_restart = true;
  return options;
}

void RemoveSsrcsAndMsids(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    content.media_description()->mutable_streams().clear();
  }
  desc->set_msid_signaling(0);
}

void RemoveSsrcsAndKeepMsids(cricket::SessionDescription* desc) {
  for (ContentInfo& content : desc->contents()) {
    std::string track_id;
    std::vector<std::string> stream_ids;
    if (!content.media_description()->streams().empty()) {
      const StreamParams& first_stream =
          content.media_description()->streams()[0];
      track_id = first_stream.id;
      stream_ids = first_stream.stream_ids();
    }
    content.media_description()->mutable_streams().clear();
    StreamParams new_stream;
    new_stream.id = track_id;
    new_stream.set_stream_ids(stream_ids);
    content.media_description()->AddStream(new_stream);
  }
}

int FindFirstMediaStatsIndexByKind(
    const std::string& kind,
    const std::vector<const RTCInboundRtpStreamStats*>& inbound_rtps) {
  for (size_t i = 0; i < inbound_rtps.size(); i++) {
    if (*inbound_rtps[i]->kind == kind) {
      return i;
    }
  }
  return -1;
}

void ReplaceFirstSsrc(StreamParams& stream, uint32_t ssrc) {
  stream.ssrcs[0] = ssrc;
  for (auto& group : stream.ssrc_groups) {
    group.ssrcs[0] = ssrc;
  }
}

TaskQueueMetronome::TaskQueueMetronome(TimeDelta tick_period)
    : tick_period_(tick_period) {}

TaskQueueMetronome::~TaskQueueMetronome() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}
void TaskQueueMetronome::RequestCallOnNextTick(
    absl::AnyInvocable<void() &&> callback) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  callbacks_.push_back(std::move(callback));
  // Only schedule a tick callback for the first `callback` addition.
  // Schedule on the current task queue to comply with RequestCallOnNextTick
  // requirements.
  if (callbacks_.size() == 1) {
    TaskQueueBase::Current()->PostDelayedTask(
        SafeTask(safety_.flag(),
                 [this] {
                   RTC_DCHECK_RUN_ON(&sequence_checker_);
                   std::vector<absl::AnyInvocable<void() &&>> callbacks;
                   callbacks_.swap(callbacks);
                   for (auto& callback : callbacks)
                     std::move(callback)();
                 }),
        tick_period_);
  }
}

TimeDelta TaskQueueMetronome::TickPeriod() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return tick_period_;
}

// Implementation of PeerConnectionIntegrationWrapper functions
void PeerConnectionIntegrationWrapper::StartWatchingDelayStats() {
  // Get the baseline numbers for audio_packets and audio_delay.
  auto received_stats = NewGetStats();
  auto rtp_stats =
      received_stats->GetStatsOfType<RTCInboundRtpStreamStats>()[0];
  ASSERT_TRUE(rtp_stats->relative_packet_arrival_delay.has_value());
  ASSERT_TRUE(rtp_stats->packets_received.has_value());
  rtp_stats_id_ = rtp_stats->id();
  audio_packets_stat_ = *rtp_stats->packets_received;
  audio_delay_stat_ = *rtp_stats->relative_packet_arrival_delay;
  audio_samples_stat_ = *rtp_stats->total_samples_received;
  audio_concealed_stat_ = *rtp_stats->concealed_samples;
}

void PeerConnectionIntegrationWrapper::UpdateDelayStats(std::string tag,
                                                        int desc_size) {
  auto report = NewGetStats();
  auto rtp_stats = report->GetAs<RTCInboundRtpStreamStats>(rtp_stats_id_);
  ASSERT_TRUE(rtp_stats);
  auto delta_packets = *rtp_stats->packets_received - audio_packets_stat_;
  auto delta_rpad =
      *rtp_stats->relative_packet_arrival_delay - audio_delay_stat_;
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
  auto delta_samples = *rtp_stats->total_samples_received - audio_samples_stat_;
  auto delta_concealed = *rtp_stats->concealed_samples - audio_concealed_stat_;
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
  // Nondebug: Linux32 Release at conceal rate 0.606597 (CI run)
  // Debug: linux_x86_dbg bot at conceal rate 0.854
  //        internal bot at conceal rate 0.967 (b/294020344)
  // TODO(https://crbug.com/webrtc/15393): Improve audio quality during
  // renegotiation so that we can reduce these thresholds, 99% is not even
  // close to the 20% deemed unacceptable above or the 0% that would be ideal.
  if (delta_samples > 0) {
#if !defined(NDEBUG)
    EXPECT_LT(1.0 * delta_concealed / delta_samples, 0.99)
        << "Concealed " << delta_concealed << " of " << delta_samples
        << " samples";
#else
    EXPECT_LT(1.0 * delta_concealed / delta_samples, 0.7)
        << "Concealed " << delta_concealed << " of " << delta_samples
        << " samples";
#endif
  }
  // Increment trailing counters
  audio_packets_stat_ = *rtp_stats->packets_received;
  audio_delay_stat_ = *rtp_stats->relative_packet_arrival_delay;
  audio_samples_stat_ = *rtp_stats->total_samples_received;
  audio_concealed_stat_ = *rtp_stats->concealed_samples;
}

bool PeerConnectionIntegrationWrapper::Init(
    const PeerConnectionFactory::Options* options,
    const PeerConnectionInterface::RTCConfiguration* config,
    PeerConnectionDependencies dependencies,
    rtc::SocketServer* socket_server,
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    std::unique_ptr<FakeRtcEventLogFactory> event_log_factory,
    bool reset_encoder_factory,
    bool reset_decoder_factory,
    bool create_media_engine) {
  // There's an error in this test code if Init ends up being called twice.
  RTC_DCHECK(!peer_connection_);
  RTC_DCHECK(!peer_connection_factory_);

  fake_network_manager_.reset(new rtc::FakeNetworkManager());
  fake_network_manager_->AddInterface(kDefaultLocalAddress);

  socket_factory_.reset(new rtc::BasicPacketSocketFactory(socket_server));

  std::unique_ptr<cricket::PortAllocator> port_allocator(
      new cricket::BasicPortAllocator(fake_network_manager_.get(),
                                      socket_factory_.get()));
  port_allocator_ = port_allocator.get();
  fake_audio_capture_module_ = FakeAudioCaptureModule::Create();
  if (!fake_audio_capture_module_) {
    return false;
  }
  rtc::Thread* const signaling_thread = rtc::Thread::Current();

  PeerConnectionFactoryDependencies pc_factory_dependencies;
  pc_factory_dependencies.network_thread = network_thread;
  pc_factory_dependencies.worker_thread = worker_thread;
  pc_factory_dependencies.signaling_thread = signaling_thread;
  pc_factory_dependencies.task_queue_factory = CreateDefaultTaskQueueFactory();
  pc_factory_dependencies.trials = std::make_unique<FieldTrialBasedConfig>();
  pc_factory_dependencies.decode_metronome =
      std::make_unique<TaskQueueMetronome>(TimeDelta::Millis(8));

  pc_factory_dependencies.adm = fake_audio_capture_module_;
  if (create_media_engine) {
    EnableMediaWithDefaults(pc_factory_dependencies);
  }

  if (reset_encoder_factory) {
    pc_factory_dependencies.video_encoder_factory.reset();
  }
  if (reset_decoder_factory) {
    pc_factory_dependencies.video_decoder_factory.reset();
  }

  if (!pc_factory_dependencies.audio_processing) {
    // If the standard Creation method for APM returns a null pointer, instead
    // use the builder for testing to create an APM object.
    pc_factory_dependencies.audio_processing =
        AudioProcessingBuilderForTesting().Create();
  }

  if (event_log_factory) {
    event_log_factory_ = event_log_factory.get();
    pc_factory_dependencies.event_log_factory = std::move(event_log_factory);
  } else {
    pc_factory_dependencies.event_log_factory =
        std::make_unique<RtcEventLogFactory>();
  }
  peer_connection_factory_ =
      CreateModularPeerConnectionFactory(std::move(pc_factory_dependencies));

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

}  // namespace webrtc
