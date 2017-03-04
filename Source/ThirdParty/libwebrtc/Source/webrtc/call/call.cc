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

#include "webrtc/audio/audio_receive_stream.h"
#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/location.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/call/bitrate_allocator.h"
#include "webrtc/call/call.h"
#include "webrtc/call/flexfec_receive_stream_impl.h"
#include "webrtc/config.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/cpu_info.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/send_delay_stats.h"
#include "webrtc/video/stats_counter.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/video/vie_remb.h"

namespace webrtc {

const int Call::Config::kDefaultStartBitrateBps = 300000;

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

}  // namespace

namespace internal {

class Call : public webrtc::Call,
             public PacketReceiver,
             public RecoveredPacketReceiver,
             public CongestionController::Observer,
             public BitrateAllocator::LimitObserver {
 public:
  explicit Call(const Call::Config& config);
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
                               const uint8_t* packet,
                               size_t length,
                               const PacketTime& packet_time) override;

  // Implements RecoveredPacketReceiver.
  bool OnRecoveredPacket(const uint8_t* packet, size_t length) override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;

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
                            const uint8_t* packet,
                            size_t length,
                            const PacketTime& packet_time);
  void ConfigureSync(const std::string& sync_group)
      EXCLUSIVE_LOCKS_REQUIRED(receive_crit_);

  void NotifyBweOfReceivedPacket(const RtpPacketReceived& packet,
                                 MediaType media_type)
      SHARED_LOCKS_REQUIRED(receive_crit_);

  rtc::Optional<RtpPacketReceived> ParseRtpPacket(const uint8_t* packet,
                                                  size_t length,
                                                  const PacketTime& packet_time)
      SHARED_LOCKS_REQUIRED(receive_crit_);

  void UpdateSendHistograms() EXCLUSIVE_LOCKS_REQUIRED(&bitrate_crit_);
  void UpdateReceiveHistograms();
  void UpdateHistograms();
  void UpdateAggregateNetworkState();

  Clock* const clock_;

  const int num_cpu_cores_;
  const std::unique_ptr<ProcessThread> module_process_thread_;
  const std::unique_ptr<ProcessThread> pacer_thread_;
  const std::unique_ptr<CallStats> call_stats_;
  const std::unique_ptr<BitrateAllocator> bitrate_allocator_;
  Call::Config config_;
  rtc::ThreadChecker configuration_thread_checker_;

  NetworkState audio_network_state_;
  NetworkState video_network_state_;

  std::unique_ptr<RWLockWrapper> receive_crit_;
  // Audio, Video, and FlexFEC receive streams are owned by the client that
  // creates them.
  std::map<uint32_t, AudioReceiveStream*> audio_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::map<uint32_t, VideoReceiveStream*> video_receive_ssrcs_
      GUARDED_BY(receive_crit_);
  std::set<VideoReceiveStream*> video_receive_streams_
      GUARDED_BY(receive_crit_);
  // Each media stream could conceivably be protected by multiple FlexFEC
  // streams.
  std::multimap<uint32_t, FlexfecReceiveStreamImpl*>
      flexfec_receive_ssrcs_media_ GUARDED_BY(receive_crit_);
  std::map<uint32_t, FlexfecReceiveStreamImpl*>
      flexfec_receive_ssrcs_protection_ GUARDED_BY(receive_crit_);
  std::set<FlexfecReceiveStreamImpl*> flexfec_receive_streams_
      GUARDED_BY(receive_crit_);
  std::map<std::string, AudioReceiveStream*> sync_stream_mapping_
      GUARDED_BY(receive_crit_);

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
      GUARDED_BY(receive_crit_);

  std::unique_ptr<RWLockWrapper> send_crit_;
  // Audio and Video send streams are owned by the client that creates them.
  std::map<uint32_t, AudioSendStream*> audio_send_ssrcs_ GUARDED_BY(send_crit_);
  std::map<uint32_t, VideoSendStream*> video_send_ssrcs_ GUARDED_BY(send_crit_);
  std::set<VideoSendStream*> video_send_streams_ GUARDED_BY(send_crit_);

  VideoSendStream::RtpStateMap suspended_video_send_ssrcs_;
  webrtc::RtcEventLog* event_log_;

  // The following members are only accessed (exclusively) from one thread and
  // from the destructor, and therefore doesn't need any explicit
  // synchronization.
  int64_t first_packet_sent_ms_;
  RateCounter received_bytes_per_second_counter_;
  RateCounter received_audio_bytes_per_second_counter_;
  RateCounter received_video_bytes_per_second_counter_;
  RateCounter received_rtcp_bytes_per_second_counter_;

  // TODO(holmer): Remove this lock once BitrateController no longer calls
  // OnNetworkChanged from multiple threads.
  rtc::CriticalSection bitrate_crit_;
  uint32_t min_allocated_send_bitrate_bps_ GUARDED_BY(&bitrate_crit_);
  uint32_t configured_max_padding_bitrate_bps_ GUARDED_BY(&bitrate_crit_);
  AvgCounter estimated_send_bitrate_kbps_counter_ GUARDED_BY(&bitrate_crit_);
  AvgCounter pacer_bitrate_kbps_counter_ GUARDED_BY(&bitrate_crit_);

  std::map<std::string, rtc::NetworkRoute> network_routes_;

  VieRemb remb_;
  PacketRouter packet_router_;
  // TODO(nisse): Could be a direct member, except for constness
  // issues with GetRemoteBitrateEstimator (and maybe others).
  const std::unique_ptr<CongestionController> congestion_controller_;
  const std::unique_ptr<SendDelayStats> video_send_delay_stats_;
  const int64_t start_ms_;
  // TODO(perkj): |worker_queue_| is supposed to replace
  // |module_process_thread_|.
  // |worker_queue| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  rtc::TaskQueue worker_queue_;

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
  return new internal::Call(config);
}

