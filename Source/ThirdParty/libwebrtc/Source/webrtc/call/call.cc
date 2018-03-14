/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "api/optional.h"
#include "audio/audio_receive_stream.h"
#include "audio/audio_send_stream.h"
#include "audio/audio_state.h"
#include "audio/time_interval.h"
#include "call/bitrate_allocator.h"
#include "call/call.h"
#include "call/flexfec_receive_stream_impl.h"
#include "call/rtp_stream_receiver_controller.h"
#include "call/rtp_transport_controller_send.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/rtp_rtcp/include/flexfec_receiver.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/cpu_info.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/rw_lock_wrapper.h"
#include "video/call_stats.h"
#include "video/send_delay_stats.h"
#include "video/stats_counter.h"
#include "video/video_receive_stream.h"
#include "video/video_send_stream.h"

namespace webrtc {

namespace {

// TODO(nisse): This really begs for a shared context struct.
bool UseSendSideBwe(const std::vector<RtpExtension>& extensions,
                    bool transport_cc) {
  if (!transport_cc)
    return false;
  for (const auto& extension : extensions) {
    if (extension.uri == RtpExtension::kTransportSequenceNumberUri)
      return true;
  }
  return false;
}

bool UseSendSideBwe(const VideoReceiveStream::Config& config) {
  return UseSendSideBwe(config.rtp.extensions, config.rtp.transport_cc);
}

bool UseSendSideBwe(const AudioReceiveStream::Config& config) {
  return UseSendSideBwe(config.rtp.extensions, config.rtp.transport_cc);
}

bool UseSendSideBwe(const FlexfecReceiveStream::Config& config) {
  return UseSendSideBwe(config.rtp_header_extensions, config.transport_cc);
}

const int* FindKeyByValue(const std::map<int, int>& m, int v) {
  for (const auto& kv : m) {
    if (kv.second == v)
      return &kv.first;
  }
  return nullptr;
}

std::unique_ptr<rtclog::StreamConfig> CreateRtcLogStreamConfig(
    const VideoReceiveStream::Config& config) {
  auto rtclog_config = rtc::MakeUnique<rtclog::StreamConfig>();
  rtclog_config->remote_ssrc = config.rtp.remote_ssrc;
  rtclog_config->local_ssrc = config.rtp.local_ssrc;
  rtclog_config->rtx_ssrc = config.rtp.rtx_ssrc;
  rtclog_config->rtcp_mode = config.rtp.rtcp_mode;
  rtclog_config->remb = config.rtp.remb;
  rtclog_config->rtp_extensions = config.rtp.extensions;

  for (const auto& d : config.decoders) {
    const int* search =
        FindKeyByValue(config.rtp.rtx_associated_payload_types, d.payload_type);
    rtclog_config->codecs.emplace_back(d.payload_name, d.payload_type,
                                      search ? *search : 0);
  }
  return rtclog_config;
}

std::unique_ptr<rtclog::StreamConfig> CreateRtcLogStreamConfig(
    const VideoSendStream::Config& config,
    size_t ssrc_index) {
  auto rtclog_config = rtc::MakeUnique<rtclog::StreamConfig>();
  rtclog_config->local_ssrc = config.rtp.ssrcs[ssrc_index];
  if (ssrc_index < config.rtp.rtx.ssrcs.size()) {
    rtclog_config->rtx_ssrc = config.rtp.rtx.ssrcs[ssrc_index];
  }
  rtclog_config->rtcp_mode = config.rtp.rtcp_mode;
  rtclog_config->rtp_extensions = config.rtp.extensions;

  rtclog_config->codecs.emplace_back(config.encoder_settings.payload_name,
                                     config.encoder_settings.payload_type,
                                     config.rtp.rtx.payload_type);
  return rtclog_config;
}

std::unique_ptr<rtclog::StreamConfig> CreateRtcLogStreamConfig(
    const AudioReceiveStream::Config& config) {
  auto rtclog_config = rtc::MakeUnique<rtclog::StreamConfig>();
  rtclog_config->remote_ssrc = config.rtp.remote_ssrc;
  rtclog_config->local_ssrc = config.rtp.local_ssrc;
  rtclog_config->rtp_extensions = config.rtp.extensions;
  return rtclog_config;
}

std::unique_ptr<rtclog::StreamConfig> CreateRtcLogStreamConfig(
    const AudioSendStream::Config& config) {
  auto rtclog_config = rtc::MakeUnique<rtclog::StreamConfig>();
  rtclog_config->local_ssrc = config.rtp.ssrc;
  rtclog_config->rtp_extensions = config.rtp.extensions;
  if (config.send_codec_spec) {
    rtclog_config->codecs.emplace_back(config.send_codec_spec->format.name,
                                       config.send_codec_spec->payload_type, 0);
  }
  return rtclog_config;
}

}  // namespace

namespace internal {

class Call : public webrtc::Call,
             public PacketReceiver,
             public RecoveredPacketReceiver,
             public SendSideCongestionController::Observer,
             public BitrateAllocator::LimitObserver {
 public:
  Call(const Call::Config& config,
       std::unique_ptr<RtpTransportControllerSendInterface> transport_send);
  virtual ~Call();

  // Implements webrtc::Call.
  PacketReceiver* Receiver() override;

  webrtc::AudioSendStream* CreateAudioSendStream(
      const webrtc::AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) override;

  webrtc::AudioReceiveStream* CreateAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(
      webrtc::AudioReceiveStream* receive_stream) override;

  webrtc::VideoSendStream* CreateVideoSendStream(
      webrtc::VideoSendStream::Config config,
      VideoEncoderConfig encoder_config) override;
  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      webrtc::VideoReceiveStream::Config configuration) override;
  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;

  FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const FlexfecReceiveStream::Config& config) override;
  void DestroyFlexfecReceiveStream(
      FlexfecReceiveStream* receive_stream) override;

  Stats GetStats() const override;

