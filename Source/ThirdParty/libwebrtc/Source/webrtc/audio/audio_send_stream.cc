/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_send_stream.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"

#include "audio/audio_state.h"
#include "audio/channel_send_proxy.h"
#include "audio/conversion.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/function_view.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/audio_format_to_string.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace internal {
namespace {
// TODO(eladalon): Subsequent CL will make these values experiment-dependent.
constexpr size_t kPacketLossTrackerMaxWindowSizeMs = 15000;
constexpr size_t kPacketLossRateMinNumAckedPackets = 50;
constexpr size_t kRecoverablePacketLossRateMinNumAckedPairs = 40;

void CallEncoder(const std::unique_ptr<voe::ChannelSendProxy>& channel_proxy,
                 rtc::FunctionView<void(AudioEncoder*)> lambda) {
  channel_proxy->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder_ptr) {
    RTC_DCHECK(encoder_ptr);
    lambda(encoder_ptr->get());
  });
}

std::unique_ptr<voe::ChannelSendProxy> CreateChannelAndProxy(
    rtc::TaskQueue* worker_queue,
    ProcessThread* module_process_thread,
    RtcpRttStats* rtcp_rtt_stats,
    RtcEventLog* event_log,
    FrameEncryptorInterface* frame_encryptor) {
  return absl::make_unique<voe::ChannelSendProxy>(
      absl::make_unique<voe::ChannelSend>(worker_queue, module_process_thread,
                                          rtcp_rtt_stats, event_log,
                                          frame_encryptor));
}
}  // namespace

// Helper class to track the actively sending lifetime of this stream.
class AudioSendStream::TimedTransport : public Transport {
 public:
  TimedTransport(Transport* transport, TimeInterval* time_interval)
      : transport_(transport), lifetime_(time_interval) {}
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) {
    if (lifetime_) {
      lifetime_->Extend();
    }
    return transport_->SendRtp(packet, length, options);
  }
  bool SendRtcp(const uint8_t* packet, size_t length) {
    return transport_->SendRtcp(packet, length);
  }
  ~TimedTransport() {}

 private:
  Transport* transport_;
  TimeInterval* lifetime_;
};

AudioSendStream::AudioSendStream(
    const webrtc::AudioSendStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    rtc::TaskQueue* worker_queue,
    ProcessThread* module_process_thread,
    RtpTransportControllerSendInterface* transport,
    BitrateAllocator* bitrate_allocator,
    RtcEventLog* event_log,
    RtcpRttStats* rtcp_rtt_stats,
    const absl::optional<RtpState>& suspended_rtp_state,
    TimeInterval* overall_call_lifetime)
    : AudioSendStream(config,
                      audio_state,
                      worker_queue,
                      transport,
                      bitrate_allocator,
                      event_log,
                      rtcp_rtt_stats,
                      suspended_rtp_state,
                      overall_call_lifetime,
                      CreateChannelAndProxy(worker_queue,
                                            module_process_thread,
                                            rtcp_rtt_stats,
                                            event_log,
                                            config.frame_encryptor)) {}

AudioSendStream::AudioSendStream(
    const webrtc::AudioSendStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    rtc::TaskQueue* worker_queue,
    RtpTransportControllerSendInterface* transport,
    BitrateAllocator* bitrate_allocator,
    RtcEventLog* event_log,
    RtcpRttStats* rtcp_rtt_stats,
    const absl::optional<RtpState>& suspended_rtp_state,
    TimeInterval* overall_call_lifetime,
    std::unique_ptr<voe::ChannelSendProxy> channel_proxy)
    : worker_queue_(worker_queue),
      config_(Config(nullptr)),
      audio_state_(audio_state),
      channel_proxy_(std::move(channel_proxy)),
      event_log_(event_log),
      bitrate_allocator_(bitrate_allocator),
      transport_(transport),
      packet_loss_tracker_(kPacketLossTrackerMaxWindowSizeMs,
                           kPacketLossRateMinNumAckedPackets,
                           kRecoverablePacketLossRateMinNumAckedPairs),
      rtp_rtcp_module_(nullptr),
      suspended_rtp_state_(suspended_rtp_state),
      overall_call_lifetime_(overall_call_lifetime) {
  RTC_LOG(LS_INFO) << "AudioSendStream: " << config.rtp.ssrc;
  RTC_DCHECK(worker_queue_);
  RTC_DCHECK(audio_state_);
  RTC_DCHECK(channel_proxy_);
  RTC_DCHECK(bitrate_allocator_);
  RTC_DCHECK(transport);
  RTC_DCHECK(overall_call_lifetime_);

  channel_proxy_->SetRTCPStatus(true);
  rtp_rtcp_module_ = channel_proxy_->GetRtpRtcp();
  RTC_DCHECK(rtp_rtcp_module_);

  ConfigureStream(this, config, true);

  pacer_thread_checker_.DetachFromThread();
  // Signal congestion controller this object is ready for OnPacket* callbacks.
  transport_->RegisterPacketFeedbackObserver(this);
}