namespace internal {

Call::Call(const Call::Config& config)
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
      first_packet_sent_ms_(-1),
      received_bytes_per_second_counter_(clock_, nullptr, true),
      received_audio_bytes_per_second_counter_(clock_, nullptr, true),
      received_video_bytes_per_second_counter_(clock_, nullptr, true),
      received_rtcp_bytes_per_second_counter_(clock_, nullptr, true),
      min_allocated_send_bitrate_bps_(0),
      configured_max_padding_bitrate_bps_(0),
      estimated_send_bitrate_kbps_counter_(clock_, nullptr, true),
      pacer_bitrate_kbps_counter_(clock_, nullptr, true),
      remb_(clock_),
      congestion_controller_(new CongestionController(clock_,
                                                      this,
                                                      &remb_,
                                                      event_log_,
                                                      &packet_router_)),
      video_send_delay_stats_(new SendDelayStats(clock_)),
      start_ms_(clock_->TimeInMilliseconds()),
      worker_queue_("call_worker_queue") {
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(config.event_log != nullptr);
  RTC_DCHECK_GE(config.bitrate_config.min_bitrate_bps, 0);
  RTC_DCHECK_GT(config.bitrate_config.start_bitrate_bps,
                config.bitrate_config.min_bitrate_bps);
  if (config.bitrate_config.max_bitrate_bps != -1) {
    RTC_DCHECK_GE(config.bitrate_config.max_bitrate_bps,
                  config.bitrate_config.start_bitrate_bps);
  }
  Trace::CreateTrace();
  call_stats_->RegisterStatsObserver(congestion_controller_.get());

  congestion_controller_->SignalNetworkState(kNetworkDown);
  congestion_controller_->SetBweBitrates(
      config_.bitrate_config.min_bitrate_bps,
      config_.bitrate_config.start_bitrate_bps,
      config_.bitrate_config.max_bitrate_bps);

  module_process_thread_->Start();
  module_process_thread_->RegisterModule(call_stats_.get(), RTC_FROM_HERE);
  module_process_thread_->RegisterModule(congestion_controller_.get(),
                                         RTC_FROM_HERE);
  pacer_thread_->RegisterModule(congestion_controller_->pacer(), RTC_FROM_HERE);
  pacer_thread_->RegisterModule(
      congestion_controller_->GetRemoteBitrateEstimator(true), RTC_FROM_HERE);
  pacer_thread_->Start();
}

Call::~Call() {
  RTC_DCHECK(!remb_.InUse());
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  RTC_CHECK(audio_send_ssrcs_.empty());
  RTC_CHECK(video_send_ssrcs_.empty());
  RTC_CHECK(video_send_streams_.empty());
  RTC_CHECK(audio_receive_ssrcs_.empty());
  RTC_CHECK(video_receive_ssrcs_.empty());
  RTC_CHECK(video_receive_streams_.empty());

  pacer_thread_->Stop();
  pacer_thread_->DeRegisterModule(congestion_controller_->pacer());
  pacer_thread_->DeRegisterModule(
      congestion_controller_->GetRemoteBitrateEstimator(true));
  module_process_thread_->DeRegisterModule(congestion_controller_.get());
  module_process_thread_->DeRegisterModule(call_stats_.get());
  module_process_thread_->Stop();
  call_stats_->DeregisterStatsObserver(congestion_controller_.get());

  // Only update histograms after process threads have been shut down, so that
  // they won't try to concurrently update stats.
  {
    rtc::CritScope lock(&bitrate_crit_);
    UpdateSendHistograms();
  }
  UpdateReceiveHistograms();
  UpdateHistograms();

  Trace::ReturnTrace();
}

rtc::Optional<RtpPacketReceived> Call::ParseRtpPacket(
    const uint8_t* packet,
    size_t length,
    const PacketTime& packet_time) {
  RtpPacketReceived parsed_packet;
  if (!parsed_packet.Parse(packet, length))
    return rtc::Optional<RtpPacketReceived>();

  auto it = receive_rtp_config_.find(parsed_packet.Ssrc());
  if (it != receive_rtp_config_.end())
    parsed_packet.IdentifyExtensions(it->second.extensions);

  int64_t arrival_time_ms;
  if (packet_time.timestamp != -1) {
    arrival_time_ms = (packet_time.timestamp + 500) / 1000;
  } else {
    arrival_time_ms = clock_->TimeInMilliseconds();
  }
  parsed_packet.set_arrival_time_ms(arrival_time_ms);

  return rtc::Optional<RtpPacketReceived>(std::move(parsed_packet));
}

void Call::UpdateHistograms() {
  RTC_HISTOGRAM_COUNTS_100000(
      "WebRTC.Call.LifetimeInSeconds",
      (clock_->TimeInMilliseconds() - start_ms_) / 1000);
}

void Call::UpdateSendHistograms() {
  if (first_packet_sent_ms_ == -1)
    return;
  int64_t elapsed_sec =
      (clock_->TimeInMilliseconds() - first_packet_sent_ms_) / 1000;
  if (elapsed_sec < metrics::kMinRunTimeInSeconds)
    return;
  const int kMinRequiredPeriodicSamples = 5;
  AggregatedStats send_bitrate_stats =
      estimated_send_bitrate_kbps_counter_.ProcessAndGetStats();
  if (send_bitrate_stats.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.EstimatedSendBitrateInKbps",
                                send_bitrate_stats.average);
    LOG(LS_INFO) << "WebRTC.Call.EstimatedSendBitrateInKbps, "
                 << send_bitrate_stats.ToString();
  }
  AggregatedStats pacer_bitrate_stats =
      pacer_bitrate_kbps_counter_.ProcessAndGetStats();
  if (pacer_bitrate_stats.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.PacerBitrateInKbps",
                                pacer_bitrate_stats.average);
    LOG(LS_INFO) << "WebRTC.Call.PacerBitrateInKbps, "
                 << pacer_bitrate_stats.ToString();
  }
}