  // Implements PacketReceiver.
  DeliveryStatus DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               const PacketTime& packet_time) override;

  // Implements RecoveredPacketReceiver.
  void OnRecoveredPacket(const uint8_t* packet, size_t length) override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;

  void SetBitrateConfigMask(
      const webrtc::Call::Config::BitrateConfigMask& bitrate_config) override;

  void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>
          bitrate_allocation_strategy) override;

  void SignalChannelNetworkState(MediaType media, NetworkState state) override;

  void OnTransportOverheadChanged(MediaType media,
                                  int transport_overhead_per_packet) override;

  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override;

  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt_ms,
                        int64_t probing_interval_ms) override;

  // Implements BitrateAllocator::LimitObserver.
  void OnAllocationLimitsChanged(uint32_t min_send_bitrate_bps,
                                 uint32_t max_padding_bitrate_bps) override;

 private:
  DeliveryStatus DeliverRtcp(MediaType media_type, const uint8_t* packet,
                             size_t length);
  DeliveryStatus DeliverRtp(MediaType media_type,
                            rtc::CopyOnWriteBuffer packet,
                            const PacketTime& packet_time);
  void ConfigureSync(const std::string& sync_group)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(receive_crit_);

  void NotifyBweOfReceivedPacket(const RtpPacketReceived& packet,
                                 MediaType media_type)
      RTC_SHARED_LOCKS_REQUIRED(receive_crit_);

  void UpdateSendHistograms(int64_t first_sent_packet_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&bitrate_crit_);
  void UpdateReceiveHistograms();
  void UpdateHistograms();
  void UpdateAggregateNetworkState();

  // Applies update to the BitrateConfig cached in |config_|, restarting
  // bandwidth estimation from |new_start| if set.
  void UpdateCurrentBitrateConfig(const rtc::Optional<int>& new_start);

  Clock* const clock_;

  const int num_cpu_cores_;
  const std::unique_ptr<ProcessThread> module_process_thread_;
  const std::unique_ptr<ProcessThread> pacer_thread_;
  const std::unique_ptr<CallStats> call_stats_;
  const std::unique_ptr<BitrateAllocator> bitrate_allocator_;
  Call::Config config_;
  rtc::SequencedTaskChecker configuration_sequence_checker_;

  NetworkState audio_network_state_;
  NetworkState video_network_state_;

  std::unique_ptr<RWLockWrapper> receive_crit_;
  // Audio, Video, and FlexFEC receive streams are owned by the client that
  // creates them.
  std::set<AudioReceiveStream*> audio_receive_streams_
      RTC_GUARDED_BY(receive_crit_);
  std::set<VideoReceiveStream*> video_receive_streams_
      RTC_GUARDED_BY(receive_crit_);

  std::map<std::string, AudioReceiveStream*> sync_stream_mapping_
      RTC_GUARDED_BY(receive_crit_);

  // TODO(nisse): Should eventually be injected at creation,
  // with a single object in the bundled case.
  RtpStreamReceiverController audio_receiver_controller_;
  RtpStreamReceiverController video_receiver_controller_;

  // This extra map is used for receive processing which is
  // independent of media type.

  // TODO(nisse): In the RTP transport refactoring, we should have a
  // single mapping from ssrc to a more abstract receive stream, with
  // accessor methods for all configuration we need at this level.
  struct ReceiveRtpConfig {
    ReceiveRtpConfig() = default;  // Needed by std::map
    ReceiveRtpConfig(const std::vector<RtpExtension>& extensions,
                     bool use_send_side_bwe)
        : extensions(extensions), use_send_side_bwe(use_send_side_bwe) {}

    // Registered RTP header extensions for each stream. Note that RTP header
    // extensions are negotiated per track ("m= line") in the SDP, but we have
    // no notion of tracks at the Call level. We therefore store the RTP header
    // extensions per SSRC instead, which leads to some storage overhead.
    RtpHeaderExtensionMap extensions;
    // Set if both RTP extension the RTCP feedback message needed for
    // send side BWE are negotiated.
    bool use_send_side_bwe = false;
  };
  std::map<uint32_t, ReceiveRtpConfig> receive_rtp_config_
      RTC_GUARDED_BY(receive_crit_);

  std::unique_ptr<RWLockWrapper> send_crit_;
  // Audio and Video send streams are owned by the client that creates them.
  std::map<uint32_t, AudioSendStream*> audio_send_ssrcs_
      RTC_GUARDED_BY(send_crit_);
  std::map<uint32_t, VideoSendStream*> video_send_ssrcs_
      RTC_GUARDED_BY(send_crit_);
  std::set<VideoSendStream*> video_send_streams_ RTC_GUARDED_BY(send_crit_);

  using RtpStateMap = std::map<uint32_t, RtpState>;
  RtpStateMap suspended_audio_send_ssrcs_
      RTC_GUARDED_BY(configuration_sequence_checker_);
  RtpStateMap suspended_video_send_ssrcs_
      RTC_GUARDED_BY(configuration_sequence_checker_);

  using RtpPayloadStateMap = std::map<uint32_t, RtpPayloadState>;
  RtpPayloadStateMap suspended_video_payload_states_
      RTC_GUARDED_BY(configuration_sequence_checker_);

  webrtc::RtcEventLog* event_log_;

  // The following members are only accessed (exclusively) from one thread and
  // from the destructor, and therefore doesn't need any explicit
  // synchronization.
  RateCounter received_bytes_per_second_counter_;
  RateCounter received_audio_bytes_per_second_counter_;
  RateCounter received_video_bytes_per_second_counter_;
  RateCounter received_rtcp_bytes_per_second_counter_;
  rtc::Optional<int64_t> first_received_rtp_audio_ms_;
  rtc::Optional<int64_t> last_received_rtp_audio_ms_;
  rtc::Optional<int64_t> first_received_rtp_video_ms_;
  rtc::Optional<int64_t> last_received_rtp_video_ms_;
  TimeInterval sent_rtp_audio_timer_ms_;

  // TODO(holmer): Remove this lock once BitrateController no longer calls
  // OnNetworkChanged from multiple threads.
  rtc::CriticalSection bitrate_crit_;
  uint32_t min_allocated_send_bitrate_bps_ RTC_GUARDED_BY(&bitrate_crit_);
  uint32_t configured_max_padding_bitrate_bps_ RTC_GUARDED_BY(&bitrate_crit_);
  AvgCounter estimated_send_bitrate_kbps_counter_
      RTC_GUARDED_BY(&bitrate_crit_);
  AvgCounter pacer_bitrate_kbps_counter_ RTC_GUARDED_BY(&bitrate_crit_);

  std::map<std::string, rtc::NetworkRoute> network_routes_;

  std::unique_ptr<RtpTransportControllerSendInterface> transport_send_;
  ReceiveSideCongestionController receive_side_cc_;
  const std::unique_ptr<SendDelayStats> video_send_delay_stats_;
  const int64_t start_ms_;
  // TODO(perkj): |worker_queue_| is supposed to replace
  // |module_process_thread_|.
  // |worker_queue| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  rtc::TaskQueue worker_queue_;

  // The config mask set by SetBitrateConfigMask.
  // 0 <= min <= start <= max
  Config::BitrateConfigMask bitrate_config_mask_;

  // The config set by SetBitrateConfig.
  // min >= 0, start != 0, max == -1 || max > 0
  Config::BitrateConfig base_bitrate_config_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

std::string Call::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "Call stats: " << time_ms << ", {";
  ss << "send_bw_bps: " << send_bandwidth_bps << ", ";
  ss << "recv_bw_bps: " << recv_bandwidth_bps << ", ";
  ss << "max_pad_bps: " << max_padding_bitrate_bps << ", ";
  ss << "pacer_delay_ms: " << pacer_delay_ms << ", ";
  ss << "rtt_ms: " << rtt_ms;
  ss << '}';
  return ss.str();
}

Call* Call::Create(const Call::Config& config) {
  return new internal::Call(config,
                            rtc::MakeUnique<RtpTransportControllerSend>(
                                Clock::GetRealTimeClock(), config.event_log));
}

Call* Call::Create(
    const Call::Config& config,
    std::unique_ptr<RtpTransportControllerSendInterface> transport_send) {
  return new internal::Call(config, std::move(transport_send));
}