AudioSendStream::~AudioSendStream() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "~AudioSendStream: " << config_.rtp.ssrc;
  RTC_DCHECK(!sending_);
  transport_->DeRegisterPacketFeedbackObserver(this);
  channel_proxy_->RegisterTransport(nullptr);
  channel_proxy_->ResetSenderCongestionControlObjects();
  // Lifetime can only be updated after deregistering
  // |timed_send_transport_adapter_| in the underlying channel object to avoid
  // data races in |active_lifetime_|.
  overall_call_lifetime_->Extend(active_lifetime_);
}

const webrtc::AudioSendStream::Config& AudioSendStream::GetConfig() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return config_;
}

void AudioSendStream::Reconfigure(
    const webrtc::AudioSendStream::Config& new_config) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  ConfigureStream(this, new_config, false);
}

AudioSendStream::ExtensionIds AudioSendStream::FindExtensionIds(
    const std::vector<RtpExtension>& extensions) {
  ExtensionIds ids;
  for (const auto& extension : extensions) {
    if (extension.uri == RtpExtension::kAudioLevelUri) {
      ids.audio_level = extension.id;
    } else if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
      ids.transport_sequence_number = extension.id;
    } else if (extension.uri == RtpExtension::kMidUri) {
      ids.mid = extension.id;
    }
  }
  return ids;
}

void AudioSendStream::ConfigureStream(
    webrtc::internal::AudioSendStream* stream,
    const webrtc::AudioSendStream::Config& new_config,
    bool first_time) {
  RTC_LOG(LS_INFO) << "AudioSendStream::ConfigureStream: "
                   << new_config.ToString();
  const auto& channel_proxy = stream->channel_proxy_;
  const auto& old_config = stream->config_;

  if (first_time || old_config.rtp.ssrc != new_config.rtp.ssrc) {
    channel_proxy->SetLocalSSRC(new_config.rtp.ssrc);
    if (stream->suspended_rtp_state_) {
      stream->rtp_rtcp_module_->SetRtpState(*stream->suspended_rtp_state_);
    }
  }
  if (first_time || old_config.rtp.c_name != new_config.rtp.c_name) {
    channel_proxy->SetRTCP_CNAME(new_config.rtp.c_name);
  }
  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  if (first_time || old_config.rtp.nack.rtp_history_ms !=
                        new_config.rtp.nack.rtp_history_ms) {
    channel_proxy->SetNACKStatus(new_config.rtp.nack.rtp_history_ms != 0,
                                 new_config.rtp.nack.rtp_history_ms / 20);
  }

  if (first_time || new_config.send_transport != old_config.send_transport) {
    if (old_config.send_transport) {
      channel_proxy->RegisterTransport(nullptr);
    }
    if (new_config.send_transport) {
      stream->timed_send_transport_adapter_.reset(new TimedTransport(
          new_config.send_transport, &stream->active_lifetime_));
    } else {
      stream->timed_send_transport_adapter_.reset(nullptr);
    }
    channel_proxy->RegisterTransport(
        stream->timed_send_transport_adapter_.get());
  }

  // Enable the frame encryptor if a new frame encryptor has been provided.
  if (first_time || new_config.frame_encryptor != old_config.frame_encryptor) {
    channel_proxy->SetFrameEncryptor(new_config.frame_encryptor);
  }

  const ExtensionIds old_ids = FindExtensionIds(old_config.rtp.extensions);
  const ExtensionIds new_ids = FindExtensionIds(new_config.rtp.extensions);
  // Audio level indication
  if (first_time || new_ids.audio_level != old_ids.audio_level) {
    channel_proxy->SetSendAudioLevelIndicationStatus(new_ids.audio_level != 0,
                                                     new_ids.audio_level);
  }
  bool transport_seq_num_id_changed =
      new_ids.transport_sequence_number != old_ids.transport_sequence_number;
  if (first_time ||
      (transport_seq_num_id_changed &&
       !webrtc::field_trial::IsEnabled("WebRTC-Audio-ForceNoTWCC"))) {
    if (!first_time) {
      channel_proxy->ResetSenderCongestionControlObjects();
    }

    RtcpBandwidthObserver* bandwidth_observer = nullptr;
    bool has_transport_sequence_number =
        new_ids.transport_sequence_number != 0 &&
        !webrtc::field_trial::IsEnabled("WebRTC-Audio-ForceNoTWCC");
    if (has_transport_sequence_number) {
      channel_proxy->EnableSendTransportSequenceNumber(
          new_ids.transport_sequence_number);
      // Probing in application limited region is only used in combination with
      // send side congestion control, wich depends on feedback packets which
      // requires transport sequence numbers to be enabled.
      stream->transport_->EnablePeriodicAlrProbing(true);
      bandwidth_observer = stream->transport_->GetBandwidthObserver();
    }

    channel_proxy->RegisterSenderCongestionControlObjects(stream->transport_,
                                                          bandwidth_observer);
  }

  // MID RTP header extension.
  if ((first_time || new_ids.mid != old_ids.mid ||
       new_config.rtp.mid != old_config.rtp.mid) &&
      new_ids.mid != 0 && !new_config.rtp.mid.empty()) {
    channel_proxy->SetMid(new_config.rtp.mid, new_ids.mid);
  }

  if (!ReconfigureSendCodec(stream, new_config)) {
    RTC_LOG(LS_ERROR) << "Failed to set up send codec state.";
  }

  if (stream->sending_) {
    ReconfigureBitrateObserver(stream, new_config);
  }
  stream->config_ = new_config;
}