void Call::UpdateReceiveHistograms() {
  const int kMinRequiredPeriodicSamples = 5;
  AggregatedStats video_bytes_per_sec =
      received_video_bytes_per_second_counter_.GetStats();
  if (video_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.VideoBitrateReceivedInKbps",
                                video_bytes_per_sec.average * 8 / 1000);
    LOG(LS_INFO) << "WebRTC.Call.VideoBitrateReceivedInBps, "
                 << video_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats audio_bytes_per_sec =
      received_audio_bytes_per_second_counter_.GetStats();
  if (audio_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.AudioBitrateReceivedInKbps",
                                audio_bytes_per_sec.average * 8 / 1000);
    LOG(LS_INFO) << "WebRTC.Call.AudioBitrateReceivedInBps, "
                 << audio_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats rtcp_bytes_per_sec =
      received_rtcp_bytes_per_second_counter_.GetStats();
  if (rtcp_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.RtcpBitrateReceivedInBps",
                                rtcp_bytes_per_sec.average * 8);
    LOG(LS_INFO) << "WebRTC.Call.RtcpBitrateReceivedInBps, "
                 << rtcp_bytes_per_sec.ToStringWithMultiplier(8);
  }
  AggregatedStats recv_bytes_per_sec =
      received_bytes_per_second_counter_.GetStats();
  if (recv_bytes_per_sec.num_samples > kMinRequiredPeriodicSamples) {
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Call.BitrateReceivedInKbps",
                                recv_bytes_per_sec.average * 8 / 1000);
    LOG(LS_INFO) << "WebRTC.Call.BitrateReceivedInBps, "
                 << recv_bytes_per_sec.ToStringWithMultiplier(8);
  }
}