namespace internal {

Call::Call(const Call::Config& config,
           std::unique_ptr<RtpTransportControllerSendInterface> transport_send)
    : clock_(Clock::GetRealTimeClock()),
      num_cpu_cores_(CpuInfo::DetectNumberOfCores()),
      module_process_thread_(ProcessThread::Create("ModuleProcessThread")),
      pacer_thread_(ProcessThread::Create("PacerThread")),
      call_stats_(new CallStats(clock_)),
      bitrate_allocator_(new BitrateAllocator(this)),
      config_(config),
      audio_network_state_(kNetworkDown),
      video_network_state_(kNetworkDown),
      receive_crit_(RWLockWrapper::CreateRWLock()),
      send_crit_(RWLockWrapper::CreateRWLock()),
      event_log_(config.event_log),
      received_bytes_per_second_counter_(clock_, nullptr, true),
      received_audio_bytes_per_second_counter_(clock_, nullptr, true),
      received_video_bytes_per_second_counter_(clock_, nullptr, true),
      received_rtcp_bytes_per_second_counter_(clock_, nullptr, true),
      min_allocated_send_bitrate_bps_(0),
      configured_max_padding_bitrate_bps_(0),
      estimated_send_bitrate_kbps_counter_(clock_, nullptr, true),
      pacer_bitrate_kbps_counter_(clock_, nullptr, true),
      receive_side_cc_(clock_, transport_send->packet_router()),
      video_send_delay_stats_(new SendDelayStats(clock_)),
      start_ms_(clock_->TimeInMilliseconds()),
      worker_queue_("call_worker_queue"),
      base_bitrate_config_(config.bitrate_config) {
  RTC_DCHECK(config.event_log != nullptr);
  RTC_DCHECK_GE(config.bitrate_config.min_bitrate_bps, 0);
  RTC_DCHECK_GE(config.bitrate_config.start_bitrate_bps,
                config.bitrate_config.min_bitrate_bps);
  if (config.bitrate_config.max_bitrate_bps != -1) {
    RTC_DCHECK_GE(config.bitrate_config.max_bitrate_bps,
                  config.bitrate_config.start_bitrate_bps);
  }
  transport_send->send_side_cc()->RegisterNetworkObserver(this);
  transport_send_ = std::move(transport_send);
  transport_send_->send_side_cc()->SignalNetworkState(kNetworkDown);
  transport_send_->send_side_cc()->SetBweBitrates(
      config_.bitrate_config.min_bitrate_bps,
      config_.bitrate_config.start_bitrate_bps,
      config_.bitrate_config.max_bitrate_bps);
  call_stats_->RegisterStatsObserver(&receive_side_cc_);
  call_stats_->RegisterStatsObserver(transport_send_->send_side_cc());

  // We have to attach the pacer to the pacer thread before starting the
  // module process thread to avoid a race accessing the process thread
  // both from the process thread and the pacer thread.
  pacer_thread_->RegisterModule(transport_send_->pacer(), RTC_FROM_HERE);
  pacer_thread_->RegisterModule(
      receive_side_cc_.GetRemoteBitrateEstimator(true), RTC_FROM_HERE);
  pacer_thread_->Start();

  module_process_thread_->RegisterModule(call_stats_.get(), RTC_FROM_HERE);
  module_process_thread_->RegisterModule(&receive_side_cc_, RTC_FROM_HERE);
  module_process_thread_->RegisterModule(transport_send_->send_side_cc(),
                                         RTC_FROM_HERE);
  module_process_thread_->Start();
}

Call::~Call() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  RTC_CHECK(audio_send_ssrcs_.empty());
  RTC_CHECK(video_send_ssrcs_.empty());
  RTC_CHECK(video_send_streams_.empty());
  RTC_CHECK(audio_receive_streams_.empty());
  RTC_CHECK(video_receive_streams_.empty());

  // The send-side congestion controller must be de-registered prior to
  // the pacer thread being stopped to avoid a race when accessing the
  // pacer thread object on the module process thread at the same time as
  // the pacer thread is stopped.
  module_process_thread_->DeRegisterModule(transport_send_->send_side_cc());
  pacer_thread_->Stop();
  pacer_thread_->DeRegisterModule(transport_send_->pacer());
  pacer_thread_->DeRegisterModule(
      receive_side_cc_.GetRemoteBitrateEstimator(true));
  module_process_thread_->DeRegisterModule(&receive_side_cc_);
  module_process_thread_->DeRegisterModule(call_stats_.get());
  module_process_thread_->Stop();
  call_stats_->DeregisterStatsObserver(&receive_side_cc_);
  call_stats_->DeregisterStatsObserver(transport_send_->send_side_cc());

  int64_t first_sent_packet_ms =
      transport_send_->send_side_cc()->GetFirstPacketTimeMs();
  // Only update histograms after process threads have been shut down, so that
  // they won't try to concurrently update stats.
  {
    rtc::CritScope lock(&bitrate_crit_);
    UpdateSendHistograms(first_sent_packet_ms);
  }
  UpdateReceiveHistograms();
  UpdateHistograms();
}

void Call::UpdateHistograms() {
  RTC_HISTOGRAM_COUNTS_100000(
      "WebRTC.Call.LifetimeInSeconds",
      (clock_->TimeInMilliseconds() - start_ms_) / 1000);
}

void Call::UpdateSendHistograms(int64_t first_sent_packet_ms) {
  if (first_sent_packet_ms == -1)
    return;
  if (!sent_rtp_audio_timer_ms_.Empty()) {
    RTC_HISTOGRAM_COUNTS_100000(
        "WebRTC.Call.TimeSendingAudioRtpPacketsInSeconds",
        sent_rtp_audio_timer_ms_.Length() / 1000);
  }
  int64_t elapsed_sec =
      (clock_->TimeInMilliseconds() - first_sent_packet_ms) / 1000;
  if (elapsed_sec < metrics::kMinRunTimeInSeconds)
    return;
  const int kMinRequiredPeriodicSamples = 5;
  AggregatedStats send_bitrate_stats =
      estimated_send_bitrate_kbps_counter_.ProcessAndGetStats();
  if (send_bitrate_stats.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.EstimatedSendBitrateInKbps",
                                send_bitrate_stats.average);
    RTC_LOG(LS_INFO) << "WebRTC.Call.EstimatedSendBitrateInKbps, "
                     << send_bitrate_stats.ToString();
  }
  AggregatedStats pacer_bitrate_stats =
      pacer_bitrate_kbps_counter_.ProcessAndGetStats();
  if (pacer_bitrate_stats.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.PacerBitrateInKbps",
                                pacer_bitrate_stats.average);
    RTC_LOG(LS_INFO) << "WebRTC.Call.PacerBitrateInKbps, "
                     << pacer_bitrate_stats.ToString();
  }
}

