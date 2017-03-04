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

#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/event.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/voice_engine/channel_proxy.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

namespace {

constexpr char kOpusCodecName[] = "opus";

bool IsCodec(const webrtc::CodecInst& codec, const char* ref_name) {
  return (STR_CASE_CMP(codec.plname, ref_name) == 0);
}
}  // namespace

namespace internal {
AudioSendStream::AudioSendStream(
    const webrtc::AudioSendStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    rtc::TaskQueue* worker_queue,
    PacketRouter* packet_router,
    CongestionController* congestion_controller,
    BitrateAllocator* bitrate_allocator,
    RtcEventLog* event_log,
    RtcpRttStats* rtcp_rtt_stats)
    : worker_queue_(worker_queue),
      config_(config),
      audio_state_(audio_state),
      bitrate_allocator_(bitrate_allocator),
      congestion_controller_(congestion_controller) {
  LOG(LS_INFO) << "AudioSendStream: " << config_.ToString();
  RTC_DCHECK_NE(config_.voe_channel_id, -1);
  RTC_DCHECK(audio_state_.get());
  RTC_DCHECK(congestion_controller);

  VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine());
  channel_proxy_ = voe_impl->GetChannelProxy(config_.voe_channel_id);
  channel_proxy_->SetRtcEventLog(event_log);
  channel_proxy_->SetRtcpRttStats(rtcp_rtt_stats);
  channel_proxy_->SetRTCPStatus(true);
  channel_proxy_->SetLocalSSRC(config.rtp.ssrc);
  channel_proxy_->SetRTCP_CNAME(config.rtp.c_name);
  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  channel_proxy_->SetNACKStatus(config_.rtp.nack.rtp_history_ms != 0,
                                config_.rtp.nack.rtp_history_ms / 20);

  channel_proxy_->RegisterExternalTransport(config.send_transport);

  for (const auto& extension : config.rtp.extensions) {
    if (extension.uri == RtpExtension::kAudioLevelUri) {
      channel_proxy_->SetSendAudioLevelIndicationStatus(true, extension.id);
    } else if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
      channel_proxy_->EnableSendTransportSequenceNumber(extension.id);
      congestion_controller->EnablePeriodicAlrProbing(true);
      bandwidth_observer_.reset(congestion_controller->GetBitrateController()
                                    ->CreateRtcpBandwidthObserver());
    } else {
      RTC_NOTREACHED() << "Registering unsupported RTP extension.";
    }
  }
  channel_proxy_->RegisterSenderCongestionControlObjects(
      congestion_controller->pacer(),
      congestion_controller->GetTransportFeedbackObserver(), packet_router,
      bandwidth_observer_.get());
  if (!SetupSendCodec()) {
    LOG(LS_ERROR) << "Failed to set up send codec state.";
  }
}

AudioSendStream::~AudioSendStream() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "~AudioSendStream: " << config_.ToString();
  channel_proxy_->DeRegisterExternalTransport();
  channel_proxy_->ResetCongestionControlObjects();
  channel_proxy_->SetRtcEventLog(nullptr);
  channel_proxy_->SetRtcpRttStats(nullptr);
}

void AudioSendStream::Start() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (config_.min_bitrate_bps != -1 && config_.max_bitrate_bps != -1) {
    RTC_DCHECK_GE(config_.max_bitrate_bps, config_.min_bitrate_bps);
    rtc::Event thread_sync_event(false /* manual_reset */, false);
    worker_queue_->PostTask([this, &thread_sync_event] {
      bitrate_allocator_->AddObserver(this, config_.min_bitrate_bps,
                                      config_.max_bitrate_bps, 0, true);
      thread_sync_event.Set();
    });
    thread_sync_event.Wait(rtc::Event::kForever);
  }

  ScopedVoEInterface<VoEBase> base(voice_engine());
  int error = base->StartSend(config_.voe_channel_id);
  if (error != 0) {
    LOG(LS_ERROR) << "AudioSendStream::Start failed with error: " << error;
  }
}

