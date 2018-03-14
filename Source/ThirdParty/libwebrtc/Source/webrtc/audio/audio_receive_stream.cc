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
  if (!sync_group.empty()) {
    ss << ", sync_group: " << sync_group;
  }
  ss << '}';
  return ss.str();
}

namespace internal {
namespace {
std::unique_ptr<voe::ChannelProxy> CreateChannelAndProxy(
    webrtc::AudioState* audio_state,
    ProcessThread* module_process_thread,
    const webrtc::AudioReceiveStream::Config& config) {
  RTC_DCHECK(audio_state);
  internal::AudioState* internal_audio_state =
      static_cast<internal::AudioState*>(audio_state);
  return std::unique_ptr<voe::ChannelProxy>(new voe::ChannelProxy(
      std::unique_ptr<voe::Channel>(new voe::Channel(
              module_process_thread,
              internal_audio_state->audio_device_module(),
              config.jitter_buffer_max_packets,
              config.jitter_buffer_fast_accelerate,
              config.decoder_factory))));
}
}  // namespace

AudioReceiveStream::AudioReceiveStream(
    RtpStreamReceiverControllerInterface* receiver_controller,
    PacketRouter* packet_router,
    ProcessThread* module_process_thread,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    webrtc::RtcEventLog* event_log)
    : AudioReceiveStream(receiver_controller,
                         packet_router,
                         config,
                         audio_state,
                         event_log,
                         CreateChannelAndProxy(audio_state.get(),
                                               module_process_thread,
                                               config)) {}

AudioReceiveStream::AudioReceiveStream(
    RtpStreamReceiverControllerInterface* receiver_controller,
    PacketRouter* packet_router,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    webrtc::RtcEventLog* event_log,
    std::unique_ptr<voe::ChannelProxy> channel_proxy)
    : audio_state_(audio_state),
      channel_proxy_(std::move(channel_proxy)) {
  RTC_LOG(LS_INFO) << "AudioReceiveStream: " << config.ToString();
  RTC_DCHECK(receiver_controller);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(config.decoder_factory);
  RTC_DCHECK(audio_state_);
  RTC_DCHECK(channel_proxy_);

  module_process_thread_checker_.DetachFromThread();

  channel_proxy_->SetRtcEventLog(event_log);
  channel_proxy_->RegisterTransport(config.rtcp_send_transport);

  // Configure bandwidth estimation.
  channel_proxy_->RegisterReceiverCongestionControlObjects(packet_router);

  // Register with transport.
  rtp_stream_receiver_ =
      receiver_controller->CreateReceiver(config.rtp.remote_ssrc,
                                          channel_proxy_.get());

  ConfigureStream(this, config, true);
}

AudioReceiveStream::~AudioReceiveStream() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "~AudioReceiveStream: " << config_.ToString();
  Stop();
  channel_proxy_->DisassociateSendChannel();
  channel_proxy_->RegisterTransport(nullptr);
  channel_proxy_->ResetReceiverCongestionControlObjects();
  channel_proxy_->SetRtcEventLog(nullptr);
}

void AudioReceiveStream::Reconfigure(
    const webrtc::AudioReceiveStream::Config& config) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "AudioReceiveStream::Reconfigure: " << config_.ToString();
  ConfigureStream(this, config, false);
}

void AudioReceiveStream::Start() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (playing_) {
    return;
  }
  channel_proxy_->StartPlayout();
  playing_ = true;
  audio_state()->AddReceivingStream(this);
}

void AudioReceiveStream::Stop() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playing_) {
    return;
  }
  channel_proxy_->StopPlayout();
  playing_ = false;
  audio_state()->RemoveReceivingStream(this);
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

void AudioReceiveStream::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  channel_proxy_->SetSink(sink);
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
    channel_proxy_->AssociateSendChannel(send_stream->GetChannelProxy());
  } else {
    channel_proxy_->DisassociateSendChannel();
  }
  associated_send_stream_ = send_stream;
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

const AudioSendStream*
    AudioReceiveStream::GetAssociatedSendStreamForTesting() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return associated_send_stream_;
}

internal::AudioState* AudioReceiveStream::audio_state() const {
  auto* audio_state = static_cast<internal::AudioState*>(audio_state_.get());
  RTC_DCHECK(audio_state);
  return audio_state;
}

void AudioReceiveStream::ConfigureStream(AudioReceiveStream* stream,
                                         const Config& new_config,
                                         bool first_time) {
  RTC_DCHECK(stream);
  const auto& channel_proxy = stream->channel_proxy_;
  const auto& old_config = stream->config_;

  // Configuration parameters which cannot be changed.
  RTC_DCHECK(first_time ||
             old_config.rtp.remote_ssrc == new_config.rtp.remote_ssrc);
  RTC_DCHECK(first_time ||
             old_config.rtcp_send_transport == new_config.rtcp_send_transport);
  // Decoder factory cannot be changed because it is configured at
  // voe::Channel construction time.
  RTC_DCHECK(first_time ||
             old_config.decoder_factory == new_config.decoder_factory);

  if (first_time || old_config.rtp.local_ssrc != new_config.rtp.local_ssrc) {
    channel_proxy->SetLocalSSRC(new_config.rtp.local_ssrc);
  }
  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  if (first_time || old_config.rtp.nack.rtp_history_ms !=
                        new_config.rtp.nack.rtp_history_ms) {
    channel_proxy->SetNACKStatus(new_config.rtp.nack.rtp_history_ms != 0,
                                 new_config.rtp.nack.rtp_history_ms / 20);
  }
  if (first_time || old_config.decoder_map != new_config.decoder_map) {
    channel_proxy->SetReceiveCodecs(new_config.decoder_map);
  }

  stream->config_ = new_config;
}
}  // namespace internal
}  // namespace webrtc