void Call::UpdateReceiveHistograms() {
  if (first_received_rtp_audio_ms_) {
    RTC_HISTOGRAM_COUNTS_100000(
        "WebRTC.Call.TimeReceivingAudioRtpPacketsInSeconds",
        (*last_received_rtp_audio_ms_ - *first_received_rtp_audio_ms_) / 1000);
  }
  if (first_received_rtp_video_ms_) {
    RTC_HISTOGRAM_COUNTS_100000(
        "WebRTC.Call.TimeReceivingVideoRtpPacketsInSeconds",
        (*last_received_rtp_video_ms_ - *first_received_rtp_video_ms_) / 1000);
  }
  const int kMinRequiredPeriodicSamples = 5;
  AggregatedStats video_bytes_per_sec =
      received_video_bytes_per_second_counter_.GetStats();
  if (video_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.VideoBitrateReceivedInKbps",
                                video_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << "WebRTC.Call.VideoBitrateReceivedInBps, "
                     << video_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats audio_bytes_per_sec =
      received_audio_bytes_per_second_counter_.GetStats();
  if (audio_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.AudioBitrateReceivedInKbps",
                                audio_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << "WebRTC.Call.AudioBitrateReceivedInBps, "
                     << audio_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats rtcp_bytes_per_sec =
      received_rtcp_bytes_per_second_counter_.GetStats();
  if (rtcp_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.RtcpBitrateReceivedInBps",
                                rtcp_bytes_per_sec.average * 8);
    RTC_LOG(LS_INFO) << "WebRTC.Call.RtcpBitrateReceivedInBps, "
                     << rtcp_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats recv_bytes_per_sec =
      received_bytes_per_second_counter_.GetStats();
  if (recv_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.BitrateReceivedInKbps",
                                recv_bytes_per_sec.average * 8 / 1000);
    RTC_LOG(LS_INFO) << "WebRTC.Call.BitrateReceivedInBps, "
                     << recv_bytes_per_sec.ToStringWithMultiplier(8);
  }
}

PacketReceiver* Call::Receiver() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  return this;
}

webrtc::AudioSendStream* Call::CreateAudioSendStream(
    const webrtc::AudioSendStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioSendStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  event_log_->Log(rtc::MakeUnique<RtcEventAudioSendStreamConfig>(
      CreateRtcLogStreamConfig(config)));

  rtc::Optional<RtpState> suspended_rtp_state;
  {
    const auto& iter = suspended_audio_send_ssrcs_.find(config.rtp.ssrc);
    if (iter != suspended_audio_send_ssrcs_.end()) {
      suspended_rtp_state.emplace(iter->second);
    }
  }

  AudioSendStream* send_stream = new AudioSendStream(
      config, config_.audio_state, &worker_queue_, module_process_thread_.get(),
      transport_send_.get(), bitrate_allocator_.get(), event_log_,
      call_stats_->rtcp_rtt_stats(), suspended_rtp_state,
      &sent_rtp_audio_timer_ms_);
  {
    WriteLockScoped write_lock(*send_crit_);
    RTC_DCHECK(audio_send_ssrcs_.find(config.rtp.ssrc) ==
               audio_send_ssrcs_.end());
    audio_send_ssrcs_[config.rtp.ssrc] = send_stream;
  }
  {
    ReadLockScoped read_lock(*receive_crit_);
    for (AudioReceiveStream* stream : audio_receive_streams_) {
      if (stream->config().rtp.local_ssrc == config.rtp.ssrc) {
        stream->AssociateSendStream(send_stream);
      }
    }
  }
  send_stream->SignalNetworkState(audio_network_state_);
  UpdateAggregateNetworkState();
  return send_stream;
}

void Call::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioSendStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  RTC_DCHECK(send_stream != nullptr);

  send_stream->Stop();

  const uint32_t ssrc = send_stream->GetConfig().rtp.ssrc;
  webrtc::internal::AudioSendStream* audio_send_stream =
      static_cast<webrtc::internal::AudioSendStream*>(send_stream);
  suspended_audio_send_ssrcs_[ssrc] = audio_send_stream->GetRtpState();
  {
    WriteLockScoped write_lock(*send_crit_);
    size_t num_deleted = audio_send_ssrcs_.erase(ssrc);
    RTC_DCHECK_EQ(1, num_deleted);
  }
  {
    ReadLockScoped read_lock(*receive_crit_);
    for (AudioReceiveStream* stream : audio_receive_streams_) {
      if (stream->config().rtp.local_ssrc == ssrc) {
        stream->AssociateSendStream(nullptr);
      }
    }
  }
  UpdateAggregateNetworkState();
  delete send_stream;
}

webrtc::AudioReceiveStream* Call::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioReceiveStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  event_log_->Log(rtc::MakeUnique<RtcEventAudioReceiveStreamConfig>(
      CreateRtcLogStreamConfig(config)));
  AudioReceiveStream* receive_stream = new AudioReceiveStream(
      &audio_receiver_controller_, transport_send_->packet_router(),
      module_process_thread_.get(), config, config_.audio_state, event_log_);
  {
    WriteLockScoped write_lock(*receive_crit_);
    receive_rtp_config_[config.rtp.remote_ssrc] =
        ReceiveRtpConfig(config.rtp.extensions, UseSendSideBwe(config));
    audio_receive_streams_.insert(receive_stream);

    ConfigureSync(config.sync_group);
  }
  {
    ReadLockScoped read_lock(*send_crit_);
    auto it = audio_send_ssrcs_.find(config.rtp.local_ssrc);
    if (it != audio_send_ssrcs_.end()) {
      receive_stream->AssociateSendStream(it->second);
    }
  }
  receive_stream->SignalNetworkState(audio_network_state_);
  UpdateAggregateNetworkState();
  return receive_stream;
}

void Call::DestroyAudioReceiveStream(
    webrtc::AudioReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioReceiveStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  RTC_DCHECK(receive_stream != nullptr);
  webrtc::internal::AudioReceiveStream* audio_receive_stream =
      static_cast<webrtc::internal::AudioReceiveStream*>(receive_stream);
  {
    WriteLockScoped write_lock(*receive_crit_);
    const AudioReceiveStream::Config& config = audio_receive_stream->config();
    uint32_t ssrc = config.rtp.remote_ssrc;
    receive_side_cc_.GetRemoteBitrateEstimator(UseSendSideBwe(config))
        ->RemoveStream(ssrc);
    audio_receive_streams_.erase(audio_receive_stream);
    const std::string& sync_group = audio_receive_stream->config().sync_group;
    const auto it = sync_stream_mapping_.find(sync_group);
    if (it != sync_stream_mapping_.end() &&
        it->second == audio_receive_stream) {
      sync_stream_mapping_.erase(it);
      ConfigureSync(sync_group);
    }
    receive_rtp_config_.erase(ssrc);
  }
  UpdateAggregateNetworkState();
  delete audio_receive_stream;
}