PacketReceiver* Call::Receiver() {
  // TODO(solenberg): Some test cases in EndToEndTest use this from a different
  // thread. Re-enable once that is fixed.
  // RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  return this;
}

webrtc::AudioSendStream* Call::CreateAudioSendStream(
    const webrtc::AudioSendStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioSendStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  event_log_->LogAudioSendStreamConfig(config);
  AudioSendStream* send_stream = new AudioSendStream(
      config, config_.audio_state, &worker_queue_, &packet_router_,
      congestion_controller_.get(), bitrate_allocator_.get(), event_log_,
      call_stats_->rtcp_rtt_stats());
  {
    WriteLockScoped write_lock(*send_crit_);
    RTC_DCHECK(audio_send_ssrcs_.find(config.rtp.ssrc) ==
               audio_send_ssrcs_.end());
    audio_send_ssrcs_[config.rtp.ssrc] = send_stream;
  }
  {
    ReadLockScoped read_lock(*receive_crit_);
    for (const auto& kv : audio_receive_ssrcs_) {
      if (kv.second->config().rtp.local_ssrc == config.rtp.ssrc) {
        kv.second->AssociateSendStream(send_stream);
      }
    }
  }
  send_stream->SignalNetworkState(audio_network_state_);
  UpdateAggregateNetworkState();
  return send_stream;
}

void Call::DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyAudioSendStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(send_stream != nullptr);

  send_stream->Stop();

  webrtc::internal::AudioSendStream* audio_send_stream =
      static_cast<webrtc::internal::AudioSendStream*>(send_stream);
  uint32_t ssrc = audio_send_stream->config().rtp.ssrc;
  {
    WriteLockScoped write_lock(*send_crit_);
    size_t num_deleted = audio_send_ssrcs_.erase(ssrc);
    RTC_DCHECK_EQ(1, num_deleted);
  }
  {
    ReadLockScoped read_lock(*receive_crit_);
    for (const auto& kv : audio_receive_ssrcs_) {
      if (kv.second->config().rtp.local_ssrc == ssrc) {
        kv.second->AssociateSendStream(nullptr);
      }
    }
  }
  UpdateAggregateNetworkState();
  delete audio_send_stream;
}