void AudioSendStream::Stop() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  rtc::Event thread_sync_event(false /* manual_reset */, false);
  worker_queue_->PostTask([this, &thread_sync_event] {
    bitrate_allocator_->RemoveObserver(this);
    thread_sync_event.Set();
  });
  thread_sync_event.Wait(rtc::Event::kForever);

  ScopedVoEInterface<VoEBase> base(voice_engine());
  int error = base->StopSend(config_.voe_channel_id);
  if (error != 0) {
    LOG(LS_ERROR) << "AudioSendStream::Stop failed with error: " << error;
  }
}

bool AudioSendStream::SendTelephoneEvent(int payload_type,
                                         int payload_frequency, int event,
                                         int duration_ms) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel_proxy_->SetSendTelephoneEventPayloadType(payload_type,
                                                          payload_frequency) &&
         channel_proxy_->SendTelephoneEventOutband(event, duration_ms);
}

void AudioSendStream::SetMuted(bool muted) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel_proxy_->SetInputMute(muted);
}

webrtc::AudioSendStream::Stats AudioSendStream::GetStats() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
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

  webrtc::CodecInst codec_inst = {0};
  if (channel_proxy_->GetSendCodec(&codec_inst)) {
    RTC_DCHECK_NE(codec_inst.pltype, -1);
    stats.codec_name = codec_inst.plname;
    stats.codec_payload_type = rtc::Optional<int>(codec_inst.pltype);

    // Get data from the last remote RTCP report.
    for (const auto& block : channel_proxy_->GetRemoteRTCPReportBlocks()) {
      // Lookup report for send ssrc only.
      if (block.source_SSRC == stats.local_ssrc) {
        stats.packets_lost = block.cumulative_num_packets_lost;
        stats.fraction_lost = Q8ToFloat(block.fraction_lost);
        stats.ext_seqnum = block.extended_highest_sequence_number;
        // Convert samples to milliseconds.
        if (codec_inst.plfreq / 1000 > 0) {
          stats.jitter_ms =
              block.interarrival_jitter / (codec_inst.plfreq / 1000);
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
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

bool AudioSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  return channel_proxy_->ReceivedRTCPPacket(packet, length);
}

uint32_t AudioSendStream::OnBitrateUpdated(uint32_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt,
                                           int64_t probing_interval_ms) {
  RTC_DCHECK_GE(bitrate_bps,
                static_cast<uint32_t>(config_.min_bitrate_bps));
  // The bitrate allocator might allocate an higher than max configured bitrate
  // if there is room, to allow for, as example, extra FEC. Ignore that for now.
  const uint32_t max_bitrate_bps = config_.max_bitrate_bps;
  if (bitrate_bps > max_bitrate_bps)
    bitrate_bps = max_bitrate_bps;

  channel_proxy_->SetBitrate(bitrate_bps, probing_interval_ms);

  // The amount of audio protection is not exposed by the encoder, hence
  // always returning 0.
  return 0;
}

const webrtc::AudioSendStream::Config& AudioSendStream::config() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

void AudioSendStream::SetTransportOverhead(int transport_overhead_per_packet) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  congestion_controller_->SetTransportOverhead(transport_overhead_per_packet);
  channel_proxy_->SetTransportOverhead(transport_overhead_per_packet);
}

VoiceEngine* AudioSendStream::voice_engine() const {
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  VoiceEngine* voice_engine = audio_state->voice_engine();
  RTC_DCHECK(voice_engine);
  return voice_engine;
}

// Apply current codec settings to a single voe::Channel used for sending.
bool AudioSendStream::SetupSendCodec() {
  // Disable VAD and FEC unless we know the other side wants them.
  channel_proxy_->SetVADStatus(false);
  channel_proxy_->SetCodecFECStatus(false);

  // We disable audio network adaptor here. This will on one hand make sure that
  // audio network adaptor is disabled by default, and on the other allow audio
  // network adaptor to be reconfigured, since SetReceiverFrameLengthRange can
  // be only called when audio network adaptor is disabled.
  channel_proxy_->DisableAudioNetworkAdaptor();

  const auto& send_codec_spec = config_.send_codec_spec;

  // We set the codec first, since the below extra configuration is only applied
  // to the "current" codec.

  // If codec is already configured, we do not it again.
  // TODO(minyue): check if this check is really needed, or can we move it into
  // |codec->SetSendCodec|.
  webrtc::CodecInst current_codec = {0};
  if (!channel_proxy_->GetSendCodec(&current_codec) ||
      (send_codec_spec.codec_inst != current_codec)) {
    if (!channel_proxy_->SetSendCodec(send_codec_spec.codec_inst)) {
      LOG(LS_WARNING) << "SetSendCodec() failed.";
      return false;
    }
  }

  // Codec internal FEC. Treat any failure as fatal internal error.
  if (send_codec_spec.enable_codec_fec) {
    if (!channel_proxy_->SetCodecFECStatus(true)) {
      LOG(LS_WARNING) << "SetCodecFECStatus() failed.";
      return false;
    }
  }

  // DTX and maxplaybackrate are only set if current codec is Opus.
  if (IsCodec(send_codec_spec.codec_inst, kOpusCodecName)) {
    if (!channel_proxy_->SetOpusDtx(send_codec_spec.enable_opus_dtx)) {
      LOG(LS_WARNING) << "SetOpusDtx() failed.";
      return false;
    }

    // If opus_max_playback_rate <= 0, the default maximum playback rate
    // (48 kHz) will be used.
    if (send_codec_spec.opus_max_playback_rate > 0) {
      if (!channel_proxy_->SetOpusMaxPlaybackRate(
              send_codec_spec.opus_max_playback_rate)) {
        LOG(LS_WARNING) << "SetOpusMaxPlaybackRate() failed.";
        return false;
      }
    }

    if (config_.audio_network_adaptor_config) {
      // Audio network adaptor is only allowed for Opus currently.
      // |SetReceiverFrameLengthRange| needs to be called before
      // |EnableAudioNetworkAdaptor|.
      channel_proxy_->SetReceiverFrameLengthRange(send_codec_spec.min_ptime_ms,
                                                  send_codec_spec.max_ptime_ms);
      channel_proxy_->EnableAudioNetworkAdaptor(
          *config_.audio_network_adaptor_config);
      LOG(LS_INFO) << "Audio network adaptor enabled on SSRC "
                   << config_.rtp.ssrc;
    }
  }

  // Set the CN payloadtype and the VAD status.
  if (send_codec_spec.cng_payload_type != -1) {
    // The CN payload type for 8000 Hz clockrate is fixed at 13.
    if (send_codec_spec.cng_plfreq != 8000) {
      webrtc::PayloadFrequencies cn_freq;
      switch (send_codec_spec.cng_plfreq) {
        case 16000:
          cn_freq = webrtc::kFreq16000Hz;
          break;
        case 32000:
          cn_freq = webrtc::kFreq32000Hz;
          break;
        default:
          RTC_NOTREACHED();
          return false;
      }
      if (!channel_proxy_->SetSendCNPayloadType(
          send_codec_spec.cng_payload_type, cn_freq)) {
        LOG(LS_WARNING) << "SetSendCNPayloadType() failed.";
        // TODO(ajm): This failure condition will be removed from VoE.
        // Restore the return here when we update to a new enough webrtc.
        //
        // Not returning false because the SetSendCNPayloadType will fail if
        // the channel is already sending.
        // This can happen if the remote description is applied twice, for
        // example in the case of ROAP on top of JSEP, where both side will
        // send the offer.
      }
    }

    // Only turn on VAD if we have a CN payload type that matches the
    // clockrate for the codec we are going to use.
    if (send_codec_spec.cng_plfreq == send_codec_spec.codec_inst.plfreq &&
        send_codec_spec.codec_inst.channels == 1) {
      // TODO(minyue): If CN frequency == 48000 Hz is allowed, consider the
      // interaction between VAD and Opus FEC.
      if (!channel_proxy_->SetVADStatus(true)) {
        LOG(LS_WARNING) << "SetVADStatus() failed.";
        return false;
      }
    }
  }
  return true;
}

}  // namespace internal
}  // namespace webrtc