webrtc::VideoSendStream* Call::CreateVideoSendStream(
    webrtc::VideoSendStream::Config config,
    VideoEncoderConfig encoder_config) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoSendStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  video_send_delay_stats_->AddSsrcs(config);
  for (size_t ssrc_index = 0; ssrc_index < config.rtp.ssrcs.size();
       ++ssrc_index) {
    event_log_->Log(rtc::MakeUnique<RtcEventVideoSendStreamConfig>(
        CreateRtcLogStreamConfig(config, ssrc_index)));
  }

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  // Copy ssrcs from |config| since |config| is moved.
  std::vector<uint32_t> ssrcs = config.rtp.ssrcs;
  VideoSendStream* send_stream = new VideoSendStream(
      num_cpu_cores_, module_process_thread_.get(), &worker_queue_,
      call_stats_.get(), transport_send_.get(), bitrate_allocator_.get(),
      video_send_delay_stats_.get(), event_log_, std::move(config),
      std::move(encoder_config), suspended_video_send_ssrcs_,
      suspended_video_payload_states_);

  {
    WriteLockScoped write_lock(*send_crit_);
    for (uint32_t ssrc : ssrcs) {
      RTC_DCHECK(video_send_ssrcs_.find(ssrc) == video_send_ssrcs_.end());
      video_send_ssrcs_[ssrc] = send_stream;
    }
    video_send_streams_.insert(send_stream);
  }
  send_stream->SignalNetworkState(video_network_state_);
  UpdateAggregateNetworkState();

  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoSendStream");
  RTC_DCHECK(send_stream != nullptr);
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  send_stream->Stop();

  VideoSendStream* send_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*send_crit_);
    auto it = video_send_ssrcs_.begin();
    while (it != video_send_ssrcs_.end()) {
      if (it->second == static_cast<VideoSendStream*>(send_stream)) {
        send_stream_impl = it->second;
        video_send_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
    video_send_streams_.erase(send_stream_impl);
  }
  RTC_CHECK(send_stream_impl != nullptr);

  VideoSendStream::RtpStateMap rtp_states;
  VideoSendStream::RtpPayloadStateMap rtp_payload_states;
  send_stream_impl->StopPermanentlyAndGetRtpStates(&rtp_states,
                                                   &rtp_payload_states);
  for (const auto& kv : rtp_states) {
    suspended_video_send_ssrcs_[kv.first] = kv.second;
  }
  for (const auto& kv : rtp_payload_states) {
    suspended_video_payload_states_[kv.first] = kv.second;
  }

  UpdateAggregateNetworkState();
  delete send_stream_impl;
}

webrtc::VideoReceiveStream* Call::CreateVideoReceiveStream(
    webrtc::VideoReceiveStream::Config configuration) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoReceiveStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  VideoReceiveStream* receive_stream = new VideoReceiveStream(
      &video_receiver_controller_, num_cpu_cores_,
      transport_send_->packet_router(), std::move(configuration),
      module_process_thread_.get(), call_stats_.get());

  const webrtc::VideoReceiveStream::Config& config = receive_stream->config();
  ReceiveRtpConfig receive_config(config.rtp.extensions,
                                  UseSendSideBwe(config));
  {
    WriteLockScoped write_lock(*receive_crit_);
    if (config.rtp.rtx_ssrc) {
      // We record identical config for the rtx stream as for the main
      // stream. Since the transport_send_cc negotiation is per payload
      // type, we may get an incorrect value for the rtx stream, but
      // that is unlikely to matter in practice.
      receive_rtp_config_[config.rtp.rtx_ssrc] = receive_config;
    }
    receive_rtp_config_[config.rtp.remote_ssrc] = receive_config;
    video_receive_streams_.insert(receive_stream);
    ConfigureSync(config.sync_group);
  }
  receive_stream->SignalNetworkState(video_network_state_);
  UpdateAggregateNetworkState();
  event_log_->Log(rtc::MakeUnique<RtcEventVideoReceiveStreamConfig>(
      CreateRtcLogStreamConfig(config)));
  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoReceiveStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  RTC_DCHECK(receive_stream != nullptr);
  VideoReceiveStream* receive_stream_impl =
      static_cast<VideoReceiveStream*>(receive_stream);
  const VideoReceiveStream::Config& config = receive_stream_impl->config();
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    receive_rtp_config_.erase(config.rtp.remote_ssrc);
    if (config.rtp.rtx_ssrc) {
      receive_rtp_config_.erase(config.rtp.rtx_ssrc);
    }
    video_receive_streams_.erase(receive_stream_impl);
    ConfigureSync(config.sync_group);
  }

  receive_side_cc_.GetRemoteBitrateEstimator(UseSendSideBwe(config))
      ->RemoveStream(config.rtp.remote_ssrc);

  UpdateAggregateNetworkState();
  delete receive_stream_impl;
}

FlexfecReceiveStream* Call::CreateFlexfecReceiveStream(
    const FlexfecReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateFlexfecReceiveStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  RecoveredPacketReceiver* recovered_packet_receiver = this;

  FlexfecReceiveStreamImpl* receive_stream;
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Unlike the video and audio receive streams,
    // FlexfecReceiveStream implements RtpPacketSinkInterface itself,
    // and hence its constructor passes its |this| pointer to
    // video_receiver_controller_->CreateStream(). Calling the
    // constructor while holding |receive_crit_| ensures that we don't
    // call OnRtpPacket until the constructor is finished and the
    // object is in a valid state.
    // TODO(nisse): Fix constructor so that it can be moved outside of
    // this locked scope.
    receive_stream = new FlexfecReceiveStreamImpl(
        &video_receiver_controller_, config, recovered_packet_receiver,
        call_stats_->rtcp_rtt_stats(), module_process_thread_.get());

    RTC_DCHECK(receive_rtp_config_.find(config.remote_ssrc) ==
               receive_rtp_config_.end());
    receive_rtp_config_[config.remote_ssrc] =
        ReceiveRtpConfig(config.rtp_header_extensions, UseSendSideBwe(config));
  }

  // TODO(brandtr): Store config in RtcEventLog here.

  return receive_stream;
}

void Call::DestroyFlexfecReceiveStream(FlexfecReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyFlexfecReceiveStream");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  RTC_DCHECK(receive_stream != nullptr);
  {
    WriteLockScoped write_lock(*receive_crit_);

    const FlexfecReceiveStream::Config& config = receive_stream->GetConfig();
    uint32_t ssrc = config.remote_ssrc;
    receive_rtp_config_.erase(ssrc);

    // Remove all SSRCs pointing to the FlexfecReceiveStreamImpl to be
    // destroyed.
    receive_side_cc_.GetRemoteBitrateEstimator(UseSendSideBwe(config))
        ->RemoveStream(ssrc);
  }

  delete receive_stream;
}

Call::Stats Call::GetStats() const {
  // TODO(solenberg): Some test cases in EndToEndTest use this from a different
  // thread. Re-enable once that is fixed.
  // RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  Stats stats;
  // Fetch available send/receive bitrates.
  uint32_t send_bandwidth = 0;
  transport_send_->send_side_cc()->AvailableBandwidth(&send_bandwidth);
  std::vector<unsigned int> ssrcs;
  uint32_t recv_bandwidth = 0;
  receive_side_cc_.GetRemoteBitrateEstimator(false)->LatestEstimate(
      &ssrcs, &recv_bandwidth);
  stats.send_bandwidth_bps = send_bandwidth;
  stats.recv_bandwidth_bps = recv_bandwidth;
  stats.pacer_delay_ms =
      transport_send_->send_side_cc()->GetPacerQueuingDelayMs();
  stats.rtt_ms = call_stats_->rtcp_rtt_stats()->LastProcessedRtt();
  {
    rtc::CritScope cs(&bitrate_crit_);
    stats.max_padding_bitrate_bps = configured_max_padding_bitrate_bps_;
  }
  return stats;
}