void AudioSendStream::Start() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  if (sending_) {
    return;
  }

  bool has_transport_sequence_number =
      FindExtensionIds(config_.rtp.extensions).transport_sequence_number != 0 &&
      !webrtc::field_trial::IsEnabled("WebRTC-Audio-ForceNoTWCC");
  if (config_.min_bitrate_bps != -1 && config_.max_bitrate_bps != -1 &&
      (has_transport_sequence_number ||
       !webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe") ||
       webrtc::field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC"))) {
    // Audio BWE is enabled.
    transport_->packet_sender()->SetAccountForAudioPackets(true);
    ConfigureBitrateObserver(config_.min_bitrate_bps, config_.max_bitrate_bps,
                             config_.bitrate_priority,
                             has_transport_sequence_number);
  }
  channel_proxy_->StartSend();
  sending_ = true;
  audio_state()->AddSendingStream(this, encoder_sample_rate_hz_,
                                  encoder_num_channels_);
}

void AudioSendStream::Stop() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  if (!sending_) {
    return;
  }

  RemoveBitrateObserver();
  channel_proxy_->StopSend();
  sending_ = false;
  audio_state()->RemoveSendingStream(this);
}

void AudioSendStream::SendAudioData(std::unique_ptr<AudioFrame> audio_frame) {
  RTC_CHECK_RUNS_SERIALIZED(&audio_capture_race_checker_);
  channel_proxy_->ProcessAndEncodeAudio(std::move(audio_frame));
}

bool AudioSendStream::SendTelephoneEvent(int payload_type,
                                         int payload_frequency,
                                         int event,
                                         int duration_ms) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_proxy_->SetSendTelephoneEventPayloadType(payload_type,
                                                          payload_frequency) &&
         channel_proxy_->SendTelephoneEventOutband(event, duration_ms);
}

void AudioSendStream::SetMuted(bool muted) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_proxy_->SetInputMute(muted);
}

webrtc::AudioSendStream::Stats AudioSendStream::GetStats() const {
  return GetStats(true);
}

