/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_send_stream.h"

#include <string>
#include <utility>
#include <vector>

#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/event.h"
#include "webrtc/base/function_view.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/call/rtp_transport_controller_send_interface.h"
#include "webrtc/modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/include/send_side_congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/voice_engine/channel_proxy.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

namespace internal {
// TODO(eladalon): Subsequent CL will make these values experiment-dependent.
constexpr size_t kPacketLossTrackerMaxWindowSizeMs = 15000;
constexpr size_t kPacketLossRateMinNumAckedPackets = 50;
constexpr size_t kRecoverablePacketLossRateMinNumAckedPairs = 40;

namespace {
void CallEncoder(const std::unique_ptr<voe::ChannelProxy>& channel_proxy,
                 rtc::FunctionView<void(AudioEncoder*)> lambda) {
  channel_proxy->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder_ptr) {
    RTC_DCHECK(encoder_ptr);
    lambda(encoder_ptr->get());
  });
}
}  // namespace

AudioSendStream::AudioSendStream(
    const webrtc::AudioSendStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    rtc::TaskQueue* worker_queue,
    RtpTransportControllerSendInterface* transport,
    BitrateAllocator* bitrate_allocator,
    RtcEventLog* event_log,
    RtcpRttStats* rtcp_rtt_stats,
    const rtc::Optional<RtpState>& suspended_rtp_state)
    : worker_queue_(worker_queue),
      config_(Config(nullptr)),
      audio_state_(audio_state),
      event_log_(event_log),
      bitrate_allocator_(bitrate_allocator),
      transport_(transport),
      packet_loss_tracker_(kPacketLossTrackerMaxWindowSizeMs,
                           kPacketLossRateMinNumAckedPackets,
                           kRecoverablePacketLossRateMinNumAckedPairs),
      rtp_rtcp_module_(nullptr),
      suspended_rtp_state_(suspended_rtp_state) {
  LOG(LS_INFO) << "AudioSendStream: " << config.ToString();
  RTC_DCHECK_NE(config.voe_channel_id, -1);
  RTC_DCHECK(audio_state_.get());
  RTC_DCHECK(transport);
  RTC_DCHECK(transport->send_side_cc());

  VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine());
  channel_proxy_ = voe_impl->GetChannelProxy(config.voe_channel_id);
  channel_proxy_->SetRtcEventLog(event_log_);
  channel_proxy_->SetRtcpRttStats(rtcp_rtt_stats);
  channel_proxy_->SetRTCPStatus(true);
  transport_->send_side_cc()->RegisterPacketFeedbackObserver(this);
  RtpReceiver* rtpReceiver = nullptr;  // Unused, but required for call.
  channel_proxy_->GetRtpRtcp(&rtp_rtcp_module_, &rtpReceiver);
  RTC_DCHECK(rtp_rtcp_module_);

  ConfigureStream(this, config, true);

  pacer_thread_checker_.DetachFromThread();
}

AudioSendStream::~AudioSendStream() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "~AudioSendStream: " << config_.ToString();
  transport_->send_side_cc()->DeRegisterPacketFeedbackObserver(this);
  channel_proxy_->DeRegisterExternalTransport();
  channel_proxy_->ResetSenderCongestionControlObjects();
  channel_proxy_->SetRtcEventLog(nullptr);
  channel_proxy_->SetRtcpRttStats(nullptr);
}

void AudioSendStream::Reconfigure(
    const webrtc::AudioSendStream::Config& new_config) {
  ConfigureStream(this, new_config, false);
}