void Call::SetBitrateConfig(
    const webrtc::Call::Config::BitrateConfig& bitrate_config) {
  TRACE_EVENT0("webrtc", "Call::SetBitrateConfig");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  RTC_DCHECK_GE(bitrate_config.min_bitrate_bps, 0);
  RTC_DCHECK_NE(bitrate_config.start_bitrate_bps, 0);
  if (bitrate_config.max_bitrate_bps != -1) {
    RTC_DCHECK_GT(bitrate_config.max_bitrate_bps, 0);
  }

  rtc::Optional<int> new_start;
  // Only update the "start" bitrate if it's set, and different from the old
  // value. In practice, this value comes from the x-google-start-bitrate codec
  // parameter in SDP, and setting the same remote description twice shouldn't
  // restart bandwidth estimation.
  if (bitrate_config.start_bitrate_bps != -1 &&
      bitrate_config.start_bitrate_bps !=
          base_bitrate_config_.start_bitrate_bps) {
    new_start.emplace(bitrate_config.start_bitrate_bps);
  }
  base_bitrate_config_ = bitrate_config;
  UpdateCurrentBitrateConfig(new_start);
}

void Call::SetBitrateConfigMask(
    const webrtc::Call::Config::BitrateConfigMask& mask) {
  TRACE_EVENT0("webrtc", "Call::SetBitrateConfigMask");
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  bitrate_config_mask_ = mask;
  UpdateCurrentBitrateConfig(mask.start_bitrate_bps);
}

void Call::UpdateCurrentBitrateConfig(const rtc::Optional<int>& new_start) {
  Config::BitrateConfig updated;
  updated.min_bitrate_bps =
      std::max(bitrate_config_mask_.min_bitrate_bps.value_or(0),
               base_bitrate_config_.min_bitrate_bps);

  updated.max_bitrate_bps =
      MinPositive(bitrate_config_mask_.max_bitrate_bps.value_or(-1),
                  base_bitrate_config_.max_bitrate_bps);

  // If the combined min ends up greater than the combined max, the max takes
  // priority.
  if (updated.max_bitrate_bps != -1 &&
      updated.min_bitrate_bps > updated.max_bitrate_bps) {
    updated.min_bitrate_bps = updated.max_bitrate_bps;
  }

  // If there is nothing to update (min/max unchanged, no new bandwidth
  // estimation start value), return early.
  if (updated.min_bitrate_bps == config_.bitrate_config.min_bitrate_bps &&
      updated.max_bitrate_bps == config_.bitrate_config.max_bitrate_bps &&
      !new_start) {
    RTC_LOG(LS_VERBOSE) << "WebRTC.Call.UpdateCurrentBitrateConfig: "
                        << "nothing to update";
    return;
  }

  if (new_start) {
    // Clamp start by min and max.
    updated.start_bitrate_bps = MinPositive(
        std::max(*new_start, updated.min_bitrate_bps), updated.max_bitrate_bps);
  } else {
    updated.start_bitrate_bps = -1;
  }

  RTC_LOG(INFO) << "WebRTC.Call.UpdateCurrentBitrateConfig: "
                << "calling SetBweBitrates with args ("
                << updated.min_bitrate_bps << ", " << updated.start_bitrate_bps
                << ", " << updated.max_bitrate_bps << ")";
  transport_send_->send_side_cc()->SetBweBitrates(updated.min_bitrate_bps,
                                                  updated.start_bitrate_bps,
                                                  updated.max_bitrate_bps);
  if (!new_start) {
    updated.start_bitrate_bps = config_.bitrate_config.start_bitrate_bps;
  }
  config_.bitrate_config = updated;
}

void Call::SetBitrateAllocationStrategy(
    std::unique_ptr<rtc::BitrateAllocationStrategy>
        bitrate_allocation_strategy) {
  if (!worker_queue_.IsCurrent()) {
    rtc::BitrateAllocationStrategy* strategy_raw =
        bitrate_allocation_strategy.release();
    auto functor = [this, strategy_raw]() {
      SetBitrateAllocationStrategy(
          rtc::WrapUnique<rtc::BitrateAllocationStrategy>(strategy_raw));
    };
    worker_queue_.PostTask([functor] { functor(); });
    return;
  }
  RTC_DCHECK_RUN_ON(&worker_queue_);
  bitrate_allocator_->SetBitrateAllocationStrategy(
      std::move(bitrate_allocation_strategy));
}

void Call::SignalChannelNetworkState(MediaType media, NetworkState state) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  switch (media) {
    case MediaType::AUDIO:
      audio_network_state_ = state;
      break;
    case MediaType::VIDEO:
      video_network_state_ = state;
      break;
    case MediaType::ANY:
    case MediaType::DATA:
      RTC_NOTREACHED();
      break;
  }

  UpdateAggregateNetworkState();
  {
    ReadLockScoped read_lock(*send_crit_);
    for (auto& kv : audio_send_ssrcs_) {
      kv.second->SignalNetworkState(audio_network_state_);
    }
    for (auto& kv : video_send_ssrcs_) {
      kv.second->SignalNetworkState(video_network_state_);
    }
  }
  {
    ReadLockScoped read_lock(*receive_crit_);
    for (AudioReceiveStream* audio_receive_stream : audio_receive_streams_) {
      audio_receive_stream->SignalNetworkState(audio_network_state_);
    }
    for (VideoReceiveStream* video_receive_stream : video_receive_streams_) {
      video_receive_stream->SignalNetworkState(video_network_state_);
    }
  }
}

void Call::OnTransportOverheadChanged(MediaType media,
                                      int transport_overhead_per_packet) {
  switch (media) {
    case MediaType::AUDIO: {
      ReadLockScoped read_lock(*send_crit_);
      for (auto& kv : audio_send_ssrcs_) {
        kv.second->SetTransportOverhead(transport_overhead_per_packet);
      }
      break;
    }
    case MediaType::VIDEO: {
      ReadLockScoped read_lock(*send_crit_);
      for (auto& kv : video_send_ssrcs_) {
        kv.second->SetTransportOverhead(transport_overhead_per_packet);
      }
      break;
    }
    case MediaType::ANY:
    case MediaType::DATA:
      RTC_NOTREACHED();
      break;
  }
}