webrtc::AudioSendStream::Stats AudioSendStream::GetStats(
    bool has_remote_tracks) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  webrtc::AudioSendStream::Stats stats;
  stats.local_ssrc = config_.rtp.ssrc;

  webrtc::CallSendStatistics call_stats = channel_proxy_->GetRTCPStatistics();
  stats.bytes_sent = call_stats.bytesSent;
  stats.packets_sent = call_stats.packetsSent;
  // RTT isn't known until a RTCP report is received. Until then, VoiceEngine
  // returns 0 to indicate an error value.
  if (call_stats.rttMs > 0) {
    stats.rtt_ms = call_stats.rttMs;
  }
  if (config_.send_codec_spec) {
    const auto& spec = *config_.send_codec_spec;
    stats.codec_name = spec.format.name;
    stats.codec_payload_type = spec.payload_type;

    // Get data from the last remote RTCP report.
    for (const auto& block : channel_proxy_->GetRemoteRTCPReportBlocks()) {
      // Lookup report for send ssrc only.
      if (block.source_SSRC == stats.local_ssrc) {
        stats.packets_lost = block.cumulative_num_packets_lost;
        stats.fraction_lost = Q8ToFloat(block.fraction_lost);
        stats.ext_seqnum = block.extended_highest_sequence_number;
        // Convert timestamps to milliseconds.
        if (spec.format.clockrate_hz / 1000 > 0) {
          stats.jitter_ms =
              block.interarrival_jitter / (spec.format.clockrate_hz / 1000);
        }
        break;
      }
    }
  }

  AudioState::Stats input_stats = audio_state()->GetAudioInputStats();
  stats.audio_level = input_stats.audio_level;
  stats.total_input_energy = input_stats.total_energy;
  stats.total_input_duration = input_stats.total_duration;

  stats.typing_noise_detected = audio_state()->typing_noise_detected();
  stats.ana_statistics = channel_proxy_->GetANAStatistics();
  RTC_DCHECK(audio_state_->audio_processing());
  stats.apm_statistics =
      audio_state_->audio_processing()->GetStatistics(has_remote_tracks);

  return stats;
}

void AudioSendStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
}

bool AudioSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!worker_thread_checker_.CalledOnValidThread());
  return channel_proxy_->ReceivedRTCPPacket(packet, length);
}

uint32_t AudioSendStream::OnBitrateUpdated(uint32_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt,
                                           int64_t bwe_period_ms) {
  // A send stream may be allocated a bitrate of zero if the allocator decides
  // to disable it. For now we ignore this decision and keep sending on min
  // bitrate.
  if (bitrate_bps == 0) {
    bitrate_bps = config_.min_bitrate_bps;
  }
  RTC_DCHECK_GE(bitrate_bps, static_cast<uint32_t>(config_.min_bitrate_bps));
  // The bitrate allocator might allocate an higher than max configured bitrate
  // if there is room, to allow for, as example, extra FEC. Ignore that for now.
  const uint32_t max_bitrate_bps = config_.max_bitrate_bps;
  if (bitrate_bps > max_bitrate_bps)
    bitrate_bps = max_bitrate_bps;

  channel_proxy_->SetBitrate(bitrate_bps, bwe_period_ms);

  // The amount of audio protection is not exposed by the encoder, hence
  // always returning 0.
  return 0;
}

void AudioSendStream::OnPacketAdded(uint32_t ssrc, uint16_t seq_num) {
  RTC_DCHECK(pacer_thread_checker_.CalledOnValidThread());
  // Only packets that belong to this stream are of interest.
  if (ssrc == config_.rtp.ssrc) {
    rtc::CritScope lock(&packet_loss_tracker_cs_);
    // TODO(eladalon): This function call could potentially reset the window,
    // setting both PLR and RPLR to unknown. Consider (during upcoming
    // refactoring) passing an indication of such an event.
    packet_loss_tracker_.OnPacketAdded(seq_num, rtc::TimeMillis());
  }
}