void AudioSendStream::ConfigureStream(
    webrtc::internal::AudioSendStream* stream,
    const webrtc::AudioSendStream::Config& new_config,
    bool first_time) {
  LOG(LS_INFO) << "AudioSendStream::Configuring: " << new_config.ToString();
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

  if (first_time ||
      new_config.send_transport != old_config.send_transport) {
    if (old_config.send_transport) {
      channel_proxy->DeRegisterExternalTransport();
    }

    channel_proxy->RegisterExternalTransport(new_config.send_transport);
  }

  // RFC 5285: Each distinct extension MUST have a unique ID. The value 0 is
  // reserved for padding and MUST NOT be used as a local identifier.
  // So it should be safe to use 0 here to indicate "not configured".
  struct ExtensionIds {
    int audio_level = 0;
    int transport_sequence_number = 0;
  };

  auto find_extension_ids = [](const std::vector<RtpExtension>& extensions) {
    ExtensionIds ids;
    for (const auto& extension : extensions) {
      if (extension.uri == RtpExtension::kAudioLevelUri) {
        ids.audio_level = extension.id;
      } else if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
        ids.transport_sequence_number = extension.id;
      }
    }
    return ids;
  };

  const ExtensionIds old_ids = find_extension_ids(old_config.rtp.extensions);
  const ExtensionIds new_ids = find_extension_ids(new_config.rtp.extensions);
  // Audio level indication
  if (first_time || new_ids.audio_level != old_ids.audio_level) {
    channel_proxy->SetSendAudioLevelIndicationStatus(new_ids.audio_level != 0,
                                                     new_ids.audio_level);
  }
  // Transport sequence number
  if (first_time ||
      new_ids.transport_sequence_number != old_ids.transport_sequence_number) {
    if (old_ids.transport_sequence_number) {
      channel_proxy->ResetSenderCongestionControlObjects();
      stream->bandwidth_observer_.reset();
    }

    if (new_ids.transport_sequence_number != 0) {
      channel_proxy->EnableSendTransportSequenceNumber(
          new_ids.transport_sequence_number);
      stream->transport_->send_side_cc()->EnablePeriodicAlrProbing(true);
      stream->bandwidth_observer_.reset(stream->transport_->send_side_cc()
                                            ->GetBitrateController()
                                            ->CreateRtcpBandwidthObserver());
    }

    channel_proxy->RegisterSenderCongestionControlObjects(
        stream->transport_, stream->bandwidth_observer_.get());
  }

  if (!ReconfigureSendCodec(stream, new_config)) {
    LOG(LS_ERROR) << "Failed to set up send codec state.";
  }

  ReconfigureBitrateObserver(stream, new_config);
  stream->config_ = new_config;
}

void AudioSendStream::Start() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  if (config_.min_bitrate_bps != -1 && config_.max_bitrate_bps != -1) {
    ConfigureBitrateObserver(config_.min_bitrate_bps, config_.max_bitrate_bps);
  }

  ScopedVoEInterface<VoEBase> base(voice_engine());
  int error = base->StartSend(config_.voe_channel_id);
  if (error != 0) {
    LOG(LS_ERROR) << "AudioSendStream::Start failed with error: " << error;
  }
}

void AudioSendStream::Stop() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RemoveBitrateObserver();

  ScopedVoEInterface<VoEBase> base(voice_engine());
  int error = base->StopSend(config_.voe_channel_id);
  if (error != 0) {
    LOG(LS_ERROR) << "AudioSendStream::Stop failed with error: " << error;
  }
}

bool AudioSendStream::SendTelephoneEvent(int payload_type,
                                         int payload_frequency, int event,
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
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  webrtc::AudioSendStream::Stats stats;
  stats.local_ssrc = config_.rtp.ssrc;

  webrtc::CallStatistics call_stats = channel_proxy_->GetRTCPStatistics();
  stats.bytes_sent = call_stats.bytesSent;
  stats.packets_sent = call_stats.packetsSent;
  // RTT isn't known until a RTCP report is received. Until then, VoiceEngine
  // returns 0 to indicate an error value.
  if (call_stats.rttMs > 0) {
    stats.rtt_ms = call_stats.rttMs;
  }
  // TODO(solenberg): [was ajm]: Re-enable this metric once we have a reliable
  //                  implementation.
  stats.aec_quality_min = -1;

  if (config_.send_codec_spec) {
    const auto& spec = *config_.send_codec_spec;
    stats.codec_name = spec.format.name;
    stats.codec_payload_type = rtc::Optional<int>(spec.payload_type);

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

  ScopedVoEInterface<VoEBase> base(voice_engine());
  RTC_DCHECK(base->transmit_mixer());
  stats.audio_level = base->transmit_mixer()->AudioLevelFullRange();
  RTC_DCHECK_LE(0, stats.audio_level);

  RTC_DCHECK(base->audio_processing());
  auto audio_processing_stats = base->audio_processing()->GetStatistics();
  stats.echo_delay_median_ms = audio_processing_stats.delay_median;
  stats.echo_delay_std_ms = audio_processing_stats.delay_standard_deviation;
  stats.echo_return_loss = audio_processing_stats.echo_return_loss.instant();
  stats.echo_return_loss_enhancement =
      audio_processing_stats.echo_return_loss_enhancement.instant();
  stats.residual_echo_likelihood =
      audio_processing_stats.residual_echo_likelihood;
  stats.residual_echo_likelihood_recent_max =
      audio_processing_stats.residual_echo_likelihood_recent_max;

  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  stats.typing_noise_detected = audio_state->typing_noise_detected();

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
  RTC_DCHECK_GE(bitrate_bps,
                static_cast<uint32_t>(config_.min_bitrate_bps));
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
  // TODO(eladalon): This fails in UT; fix and uncomment.
  // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7405
  // RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::Optional<float> plr;
  rtc::Optional<float> rplr;
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

const webrtc::AudioSendStream::Config& AudioSendStream::config() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return config_;
}

void AudioSendStream::SetTransportOverhead(int transport_overhead_per_packet) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  transport_->send_side_cc()->SetTransportOverhead(
      transport_overhead_per_packet);
  channel_proxy_->SetTransportOverhead(transport_overhead_per_packet);
}