// TODO(honghaiz): Add tests for this method.
void Call::OnNetworkRouteChanged(const std::string& transport_name,
                                 const rtc::NetworkRoute& network_route) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  // Check if the network route is connected.
  if (!network_route.connected) {
    RTC_LOG(LS_INFO) << "Transport " << transport_name << " is disconnected";
    // TODO(honghaiz): Perhaps handle this in SignalChannelNetworkState and
    // consider merging these two methods.
    return;
  }

  // Check whether the network route has changed on each transport.
  auto result =
      network_routes_.insert(std::make_pair(transport_name, network_route));
  auto kv = result.first;
  bool inserted = result.second;
  if (inserted) {
    // No need to reset BWE if this is the first time the network connects.
    return;
  }
  if (kv->second != network_route) {
    kv->second = network_route;
    RTC_LOG(LS_INFO)
        << "Network route changed on transport " << transport_name
        << ": new local network id " << network_route.local_network_id
        << " new remote network id " << network_route.remote_network_id
        << " Reset bitrates to min: " << config_.bitrate_config.min_bitrate_bps
        << " bps, start: " << config_.bitrate_config.start_bitrate_bps
        << " bps,  max: " << config_.bitrate_config.start_bitrate_bps
        << " bps.";
    RTC_DCHECK_GT(config_.bitrate_config.start_bitrate_bps, 0);
    transport_send_->send_side_cc()->OnNetworkRouteChanged(
        network_route, config_.bitrate_config.start_bitrate_bps,
        config_.bitrate_config.min_bitrate_bps,
        config_.bitrate_config.max_bitrate_bps);
  }
}

void Call::UpdateAggregateNetworkState() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);

  bool have_audio = false;
  bool have_video = false;
  {
    ReadLockScoped read_lock(*send_crit_);
    if (audio_send_ssrcs_.size() > 0)
      have_audio = true;
    if (video_send_ssrcs_.size() > 0)
      have_video = true;
  }
  {
    ReadLockScoped read_lock(*receive_crit_);
    if (audio_receive_streams_.size() > 0)
      have_audio = true;
    if (video_receive_streams_.size() > 0)
      have_video = true;
  }

  NetworkState aggregate_state = kNetworkDown;
  if ((have_video && video_network_state_ == kNetworkUp) ||
      (have_audio && audio_network_state_ == kNetworkUp)) {
    aggregate_state = kNetworkUp;
  }

  RTC_LOG(LS_INFO) << "UpdateAggregateNetworkState: aggregate_state="
                   << (aggregate_state == kNetworkUp ? "up" : "down");

  transport_send_->send_side_cc()->SignalNetworkState(aggregate_state);
}

void Call::OnSentPacket(const rtc::SentPacket& sent_packet) {
  video_send_delay_stats_->OnSentPacket(sent_packet.packet_id,
                                        clock_->TimeInMilliseconds());
  transport_send_->send_side_cc()->OnSentPacket(sent_packet);
}

void Call::OnNetworkChanged(uint32_t target_bitrate_bps,
                            uint8_t fraction_loss,
                            int64_t rtt_ms,
                            int64_t probing_interval_ms) {
  // TODO(perkj): Consider making sure CongestionController operates on
  // |worker_queue_|.
  if (!worker_queue_.IsCurrent()) {
    worker_queue_.PostTask(
        [this, target_bitrate_bps, fraction_loss, rtt_ms, probing_interval_ms] {
          OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt_ms,
                           probing_interval_ms);
        });
    return;
  }
  RTC_DCHECK_RUN_ON(&worker_queue_);
  // For controlling the rate of feedback messages.
  receive_side_cc_.OnBitrateChanged(target_bitrate_bps);
  bitrate_allocator_->OnNetworkChanged(target_bitrate_bps, fraction_loss,
                                       rtt_ms, probing_interval_ms);

  // Ignore updates if bitrate is zero (the aggregate network state is down).
  if (target_bitrate_bps == 0) {
    rtc::CritScope lock(&bitrate_crit_);
    estimated_send_bitrate_kbps_counter_.ProcessAndPause();
    pacer_bitrate_kbps_counter_.ProcessAndPause();
    return;
  }

  bool sending_video;
  {
    ReadLockScoped read_lock(*send_crit_);
    sending_video = !video_send_streams_.empty();
  }

  rtc::CritScope lock(&bitrate_crit_);
  if (!sending_video) {
    // Do not update the stats if we are not sending video.
    estimated_send_bitrate_kbps_counter_.ProcessAndPause();
    pacer_bitrate_kbps_counter_.ProcessAndPause();
    return;
  }
  estimated_send_bitrate_kbps_counter_.Add(target_bitrate_bps / 1000);
  // Pacer bitrate may be higher than bitrate estimate if enforcing min bitrate.
  uint32_t pacer_bitrate_bps =
      std::max(target_bitrate_bps, min_allocated_send_bitrate_bps_);
  pacer_bitrate_kbps_counter_.Add(pacer_bitrate_bps / 1000);
}

void Call::OnAllocationLimitsChanged(uint32_t min_send_bitrate_bps,
                                     uint32_t max_padding_bitrate_bps) {
  transport_send_->SetAllocatedSendBitrateLimits(min_send_bitrate_bps,
                                                 max_padding_bitrate_bps);
  rtc::CritScope lock(&bitrate_crit_);
  min_allocated_send_bitrate_bps_ = min_send_bitrate_bps;
  configured_max_padding_bitrate_bps_ = max_padding_bitrate_bps;
}

void Call::ConfigureSync(const std::string& sync_group) {
  // Set sync only if there was no previous one.
  if (sync_group.empty())
    return;

  AudioReceiveStream* sync_audio_stream = nullptr;
  // Find existing audio stream.
  const auto it = sync_stream_mapping_.find(sync_group);
  if (it != sync_stream_mapping_.end()) {
    sync_audio_stream = it->second;
  } else {
    // No configured audio stream, see if we can find one.
    for (AudioReceiveStream* stream : audio_receive_streams_) {
      if (stream->config().sync_group == sync_group) {
        if (sync_audio_stream != nullptr) {
          RTC_LOG(LS_WARNING)
              << "Attempting to sync more than one audio stream "
                 "within the same sync group. This is not "
                 "supported in the current implementation.";
          break;
        }
        sync_audio_stream = stream;
      }
    }
  }
  if (sync_audio_stream)
    sync_stream_mapping_[sync_group] = sync_audio_stream;
  size_t num_synced_streams = 0;
  for (VideoReceiveStream* video_stream : video_receive_streams_) {
    if (video_stream->config().sync_group != sync_group)
      continue;
    ++num_synced_streams;
    if (num_synced_streams > 1) {
      // TODO(pbos): Support synchronizing more than one A/V pair.
      // https://code.google.com/p/webrtc/issues/detail?id=4762
      RTC_LOG(LS_WARNING)
          << "Attempting to sync more than one audio/video pair "
             "within the same sync group. This is not supported in "
             "the current implementation.";
    }
    // Only sync the first A/V pair within this sync group.
    if (num_synced_streams == 1) {
      // sync_audio_stream may be null and that's ok.
      video_stream->SetSync(sync_audio_stream);
    } else {
      video_stream->SetSync(nullptr);
    }
  }
}