void AudioSendStream::OnPacketFeedbackVector(
    const std::vector<PacketFeedback>& packet_feedback_vector) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  absl::optional<float> plr;
  absl::optional<float> rplr;
  {
    rtc::CritScope lock(&packet_loss_tracker_cs_);
    packet_loss_tracker_.OnPacketFeedbackVector(packet_feedback_vector);
    plr = packet_loss_tracker_.GetPacketLossRate();
    rplr = packet_loss_tracker_.GetRecoverablePacketLossRate();
  }
  // TODO(eladalon): If R/PLR go back to unknown, no indication is given that
  // the previously sent value is no longer relevant. This will be taken care
  // of with some refactoring which is now being done.
  if (plr) {
    channel_proxy_->OnTwccBasedUplinkPacketLossRate(*plr);
  }
  if (rplr) {
    channel_proxy_->OnRecoverableUplinkPacketLossRate(*rplr);
  }
}

void AudioSendStream::SetTransportOverhead(int transport_overhead_per_packet) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_proxy_->SetTransportOverhead(transport_overhead_per_packet);
}

RtpState AudioSendStream::GetRtpState() const {
  return rtp_rtcp_module_->GetRtpState();
}

const voe::ChannelSendProxy& AudioSendStream::GetChannelProxy() const {
  RTC_DCHECK(channel_proxy_.get());
  return *channel_proxy_.get();
}

internal::AudioState* AudioSendStream::audio_state() {
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  RTC_DCHECK(audio_state);
  return audio_state;
}

const internal::AudioState* AudioSendStream::audio_state() const {
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  RTC_DCHECK(audio_state);
  return audio_state;
}

void AudioSendStream::StoreEncoderProperties(int sample_rate_hz,
                                             size_t num_channels) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  encoder_sample_rate_hz_ = sample_rate_hz;
  encoder_num_channels_ = num_channels;
  if (sending_) {
    // Update AudioState's information about the stream.
    audio_state()->AddSendingStream(this, sample_rate_hz, num_channels);
  }
}

// Apply current codec settings to a single voe::Channel used for sending.
bool AudioSendStream::SetupSendCodec(AudioSendStream* stream,
                                     const Config& new_config) {
  RTC_DCHECK(new_config.send_codec_spec);
  const auto& spec = *new_config.send_codec_spec;

  RTC_DCHECK(new_config.encoder_factory);
  std::unique_ptr<AudioEncoder> encoder =
      new_config.encoder_factory->MakeAudioEncoder(
          spec.payload_type, spec.format, new_config.codec_pair_id);

  if (!encoder) {
    RTC_DLOG(LS_ERROR) << "Unable to create encoder for "
                       << rtc::ToString(spec.format);
    return false;
  }

  // If other side does not support audio TWCC and WebRTC-Audio-ABWENoTWCC is
  // not enabled, do not update target audio bitrate if we are in
  // WebRTC-Audio-SendSideBwe-For-Video experiment
  const bool do_not_update_target_bitrate =
      !webrtc::field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC") &&
      webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe-For-Video") &&
      !FindExtensionIds(new_config.rtp.extensions).transport_sequence_number;
  // If a bitrate has been specified for the codec, use it over the
  // codec's default.
  if (!do_not_update_target_bitrate && spec.target_bitrate_bps) {
    encoder->OnReceivedTargetAudioBitrate(*spec.target_bitrate_bps);
  }

  // Enable ANA if configured (currently only used by Opus).
  if (new_config.audio_network_adaptor_config) {
    if (encoder->EnableAudioNetworkAdaptor(
            *new_config.audio_network_adaptor_config, stream->event_log_)) {
      RTC_DLOG(LS_INFO) << "Audio network adaptor enabled on SSRC "
                        << new_config.rtp.ssrc;
    } else {
      RTC_NOTREACHED();
    }
  }

  // Wrap the encoder in a an AudioEncoderCNG, if VAD is enabled.
  if (spec.cng_payload_type) {
    AudioEncoderCng::Config cng_config;
    cng_config.num_channels = encoder->NumChannels();
    cng_config.payload_type = *spec.cng_payload_type;
    cng_config.speech_encoder = std::move(encoder);
    cng_config.vad_mode = Vad::kVadNormal;
    encoder.reset(new AudioEncoderCng(std::move(cng_config)));

    stream->RegisterCngPayloadType(
        *spec.cng_payload_type,
        new_config.send_codec_spec->format.clockrate_hz);
  }

  stream->StoreEncoderProperties(encoder->SampleRateHz(),
                                 encoder->NumChannels());
  stream->channel_proxy_->SetEncoder(new_config.send_codec_spec->payload_type,
                                     std::move(encoder));
  return true;
}