RtpState AudioSendStream::GetRtpState() const {
  return rtp_rtcp_module_->GetRtpState();
}

VoiceEngine* AudioSendStream::voice_engine() const {
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  VoiceEngine* voice_engine = audio_state->voice_engine();
  RTC_DCHECK(voice_engine);
  return voice_engine;
}

// Apply current codec settings to a single voe::Channel used for sending.
bool AudioSendStream::SetupSendCodec(AudioSendStream* stream,
                                     const Config& new_config) {
  RTC_DCHECK(new_config.send_codec_spec);
  const auto& spec = *new_config.send_codec_spec;

  RTC_DCHECK(new_config.encoder_factory);
  std::unique_ptr<AudioEncoder> encoder =
      new_config.encoder_factory->MakeAudioEncoder(spec.payload_type,
                                                   spec.format);

  if (!encoder) {
    LOG(LS_ERROR) << "Unable to create encoder for " << spec.format;
    return false;
  }
  // If a bitrate has been specified for the codec, use it over the
  // codec's default.
  if (spec.target_bitrate_bps) {
    encoder->OnReceivedTargetAudioBitrate(*spec.target_bitrate_bps);
  }

  // Enable ANA if configured (currently only used by Opus).
  if (new_config.audio_network_adaptor_config) {
    if (encoder->EnableAudioNetworkAdaptor(
            *new_config.audio_network_adaptor_config, stream->event_log_)) {
      LOG(LS_INFO) << "Audio network adaptor enabled on SSRC "
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

  stream->channel_proxy_->SetEncoder(new_config.send_codec_spec->payload_type,
                                     std::move(encoder));
  return true;
}

bool AudioSendStream::ReconfigureSendCodec(AudioSendStream* stream,
                                           const Config& new_config) {
  const auto& old_config = stream->config_;
  if (new_config.send_codec_spec == old_config.send_codec_spec) {
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

  // Should never move a stream from fully configured to unconfigured.
  RTC_CHECK(new_config.send_codec_spec);

  const rtc::Optional<int>& new_target_bitrate_bps =
      new_config.send_codec_spec->target_bitrate_bps;
  // If a bitrate has been specified for the codec, use it over the
  // codec's default.
  if (new_target_bitrate_bps &&
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
        LOG(LS_INFO) << "Audio network adaptor enabled on SSRC "
                     << new_config.rtp.ssrc;
      } else {
        RTC_NOTREACHED();
      }
    });
  } else {
    CallEncoder(stream->channel_proxy_, [&](AudioEncoder* encoder) {
      encoder->DisableAudioNetworkAdaptor();
    });
    LOG(LS_INFO) << "Audio network adaptor disabled on SSRC "
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
  if (stream->config_.min_bitrate_bps == new_config.min_bitrate_bps &&
      stream->config_.max_bitrate_bps == new_config.max_bitrate_bps) {
    return;
  }

  if (new_config.min_bitrate_bps != -1 && new_config.max_bitrate_bps != -1) {
    stream->ConfigureBitrateObserver(new_config.min_bitrate_bps,
                                     new_config.max_bitrate_bps);
  } else {
    stream->RemoveBitrateObserver();
  }
}

void AudioSendStream::ConfigureBitrateObserver(int min_bitrate_bps,
                                               int max_bitrate_bps) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK_GE(max_bitrate_bps, min_bitrate_bps);
  rtc::Event thread_sync_event(false /* manual_reset */, false);
  worker_queue_->PostTask([&] {
    // We may get a callback immediately as the observer is registered, so make
    // sure the bitrate limits in config_ are up-to-date.
    config_.min_bitrate_bps = min_bitrate_bps;
    config_.max_bitrate_bps = max_bitrate_bps;
    bitrate_allocator_->AddObserver(this, min_bitrate_bps, max_bitrate_bps, 0,
                                    true);
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
      LOG(LS_ERROR) << "RegisterCngPayloadType() failed to register CN to "
                       "RTP/RTCP module";
    }
  }
}


}  // namespace internal
}  // namespace webrtc