webrtc::AudioReceiveStream* Call::CreateAudioReceiveStream(
    const webrtc::AudioReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateAudioReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  event_log_->LogAudioReceiveStreamConfig(config);
  AudioReceiveStream* receive_stream = new AudioReceiveStream(
      &packet_router_, config,
      config_.audio_state, event_log_);
  {
    WriteLockScoped write_lock(*receive_crit_);
    RTC_DCHECK(audio_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
               audio_receive_ssrcs_.end());
    audio_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
    receive_rtp_config_[config.rtp.remote_ssrc] =
        ReceiveRtpConfig(config.rtp.extensions, UseSendSideBwe(config));

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
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(receive_stream != nullptr);
  webrtc::internal::AudioReceiveStream* audio_receive_stream =
      static_cast<webrtc::internal::AudioReceiveStream*>(receive_stream);
  {
    WriteLockScoped write_lock(*receive_crit_);
    const AudioReceiveStream::Config& config = audio_receive_stream->config();
    uint32_t ssrc = config.rtp.remote_ssrc;
    congestion_controller_->GetRemoteBitrateEstimator(UseSendSideBwe(config))
        ->RemoveStream(ssrc);
    size_t num_deleted = audio_receive_ssrcs_.erase(ssrc);
    RTC_DCHECK(num_deleted == 1);
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
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  video_send_delay_stats_->AddSsrcs(config);
  event_log_->LogVideoSendStreamConfig(config);

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  // Copy ssrcs from |config| since |config| is moved.
  std::vector<uint32_t> ssrcs = config.rtp.ssrcs;
  VideoSendStream* send_stream = new VideoSendStream(
      num_cpu_cores_, module_process_thread_.get(), &worker_queue_,
      call_stats_.get(), congestion_controller_.get(), &packet_router_,
      bitrate_allocator_.get(), video_send_delay_stats_.get(), &remb_,
      event_log_, std::move(config), std::move(encoder_config),
      suspended_video_send_ssrcs_);

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
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

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

  VideoSendStream::RtpStateMap rtp_state =
      send_stream_impl->StopPermanentlyAndGetRtpStates();

  for (VideoSendStream::RtpStateMap::iterator it = rtp_state.begin();
       it != rtp_state.end(); ++it) {
    suspended_video_send_ssrcs_[it->first] = it->second;
  }

  UpdateAggregateNetworkState();
  delete send_stream_impl;
}

webrtc::VideoReceiveStream* Call::CreateVideoReceiveStream(
    webrtc::VideoReceiveStream::Config configuration) {
  TRACE_EVENT0("webrtc", "Call::CreateVideoReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  bool protected_by_flexfec = false;
  {
    ReadLockScoped read_lock(*receive_crit_);
    protected_by_flexfec =
        flexfec_receive_ssrcs_media_.find(configuration.rtp.remote_ssrc) !=
        flexfec_receive_ssrcs_media_.end();
  }
  VideoReceiveStream* receive_stream = new VideoReceiveStream(
      num_cpu_cores_, protected_by_flexfec,
      &packet_router_, std::move(configuration), module_process_thread_.get(),
      call_stats_.get(), &remb_);

  const webrtc::VideoReceiveStream::Config& config = receive_stream->config();
  ReceiveRtpConfig receive_config(config.rtp.extensions,
                                  UseSendSideBwe(config));
  {
    WriteLockScoped write_lock(*receive_crit_);
    RTC_DCHECK(video_receive_ssrcs_.find(config.rtp.remote_ssrc) ==
               video_receive_ssrcs_.end());
    video_receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
    if (config.rtp.rtx_ssrc) {
      video_receive_ssrcs_[config.rtp.rtx_ssrc] = receive_stream;
      // We record identical config for the rtx stream as for the main
      // stream. Since the transport_cc negotiation is per payload
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
  event_log_->LogVideoReceiveStreamConfig(config);
  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  TRACE_EVENT0("webrtc", "Call::DestroyVideoReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(receive_stream != nullptr);
  VideoReceiveStream* receive_stream_impl = nullptr;
  {
    WriteLockScoped write_lock(*receive_crit_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    auto it = video_receive_ssrcs_.begin();
    while (it != video_receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        if (receive_stream_impl != nullptr)
          RTC_DCHECK(receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        receive_rtp_config_.erase(it->first);
        it = video_receive_ssrcs_.erase(it);
      } else {
        ++it;
      }
    }
    video_receive_streams_.erase(receive_stream_impl);
    RTC_CHECK(receive_stream_impl != nullptr);
    ConfigureSync(receive_stream_impl->config().sync_group);
  }
  const VideoReceiveStream::Config& config = receive_stream_impl->config();

  congestion_controller_->GetRemoteBitrateEstimator(UseSendSideBwe(config))
      ->RemoveStream(config.rtp.remote_ssrc);

  UpdateAggregateNetworkState();
  delete receive_stream_impl;
}

FlexfecReceiveStream* Call::CreateFlexfecReceiveStream(
    const FlexfecReceiveStream::Config& config) {
  TRACE_EVENT0("webrtc", "Call::CreateFlexfecReceiveStream");
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  RecoveredPacketReceiver* recovered_packet_receiver = this;
  FlexfecReceiveStreamImpl* receive_stream = new FlexfecReceiveStreamImpl(
      config, recovered_packet_receiver, call_stats_->rtcp_rtt_stats(),
      module_process_thread_.get());

  {
    WriteLockScoped write_lock(*receive_crit_);

    RTC_DCHECK(flexfec_receive_streams_.find(receive_stream) ==
               flexfec_receive_streams_.end());
    flexfec_receive_streams_.insert(receive_stream);

    for (auto ssrc : config.protected_media_ssrcs)
      flexfec_receive_ssrcs_media_.insert(std::make_pair(ssrc, receive_stream));

    RTC_DCHECK(flexfec_receive_ssrcs_protection_.find(config.remote_ssrc) ==
               flexfec_receive_ssrcs_protection_.end());
    flexfec_receive_ssrcs_protection_[config.remote_ssrc] = receive_stream;

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
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

  RTC_DCHECK(receive_stream != nullptr);
  // There exist no other derived classes of FlexfecReceiveStream,
  // so this downcast is safe.
  FlexfecReceiveStreamImpl* receive_stream_impl =
      static_cast<FlexfecReceiveStreamImpl*>(receive_stream);
  {
    WriteLockScoped write_lock(*receive_crit_);

    const FlexfecReceiveStream::Config& config =
        receive_stream_impl->GetConfig();
    uint32_t ssrc = config.remote_ssrc;
    receive_rtp_config_.erase(ssrc);

    // Remove all SSRCs pointing to the FlexfecReceiveStreamImpl to be
    // destroyed.
    auto prot_it = flexfec_receive_ssrcs_protection_.begin();
    while (prot_it != flexfec_receive_ssrcs_protection_.end()) {
      if (prot_it->second == receive_stream_impl)
        prot_it = flexfec_receive_ssrcs_protection_.erase(prot_it);
      else
        ++prot_it;
    }
    auto media_it = flexfec_receive_ssrcs_media_.begin();
    while (media_it != flexfec_receive_ssrcs_media_.end()) {
      if (media_it->second == receive_stream_impl)
        media_it = flexfec_receive_ssrcs_media_.erase(media_it);
      else
        ++media_it;
    }

    congestion_controller_->GetRemoteBitrateEstimator(UseSendSideBwe(config))
        ->RemoveStream(ssrc);

    flexfec_receive_streams_.erase(receive_stream_impl);
  }

  delete receive_stream_impl;
}

Call::Stats Call::GetStats() const {
  // TODO(solenberg): Some test cases in EndToEndTest use this from a different
  // thread. Re-enable once that is fixed.
  // RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  Stats stats;
  // Fetch available send/receive bitrates.
  uint32_t send_bandwidth = 0;
  congestion_controller_->GetBitrateController()->AvailableBandwidth(
      &send_bandwidth);
  std::vector<unsigned int> ssrcs;
  uint32_t recv_bandwidth = 0;
  congestion_controller_->GetRemoteBitrateEstimator(false)->LatestEstimate(
      &ssrcs, &recv_bandwidth);
  stats.send_bandwidth_bps = send_bandwidth;
  stats.recv_bandwidth_bps = recv_bandwidth;
  stats.pacer_delay_ms = congestion_controller_->GetPacerQueuingDelayMs();
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
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  RTC_DCHECK_GE(bitrate_config.min_bitrate_bps, 0);
  if (bitrate_config.max_bitrate_bps != -1)
    RTC_DCHECK_GT(bitrate_config.max_bitrate_bps, 0);
  if (config_.bitrate_config.min_bitrate_bps ==
          bitrate_config.min_bitrate_bps &&
      (bitrate_config.start_bitrate_bps <= 0 ||
       config_.bitrate_config.start_bitrate_bps ==
           bitrate_config.start_bitrate_bps) &&
      config_.bitrate_config.max_bitrate_bps ==
          bitrate_config.max_bitrate_bps) {
    // Nothing new to set, early abort to avoid encoder reconfigurations.
    return;
  }
  config_.bitrate_config.min_bitrate_bps = bitrate_config.min_bitrate_bps;
  // Start bitrate of -1 means we should keep the old bitrate, which there is
  // no point in remembering for the future.
  if (bitrate_config.start_bitrate_bps > 0)
    config_.bitrate_config.start_bitrate_bps = bitrate_config.start_bitrate_bps;
  config_.bitrate_config.max_bitrate_bps = bitrate_config.max_bitrate_bps;
  RTC_DCHECK_NE(bitrate_config.start_bitrate_bps, 0);
  congestion_controller_->SetBweBitrates(bitrate_config.min_bitrate_bps,
                                         bitrate_config.start_bitrate_bps,
                                         bitrate_config.max_bitrate_bps);
}

void Call::SignalChannelNetworkState(MediaType media, NetworkState state) {
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
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
    for (auto& kv : audio_receive_ssrcs_) {
      kv.second->SignalNetworkState(audio_network_state_);
    }
    for (auto& kv : video_receive_ssrcs_) {
      kv.second->SignalNetworkState(video_network_state_);
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
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());
  // Check if the network route is connected.
  if (!network_route.connected) {
    LOG(LS_INFO) << "Transport " << transport_name << " is disconnected";
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
    LOG(LS_INFO) << "Network route changed on transport " << transport_name
                 << ": new local network id " << network_route.local_network_id
                 << " new remote network id " << network_route.remote_network_id
                 << " Reset bitrates to min: "
                 << config_.bitrate_config.min_bitrate_bps
                 << " bps, start: " << config_.bitrate_config.start_bitrate_bps
                 << " bps,  max: " << config_.bitrate_config.start_bitrate_bps
                 << " bps.";
    RTC_DCHECK_GT(config_.bitrate_config.start_bitrate_bps, 0);
    congestion_controller_->ResetBweAndBitrates(
        config_.bitrate_config.start_bitrate_bps,
        config_.bitrate_config.min_bitrate_bps,
        config_.bitrate_config.max_bitrate_bps);
  }
}

void Call::UpdateAggregateNetworkState() {
  RTC_DCHECK(configuration_thread_checker_.CalledOnValidThread());

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
    if (audio_receive_ssrcs_.size() > 0)
      have_audio = true;
    if (video_receive_ssrcs_.size() > 0)
      have_video = true;
  }

  NetworkState aggregate_state = kNetworkDown;
  if ((have_video && video_network_state_ == kNetworkUp) ||
      (have_audio && audio_network_state_ == kNetworkUp)) {
    aggregate_state = kNetworkUp;
  }

  LOG(LS_INFO) << "UpdateAggregateNetworkState: aggregate_state="
               << (aggregate_state == kNetworkUp ? "up" : "down");

  congestion_controller_->SignalNetworkState(aggregate_state);
}

void Call::OnSentPacket(const rtc::SentPacket& sent_packet) {
  if (first_packet_sent_ms_ == -1)
    first_packet_sent_ms_ = clock_->TimeInMilliseconds();
  video_send_delay_stats_->OnSentPacket(sent_packet.packet_id,
                                        clock_->TimeInMilliseconds());
  congestion_controller_->OnSentPacket(sent_packet);
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
  congestion_controller_->SetAllocatedSendBitrateLimits(
      min_send_bitrate_bps, max_padding_bitrate_bps);
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
    for (const auto& kv : audio_receive_ssrcs_) {
      if (kv.second->config().sync_group == sync_group) {
        if (sync_audio_stream != nullptr) {
          LOG(LS_WARNING) << "Attempting to sync more than one audio stream "
                             "within the same sync group. This is not "
                             "supported in the current implementation.";
          break;
        }
        sync_audio_stream = kv.second;
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
      LOG(LS_WARNING) << "Attempting to sync more than one audio/video pair "
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
    for (auto& kv : audio_receive_ssrcs_) {
      if (kv.second->DeliverRtcp(packet, length))
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

  if (rtcp_delivered)
    event_log_->LogRtcpPacket(kIncomingPacket, media_type, packet, length);

  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(MediaType media_type,
                                                const uint8_t* packet,
                                                size_t length,
                                                const PacketTime& packet_time) {
  TRACE_EVENT0("webrtc", "Call::DeliverRtp");

  ReadLockScoped read_lock(*receive_crit_);
  // TODO(nisse): We should parse the RTP header only here, and pass
  // on parsed_packet to the receive streams.
  rtc::Optional<RtpPacketReceived> parsed_packet =
      ParseRtpPacket(packet, length, packet_time);

  if (!parsed_packet)
    return DELIVERY_PACKET_ERROR;

  NotifyBweOfReceivedPacket(*parsed_packet, media_type);

  uint32_t ssrc = parsed_packet->Ssrc();

  if (media_type == MediaType::ANY || media_type == MediaType::AUDIO) {
    auto it = audio_receive_ssrcs_.find(ssrc);
    if (it != audio_receive_ssrcs_.end()) {
      received_bytes_per_second_counter_.Add(static_cast<int>(length));
      received_audio_bytes_per_second_counter_.Add(static_cast<int>(length));
      it->second->OnRtpPacket(*parsed_packet);
      event_log_->LogRtpHeader(kIncomingPacket, media_type, packet, length);
      return DELIVERY_OK;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    auto it = video_receive_ssrcs_.find(ssrc);
    if (it != video_receive_ssrcs_.end()) {
      received_bytes_per_second_counter_.Add(static_cast<int>(length));
      received_video_bytes_per_second_counter_.Add(static_cast<int>(length));
      it->second->OnRtpPacket(*parsed_packet);

      // Deliver media packets to FlexFEC subsystem.
      auto it_bounds = flexfec_receive_ssrcs_media_.equal_range(ssrc);
      for (auto it = it_bounds.first; it != it_bounds.second; ++it)
        it->second->OnRtpPacket(*parsed_packet);

      event_log_->LogRtpHeader(kIncomingPacket, media_type, packet, length);
      return DELIVERY_OK;
    }
  }
  if (media_type == MediaType::ANY || media_type == MediaType::VIDEO) {
    received_bytes_per_second_counter_.Add(static_cast<int>(length));
    // TODO(brandtr): Update here when FlexFEC supports protecting audio.
    received_video_bytes_per_second_counter_.Add(static_cast<int>(length));
    auto it = flexfec_receive_ssrcs_protection_.find(ssrc);
    if (it != flexfec_receive_ssrcs_protection_.end()) {
      it->second->OnRtpPacket(*parsed_packet);
      event_log_->LogRtpHeader(kIncomingPacket, media_type, packet, length);
      return DELIVERY_OK;
    }
  }
  return DELIVERY_UNKNOWN_SSRC;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(
    MediaType media_type,
    const uint8_t* packet,
    size_t length,
    const PacketTime& packet_time) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!configuration_thread_checker_.CalledOnValidThread());
  if (RtpHeaderParser::IsRtcp(packet, length))
    return DeliverRtcp(media_type, packet, length);

  return DeliverRtp(media_type, packet, length, packet_time);
}

// TODO(brandtr): Update this member function when we support protecting
// audio packets with FlexFEC.
bool Call::OnRecoveredPacket(const uint8_t* packet, size_t length) {
  uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&packet[8]);
  ReadLockScoped read_lock(*receive_crit_);
  auto it = video_receive_ssrcs_.find(ssrc);
  if (it == video_receive_ssrcs_.end())
    return false;
  return it->second->OnRecoveredPacket(packet, length);
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
  // TODO(nisse): Tests passes MediaType::ANY, see
  // FakeNetworkPipe::Process. We need to treat that as video. Tests
  // should be fixed to use the same MediaType as the production code.
  if (media_type != MediaType::AUDIO ||
      (use_send_side_bwe && header.extension.hasTransportSequenceNumber)) {
    congestion_controller_->OnReceivedPacket(
        packet.arrival_time_ms(), packet.payload_size() + packet.padding_size(),
        header);
  }
}

}  // namespace internal
}  // namespace webrtc