bool AudioSendStream::ReconfigureSendCodec(AudioSendStream* stream,
                                           const Config& new_config) {
  const auto& old_config = stream->config_;

  if (!new_config.send_codec_spec) {
    // We cannot de-configure a send codec. So we will do nothing.
    // By design, the send codec should have not been configured.
    RTC_DCHECK(!old_config.send_codec_spec);
    return true;
  }

  if (new_config.send_codec_spec == old_config.send_codec_spec &&
      new_config.audio_network_adaptor_config ==
          old_config.audio_network_adaptor_config) {
    return true;
  }

  // If we have no encoder, or the format or payload type's changed, create a
  // new encoder.
  if (!old_config.send_codec_spec ||
      new_config.send_codec_spec->format !=
          old_config.send_codec_spec->format ||
      new_config.send_codec_spec->payload_type !=
          old_config.send_codec_spec->payload_type) {
    return SetupSendCodec(stream, new_config);
  }

  // If other side does not support audio TWCC and WebRTC-Audio-ABWENoTWCC is
  // not enabled, do not update target audio bitrate if we are in
  // WebRTC-Audio-SendSideBwe-For-Video experiment
  const bool do_not_update_target_bitrate =
      !webrtc::field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC") &&
      webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe-For-Video") &&
      !FindExtensionIds(new_config.rtp.extensions).transport_sequence_number;

  const absl::optional<int>& new_target_bitrate_bps =
      new_config.send_codec_spec->target_bitrate_bps;
  // If a bitrate has been specified for the codec, use it over the
  // codec's default.
  if (!do_not_update_target_bitrate && new_target_bitrate_bps &&
      new_target_bitrate_bps !=
          old_config.send_codec_spec->target_bitrate_bps) {
    CallEncoder(stream->channel_proxy_, [&](AudioEncoder* encoder) {
      encoder->OnReceivedTargetAudioBitrate(*new_target_bitrate_bps);
    });
  }

  ReconfigureANA(stream, new_config);
  ReconfigureCNG(stream, new_config);

  return true;
}

void AudioSendStream::ReconfigureANA(AudioSendStream* stream,
                                     const Config& new_config) {
  if (new_config.audio_network_adaptor_config ==
      stream->config_.audio_network_adaptor_config) {
    return;
  }
  if (new_config.audio_network_adaptor_config) {
    CallEncoder(stream->channel_proxy_, [&](AudioEncoder* encoder) {
      if (encoder->EnableAudioNetworkAdaptor(
              *new_config.audio_network_adaptor_config, stream->event_log_)) {
        RTC_DLOG(LS_INFO) << "Audio network adaptor enabled on SSRC "
                          << new_config.rtp.ssrc;
      } else {
        RTC_NOTREACHED();
      }
    });
  } else {
    CallEncoder(stream->channel_proxy_, [&](AudioEncoder* encoder) {
      encoder->DisableAudioNetworkAdaptor();
    });
    RTC_DLOG(LS_INFO) << "Audio network adaptor disabled on SSRC "
                      << new_config.rtp.ssrc;
  }
}

void AudioSendStream::ReconfigureCNG(AudioSendStream* stream,
                                     const Config& new_config) {
  if (new_config.send_codec_spec->cng_payload_type ==
      stream->config_.send_codec_spec->cng_payload_type) {
    return;
  }

  // Register the CNG payload type if it's been added, don't do anything if CNG
  // is removed. Payload types must not be redefined.
  if (new_config.send_codec_spec->cng_payload_type) {
    stream->RegisterCngPayloadType(
        *new_config.send_codec_spec->cng_payload_type,
        new_config.send_codec_spec->format.clockrate_hz);
  }

  // Wrap or unwrap the encoder in an AudioEncoderCNG.
  stream->channel_proxy_->ModifyEncoder(
      [&](std::unique_ptr<AudioEncoder>* encoder_ptr) {
        std::unique_ptr<AudioEncoder> old_encoder(std::move(*encoder_ptr));
        auto sub_encoders = old_encoder->ReclaimContainedEncoders();
        if (!sub_encoders.empty()) {
          // Replace enc with its sub encoder. We need to put the sub
          // encoder in a temporary first, since otherwise the old value
          // of enc would be destroyed before the new value got assigned,
          // which would be bad since the new value is a part of the old
          // value.
          auto tmp = std::move(sub_encoders[0]);
          old_encoder = std::move(tmp);
        }
        if (new_config.send_codec_spec->cng_payload_type) {
          AudioEncoderCng::Config config;
          config.speech_encoder = std::move(old_encoder);
          config.num_channels = config.speech_encoder->NumChannels();
          config.payload_type = *new_config.send_codec_spec->cng_payload_type;
          config.vad_mode = Vad::kVadNormal;
          encoder_ptr->reset(new AudioEncoderCng(std::move(config)));
        } else {
          *encoder_ptr = std::move(old_encoder);
        }
      });
}

