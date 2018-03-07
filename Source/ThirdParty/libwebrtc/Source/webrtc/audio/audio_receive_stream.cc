/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_receive_stream.h"

#include <string>
#include <utility>

#include "api/call/audio_sink.h"
#include "audio/audio_send_stream.h"
#include "audio/audio_state.h"
#include "audio/conversion.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/rtp_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "voice_engine/channel_proxy.h"
#include "voice_engine/include/voe_base.h"
#include "voice_engine/voice_engine_impl.h"

namespace webrtc {

std::string AudioReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", transport_cc: " << (transport_cc ? "on" : "off");
  ss << ", nack: " << nack.ToString();
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", rtcp_send_transport: "
     << (rtcp_send_transport ? "(Transport)" : "null");
  ss << ", voe_channel_id: " << voe_channel_id;
  if (!sync_group.empty()) {
    ss << ", sync_group: " << sync_group;
  }
  ss << '}';
  return ss.str();
}

namespace internal {
AudioReceiveStream::AudioReceiveStream(
    RtpStreamReceiverControllerInterface* receiver_controller,
    PacketRouter* packet_router,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    webrtc::RtcEventLog* event_log)
    : config_(config), audio_state_(audio_state) {
  RTC_LOG(LS_INFO) << "AudioReceiveStream: " << config_.ToString();
  RTC_DCHECK_NE(config_.voe_channel_id, -1);
  RTC_DCHECK(audio_state_.get());
  RTC_DCHECK(packet_router);

  module_process_thread_checker_.DetachFromThread();

  VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine());
  channel_proxy_ = voe_impl->GetChannelProxy(config_.voe_channel_id);
  channel_proxy_->SetRtcEventLog(event_log);
  channel_proxy_->SetLocalSSRC(config.rtp.local_ssrc);
  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  channel_proxy_->SetNACKStatus(config_.rtp.nack.rtp_history_ms != 0,
                                config_.rtp.nack.rtp_history_ms / 20);

  // TODO(ossu): This is where we'd like to set the decoder factory to
  // use. However, since it needs to be included when constructing Channel, we
  // cannot do that until we're able to move Channel ownership into the
  // Audio{Send,Receive}Streams.  The best we can do is check that we're not
  // trying to use two different factories using the different interfaces.
  RTC_CHECK(config.decoder_factory);
  RTC_CHECK_EQ(config.decoder_factory,
               channel_proxy_->GetAudioDecoderFactory());

  channel_proxy_->RegisterTransport(config.rtcp_send_transport);
  channel_proxy_->SetReceiveCodecs(config.decoder_map);

  for (const auto& extension : config.rtp.extensions) {
    if (extension.uri == RtpExtension::kAudioLevelUri) {
      channel_proxy_->SetReceiveAudioLevelIndicationStatus(true, extension.id);
    } else if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
      channel_proxy_->EnableReceiveTransportSequenceNumber(extension.id);
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }
  // Configure bandwidth estimation.
  channel_proxy_->RegisterReceiverCongestionControlObjects(packet_router);

  // Register with transport.
  rtp_stream_receiver_ =
      receiver_controller->CreateReceiver(config_.rtp.remote_ssrc,
                                          channel_proxy_.get());
}

AudioReceiveStream::~AudioReceiveStream() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "~AudioReceiveStream: " << config_.ToString();
  if (playing_) {
    Stop();
  }
  channel_proxy_->DisassociateSendChannel();
  channel_proxy_->RegisterTransport(nullptr);
  channel_proxy_->ResetReceiverCongestionControlObjects();
  channel_proxy_->SetRtcEventLog(nullptr);
}

void AudioReceiveStream::Start() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (playing_) {
    return;
  }

  int error = SetVoiceEnginePlayout(true);
  if (error != 0) {
    RTC_LOG(LS_ERROR) << "AudioReceiveStream::Start failed with error: "
                      << error;
    return;
  }

  if (!audio_state()->mixer()->AddSource(this)) {
    RTC_LOG(LS_ERROR) << "Failed to add source to mixer.";
    SetVoiceEnginePlayout(false);
    return;
  }

  playing_ = true;
}