PacketReceiver::DeliveryStatus Call::DeliverRtcp(MediaType media_type,
                                                 const uint8_t* packet,
                                                 size_t length) {
  TRACE_EVENT0("webrtc", "Call::DeliverRtcp");
  // TODO(pbos): Make sure it's a valid packet.
  //             Return DELIVERY_UNKNOWN_SSRC if it can be determined that
  //             there's no receiver of the packet.
  if (received_bytes_per_second_counter_.HasSample()) {
    // First RTP packet has been received.
    received_bytes_per_second_counter_.Add(static_cast<int>(length));
    received_rtcp_bytes_per_second_counter_.Add(static_cast<int>(length));
  }
  bool rtcp_delivered = false;
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*receive_crit_);
    for (VideoReceiveStream* stream : video_receive_streams_) {
      if (stream->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::AUDIO) {
    ReadLockScoped read_lock(*receive_crit_);
    for (AudioReceiveStream* stream : audio_receive_streams_) {
      if (stream->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    ReadLockScoped read_lock(*send_crit_);
    for (VideoSendStream* stream : video_send_streams_) {
      if (stream->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::AUDIO) {
    ReadLockScoped read_lock(*send_crit_);
    for (auto& kv : audio_send_ssrcs_) {
      if (kv.second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }

  if (rtcp_delivered) {
    event_log_->Log(rtc::MakeUnique<RtcEventRtcpPacketIncoming>(
        rtc::MakeArrayView(packet, length)));
  }

  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(MediaType media_type,
                                                rtc::CopyOnWriteBuffer packet,
                                                const PacketTime& packet_time) {
  TRACE_EVENT0("webrtc", "Call::DeliverRtp");

  RtpPacketReceived parsed_packet;
  if (!parsed_packet.Parse(std::move(packet)))
    return DELIVERY_PACKET_ERROR;

  if (packet_time.timestamp != -1) {
    parsed_packet.set_arrival_time_ms((packet_time.timestamp + 500) / 1000);
  } else {
    parsed_packet.set_arrival_time_ms(clock_->TimeInMilliseconds());
  }

  // We might get RTP keep-alive packets in accordance with RFC6263 section 4.6.
  // These are empty (zero length payload) RTP packets with an unsignaled
  // payload type.
  const bool is_keep_alive_packet = parsed_packet.payload_size() == 0;

  RTC_DCHECK(media_type == MediaType::AUDIO || media_type == MediaType::VIDEO ||
             is_keep_alive_packet);

  ReadLockScoped read_lock(*receive_crit_);
  auto it = receive_rtp_config_.find(parsed_packet.Ssrc());
  if (it == receive_rtp_config_.end()) {
    RTC_LOG(LS_ERROR) << "receive_rtp_config_ lookup failed for ssrc "
                      << parsed_packet.Ssrc();
    // Destruction of the receive stream, including deregistering from the
    // RtpDemuxer, is not protected by the |receive_crit_| lock. But
    // deregistering in the |receive_rtp_config_| map is protected by that lock.
    // So by not passing the packet on to demuxing in this case, we prevent
    // incoming packets to be passed on via the demuxer to a receive stream
    // which is being torned down.
    return DELIVERY_UNKNOWN_SSRC;
  }
  parsed_packet.IdentifyExtensions(it->second.extensions);

  NotifyBweOfReceivedPacket(parsed_packet, media_type);

  // RateCounters expect input parameter as int, save it as int,
  // instead of converting each time it is passed to RateCounter::Add below.
  int length = static_cast<int>(parsed_packet.size());
  if (media_type == MediaType::AUDIO) {
    if (audio_receiver_controller_.OnRtpPacket(parsed_packet)) {
      received_bytes_per_second_counter_.Add(length);
      received_audio_bytes_per_second_counter_.Add(length);
      event_log_->Log(
          rtc::MakeUnique<RtcEventRtpPacketIncoming>(parsed_packet));
      const int64_t arrival_time_ms = parsed_packet.arrival_time_ms();
      if (!first_received_rtp_audio_ms_) {
        first_received_rtp_audio_ms_.emplace(arrival_time_ms);
      }
      last_received_rtp_audio_ms_.emplace(arrival_time_ms);
      return DELIVERY_OK;
    }
  } else if (media_type == MediaType::VIDEO) {
    if (video_receiver_controller_.OnRtpPacket(parsed_packet)) {
      received_bytes_per_second_counter_.Add(length);
      received_video_bytes_per_second_counter_.Add(length);
      event_log_->Log(
          rtc::MakeUnique<RtcEventRtpPacketIncoming>(parsed_packet));
      const int64_t arrival_time_ms = parsed_packet.arrival_time_ms();
      if (!first_received_rtp_video_ms_) {
        first_received_rtp_video_ms_.emplace(arrival_time_ms);
      }
      last_received_rtp_video_ms_.emplace(arrival_time_ms);
      return DELIVERY_OK;
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(
    MediaType media_type,
    rtc::CopyOnWriteBuffer packet,
    const PacketTime& packet_time) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&configuration_sequence_checker_);
  if (RtpHeaderParser::IsRtcp(packet.cdata(), packet.size()))
    return DeliverRtcp(media_type, packet.cdata(), packet.size());

  return DeliverRtp(media_type, std::move(packet), packet_time);
}

void Call::OnRecoveredPacket(const uint8_t* packet, size_t length) {
  RtpPacketReceived parsed_packet;
  if (!parsed_packet.Parse(packet, length))
    return;

  parsed_packet.set_recovered(true);

  ReadLockScoped read_lock(*receive_crit_);
  auto it = receive_rtp_config_.find(parsed_packet.Ssrc());
  if (it == receive_rtp_config_.end()) {
    RTC_LOG(LS_ERROR) << "receive_rtp_config_ lookup failed for ssrc "
                      << parsed_packet.Ssrc();
    // Destruction of the receive stream, including deregistering from the
    // RtpDemuxer, is not protected by the |receive_crit_| lock. But
    // deregistering in the |receive_rtp_config_| map is protected by that lock.
    // So by not passing the packet on to demuxing in this case, we prevent
    // incoming packets to be passed on via the demuxer to a receive stream
    // which is being torned down.
    return;
  }
  parsed_packet.IdentifyExtensions(it->second.extensions);

  // TODO(brandtr): Update here when we support protecting audio packets too.
  video_receiver_controller_.OnRtpPacket(parsed_packet);
}

void Call::NotifyBweOfReceivedPacket(const RtpPacketReceived& packet,
                                     MediaType media_type) {
  auto it = receive_rtp_config_.find(packet.Ssrc());
  bool use_send_side_bwe =
      (it != receive_rtp_config_.end()) && it->second.use_send_side_bwe;

  RTPHeader header;
  packet.GetHeader(&header);

  if (!use_send_side_bwe && header.extension.hasTransportSequenceNumber) {
    // Inconsistent configuration of send side BWE. Do nothing.
    // TODO(nisse): Without this check, we may produce RTCP feedback
    // packets even when not negotiated. But it would be cleaner to
    // move the check down to RTCPSender::SendFeedbackPacket, which
    // would also help the PacketRouter to select an appropriate rtp
    // module in the case that some, but not all, have RTCP feedback
    // enabled.
    return;
  }
  // For audio, we only support send side BWE.
  if (media_type == MediaType::VIDEO ||
      (use_send_side_bwe && header.extension.hasTransportSequenceNumber)) {
    receive_side_cc_.OnReceivedPacket(
        packet.arrival_time_ms(), packet.payload_size() + packet.padding_size(),
        header);
  }
}

}  // namespace internal

}  // namespace webrtc