void AudioSendStream::ReconfigureBitrateObserver(
    AudioSendStream* stream,
    const webrtc::AudioSendStream::Config& new_config) {
  // Since the Config's default is for both of these to be -1, this test will
  // allow us to configure the bitrate observer if the new config has bitrate
  // limits set, but would only have us call RemoveBitrateObserver if we were
  // previously configured with bitrate limits.
  int new_transport_seq_num_id =
      FindExtensionIds(new_config.rtp.extensions).transport_sequence_number;
  if (stream->config_.min_bitrate_bps == new_config.min_bitrate_bps &&
      stream->config_.max_bitrate_bps == new_config.max_bitrate_bps &&
      stream->config_.bitrate_priority == new_config.bitrate_priority &&
      (FindExtensionIds(stream->config_.rtp.extensions)
               .transport_sequence_number == new_transport_seq_num_id ||
       !webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe"))) {
    return;
  }

  bool has_transport_sequence_number = new_transport_seq_num_id != 0;
  if (new_config.min_bitrate_bps != -1 && new_config.max_bitrate_bps != -1 &&
      (has_transport_sequence_number ||
       !webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe"))) {
    stream->ConfigureBitrateObserver(
        new_config.min_bitrate_bps, new_config.max_bitrate_bps,
        new_config.bitrate_priority, has_transport_sequence_number);
  } else {
    stream->RemoveBitrateObserver();
  }
}

void AudioSendStream::ConfigureBitrateObserver(int min_bitrate_bps,
                                               int max_bitrate_bps,
                                               double bitrate_priority,
                                               bool has_packet_feedback) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK_GE(max_bitrate_bps, min_bitrate_bps);
  rtc::Event thread_sync_event(false /* manual_reset */, false);
  worker_queue_->PostTask([&] {
    // We may get a callback immediately as the observer is registered, so make
    // sure the bitrate limits in config_ are up-to-date.
    config_.min_bitrate_bps = min_bitrate_bps;
    config_.max_bitrate_bps = max_bitrate_bps;
    config_.bitrate_priority = bitrate_priority;
    // This either updates the current observer or adds a new observer.
    bitrate_allocator_->AddObserver(
        this, MediaStreamAllocationConfig{
                  static_cast<uint32_t>(min_bitrate_bps),
                  static_cast<uint32_t>(max_bitrate_bps), 0, true,
                  config_.track_id, bitrate_priority, has_packet_feedback});
    thread_sync_event.Set();
  });
  thread_sync_event.Wait(rtc::Event::kForever);
}

void AudioSendStream::RemoveBitrateObserver() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::Event thread_sync_event(false /* manual_reset */, false);
  worker_queue_->PostTask([this, &thread_sync_event] {
    bitrate_allocator_->RemoveObserver(this);
    thread_sync_event.Set();
  });
  thread_sync_event.Wait(rtc::Event::kForever);
}

void AudioSendStream::RegisterCngPayloadType(int payload_type,
                                             int clockrate_hz) {
  const CodecInst codec = {payload_type, "CN", clockrate_hz, 0, 1, 0};
  if (rtp_rtcp_module_->RegisterSendPayload(codec) != 0) {
    rtp_rtcp_module_->DeRegisterSendPayload(codec.pltype);
    if (rtp_rtcp_module_->RegisterSendPayload(codec) != 0) {
      RTC_DLOG(LS_ERROR) << "RegisterCngPayloadType() failed to register CN to "
                            "RTP/RTCP module";
    }
  }
}
}  // namespace internal
}  // namespace webrtc