void AudioReceiveStream::Stop() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playing_) {
    return;
  }
  playing_ = false;

  audio_state()->mixer()->RemoveSource(this);
  SetVoiceEnginePlayout(false);
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  webrtc::AudioReceiveStream::Stats stats;
  stats.remote_ssrc = config_.rtp.remote_ssrc;

  webrtc::CallStatistics call_stats = channel_proxy_->GetRTCPStatistics();
  // TODO(solenberg): Don't return here if we can't get the codec - return the
  //                  stats we *can* get.
  webrtc::CodecInst codec_inst = {0};
  if (!channel_proxy_->GetRecCodec(&codec_inst)) {
    return stats;
  }

  stats.bytes_rcvd = call_stats.bytesReceived;
  stats.packets_rcvd = call_stats.packetsReceived;
  stats.packets_lost = call_stats.cumulativeLost;
  stats.fraction_lost = Q8ToFloat(call_stats.fractionLost);
  stats.capture_start_ntp_time_ms = call_stats.capture_start_ntp_time_ms_;
  if (codec_inst.pltype != -1) {
    stats.codec_name = codec_inst.plname;
    stats.codec_payload_type = codec_inst.pltype;
  }
  stats.ext_seqnum = call_stats.extendedMax;
  if (codec_inst.plfreq / 1000 > 0) {
    stats.jitter_ms = call_stats.jitterSamples / (codec_inst.plfreq / 1000);
  }
  stats.delay_estimate_ms = channel_proxy_->GetDelayEstimate();
  stats.audio_level = channel_proxy_->GetSpeechOutputLevelFullRange();
  stats.total_output_energy = channel_proxy_->GetTotalOutputEnergy();
  stats.total_output_duration = channel_proxy_->GetTotalOutputDuration();

  // Get jitter buffer and total delay (alg + jitter + playout) stats.
  auto ns = channel_proxy_->GetNetworkStatistics();
  stats.jitter_buffer_ms = ns.currentBufferSize;
  stats.jitter_buffer_preferred_ms = ns.preferredBufferSize;
  stats.total_samples_received = ns.totalSamplesReceived;
  stats.concealed_samples = ns.concealedSamples;
  stats.concealment_events = ns.concealmentEvents;
  stats.jitter_buffer_delay_seconds =
      static_cast<double>(ns.jitterBufferDelayMs) /
      static_cast<double>(rtc::kNumMillisecsPerSec);
  stats.expand_rate = Q14ToFloat(ns.currentExpandRate);
  stats.speech_expand_rate = Q14ToFloat(ns.currentSpeechExpandRate);
  stats.secondary_decoded_rate = Q14ToFloat(ns.currentSecondaryDecodedRate);
  stats.secondary_discarded_rate = Q14ToFloat(ns.currentSecondaryDiscardedRate);
  stats.accelerate_rate = Q14ToFloat(ns.currentAccelerateRate);
  stats.preemptive_expand_rate = Q14ToFloat(ns.currentPreemptiveRate);

  auto ds = channel_proxy_->GetDecodingCallStatistics();
  stats.decoding_calls_to_silence_generator = ds.calls_to_silence_generator;
  stats.decoding_calls_to_neteq = ds.calls_to_neteq;
  stats.decoding_normal = ds.decoded_normal;
  stats.decoding_plc = ds.decoded_plc;
  stats.decoding_cng = ds.decoded_cng;
  stats.decoding_plc_cng = ds.decoded_plc_cng;
  stats.decoding_muted_output = ds.decoded_muted_output;

  return stats;
}

int AudioReceiveStream::GetOutputLevel() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return channel_proxy_->GetSpeechOutputLevel();
}

void AudioReceiveStream::SetSink(std::unique_ptr<AudioSinkInterface> sink) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  channel_proxy_->SetSink(std::move(sink));
}

void AudioReceiveStream::SetGain(float gain) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  channel_proxy_->SetChannelOutputVolumeScaling(gain);
}

std::vector<RtpSource> AudioReceiveStream::GetSources() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return channel_proxy_->GetSources();
}

AudioMixer::Source::AudioFrameInfo AudioReceiveStream::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  return channel_proxy_->GetAudioFrameWithInfo(sample_rate_hz, audio_frame);
}

int AudioReceiveStream::Ssrc() const {
  return config_.rtp.remote_ssrc;
}

int AudioReceiveStream::PreferredSampleRate() const {
  return channel_proxy_->PreferredSampleRate();
}

int AudioReceiveStream::id() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_.rtp.remote_ssrc;
}

rtc::Optional<Syncable::Info> AudioReceiveStream::GetInfo() const {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  Syncable::Info info;

  RtpRtcp* rtp_rtcp = nullptr;
  RtpReceiver* rtp_receiver = nullptr;
  channel_proxy_->GetRtpRtcp(&rtp_rtcp, &rtp_receiver);
  RTC_DCHECK(rtp_rtcp);
  RTC_DCHECK(rtp_receiver);

  if (!rtp_receiver->GetLatestTimestamps(
          &info.latest_received_capture_timestamp,
          &info.latest_receive_time_ms)) {
    return rtc::nullopt;
  }
  if (rtp_rtcp->RemoteNTP(&info.capture_time_ntp_secs,
                          &info.capture_time_ntp_frac,
                          nullptr,
                          nullptr,
                          &info.capture_time_source_clock) != 0) {
    return rtc::nullopt;
  }

  info.current_delay_ms = channel_proxy_->GetDelayEstimate();
  return info;
}

uint32_t AudioReceiveStream::GetPlayoutTimestamp() const {
  // Called on video capture thread.
  return channel_proxy_->GetPlayoutTimestamp();
}

void AudioReceiveStream::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  return channel_proxy_->SetMinimumPlayoutDelay(delay_ms);
}

void AudioReceiveStream::AssociateSendStream(AudioSendStream* send_stream) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (send_stream) {
    VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine());
    std::unique_ptr<voe::ChannelProxy> send_channel_proxy =
        voe_impl->GetChannelProxy(send_stream->GetConfig().voe_channel_id);
    channel_proxy_->AssociateSendChannel(*send_channel_proxy.get());
  } else {
    channel_proxy_->DisassociateSendChannel();
  }
}

void AudioReceiveStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
}

bool AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  return channel_proxy_->ReceivedRTCPPacket(packet, length);
}

void AudioReceiveStream::OnRtpPacket(const RtpPacketReceived& packet) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  channel_proxy_->OnRtpPacket(packet);
}

const webrtc::AudioReceiveStream::Config& AudioReceiveStream::config() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_;
}

VoiceEngine* AudioReceiveStream::voice_engine() const {
  auto* voice_engine = audio_state()->voice_engine();
  RTC_DCHECK(voice_engine);
  return voice_engine;
}

internal::AudioState* AudioReceiveStream::audio_state() const {
  auto* audio_state = static_cast<internal::AudioState*>(audio_state_.get());
  RTC_DCHECK(audio_state);
  return audio_state;
}

int AudioReceiveStream::SetVoiceEnginePlayout(bool playout) {
  ScopedVoEInterface<VoEBase> base(voice_engine());
  if (playout) {
    return base->StartPlayout(config_.voe_channel_id);
  } else {
    return base->StopPlayout(config_.voe_channel_id);
  }
}
}  // namespace internal
}  // namespace webrtc
