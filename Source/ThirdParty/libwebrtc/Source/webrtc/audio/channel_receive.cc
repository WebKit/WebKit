/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "audio/channel_send.h"
#include "audio/utility/audio_frame_operations.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace voe {

namespace {

constexpr double kAudioSampleDurationSeconds = 0.01;
constexpr int64_t kMaxRetransmissionWindowMs = 1000;
constexpr int64_t kMinRetransmissionWindowMs = 30;

// Video Sync.
constexpr int kVoiceEngineMinMinPlayoutDelayMs = 0;
constexpr int kVoiceEngineMaxMinPlayoutDelayMs = 10000;

}  // namespace

int32_t ChannelReceive::OnReceivedPayloadData(
    const uint8_t* payloadData,
    size_t payloadSize,
    const WebRtcRTPHeader* rtpHeader) {
  if (!channel_state_.Get().playing) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    return 0;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (audio_coding_->IncomingPacket(payloadData, payloadSize, *rtpHeader) !=
      0) {
    RTC_DLOG(LS_ERROR) << "ChannelReceive::OnReceivedPayloadData() unable to "
                          "push data to the ACM";
    return -1;
  }

  int64_t round_trip_time = 0;
  _rtpRtcpModule->RTT(remote_ssrc_, &round_trip_time, NULL, NULL, NULL);

  std::vector<uint16_t> nack_list = audio_coding_->GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Can't use nack_list.data() since it's not supported by all
    // compilers.
    ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
  return 0;
}

AudioMixer::Source::AudioFrameInfo ChannelReceive::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  unsigned int ssrc;
  RTC_CHECK_EQ(GetRemoteSSRC(ssrc), 0);
  event_log_->Log(absl::make_unique<RtcEventAudioPlayout>(ssrc));
  // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
  bool muted;
  if (audio_coding_->PlayoutData10Ms(audio_frame->sample_rate_hz_, audio_frame,
                                     &muted) == -1) {
    RTC_DLOG(LS_ERROR)
        << "ChannelReceive::GetAudioFrame() PlayoutData10Ms() failed!";
    // In all likelihood, the audio in this frame is garbage. We return an
    // error so that the audio mixer module doesn't add it to the mix. As
    // a result, it won't be played out and the actions skipped here are
    // irrelevant.
    return AudioMixer::Source::AudioFrameInfo::kError;
  }

  if (muted) {
    // TODO(henrik.lundin): We should be able to do better than this. But we
    // will have to go through all the cases below where the audio samples may
    // be used, and handle the muted case in some way.
    AudioFrameOperations::Mute(audio_frame);
  }

  {
    // Pass the audio buffers to an optional sink callback, before applying
    // scaling/panning, as that applies to the mix operation.
    // External recipients of the audio (e.g. via AudioTrack), will do their
    // own mixing/dynamic processing.
    rtc::CritScope cs(&_callbackCritSect);
    if (audio_sink_) {
      AudioSinkInterface::Data data(
          audio_frame->data(), audio_frame->samples_per_channel_,
          audio_frame->sample_rate_hz_, audio_frame->num_channels_,
          audio_frame->timestamp_);
      audio_sink_->OnData(data);
    }
  }

  float output_gain = 1.0f;
  {
    rtc::CritScope cs(&volume_settings_critsect_);
    output_gain = _outputGain;
  }

  // Output volume scaling
  if (output_gain < 0.99f || output_gain > 1.01f) {
    // TODO(solenberg): Combine with mute state - this can cause clicks!
    AudioFrameOperations::ScaleWithSat(output_gain, audio_frame);
  }

  // Measure audio level (0-9)
  // TODO(henrik.lundin) Use the |muted| information here too.
  // TODO(deadbeef): Use RmsLevel for |_outputAudioLevel| (see
  // https://crbug.com/webrtc/7517).
  _outputAudioLevel.ComputeLevel(*audio_frame, kAudioSampleDurationSeconds);

  if (capture_start_rtp_time_stamp_ < 0 && audio_frame->timestamp_ != 0) {
    // The first frame with a valid rtp timestamp.
    capture_start_rtp_time_stamp_ = audio_frame->timestamp_;
  }

  if (capture_start_rtp_time_stamp_ >= 0) {
    // audio_frame.timestamp_ should be valid from now on.

    // Compute elapsed time.
    int64_t unwrap_timestamp =
        rtp_ts_wraparound_handler_->Unwrap(audio_frame->timestamp_);
    audio_frame->elapsed_time_ms_ =
        (unwrap_timestamp - capture_start_rtp_time_stamp_) /
        (GetRtpTimestampRateHz() / 1000);

    {
      rtc::CritScope lock(&ts_stats_lock_);
      // Compute ntp time.
      audio_frame->ntp_time_ms_ =
          ntp_estimator_.Estimate(audio_frame->timestamp_);
      // |ntp_time_ms_| won't be valid until at least 2 RTCP SRs are received.
      if (audio_frame->ntp_time_ms_ > 0) {
        // Compute |capture_start_ntp_time_ms_| so that
        // |capture_start_ntp_time_ms_| + |elapsed_time_ms_| == |ntp_time_ms_|
        capture_start_ntp_time_ms_ =
            audio_frame->ntp_time_ms_ - audio_frame->elapsed_time_ms_;
      }
    }
  }

  {
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.TargetJitterBufferDelayMs",
                              audio_coding_->TargetDelayMs());
    const int jitter_buffer_delay = audio_coding_->FilteredCurrentDelayMs();
    rtc::CritScope lock(&video_sync_lock_);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDelayEstimateMs",
                              jitter_buffer_delay + playout_delay_ms_);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverJitterBufferDelayMs",
                              jitter_buffer_delay);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDeviceDelayMs",
                              playout_delay_ms_);
  }

  return muted ? AudioMixer::Source::AudioFrameInfo::kMuted
               : AudioMixer::Source::AudioFrameInfo::kNormal;
}

int ChannelReceive::PreferredSampleRate() const {
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(audio_coding_->ReceiveFrequency(),
                  audio_coding_->PlayoutFrequency());
}

ChannelReceive::ChannelReceive(
    ProcessThread* module_process_thread,
    AudioDeviceModule* audio_device_module,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    FrameDecryptorInterface* frame_decryptor)
    : event_log_(rtc_event_log),
      rtp_receive_statistics_(
          ReceiveStatistics::Create(Clock::GetRealTimeClock())),
      remote_ssrc_(remote_ssrc),
      _outputAudioLevel(),
      ntp_estimator_(Clock::GetRealTimeClock()),
      playout_timestamp_rtp_(0),
      playout_delay_ms_(0),
      rtp_ts_wraparound_handler_(new rtc::TimestampWrapAroundHandler()),
      capture_start_rtp_time_stamp_(-1),
      capture_start_ntp_time_ms_(-1),
      _moduleProcessThreadPtr(module_process_thread),
      _audioDeviceModulePtr(audio_device_module),
      _outputGain(1.0f),
      associated_send_channel_(nullptr),
      frame_decryptor_(frame_decryptor) {
  RTC_DCHECK(module_process_thread);
  RTC_DCHECK(audio_device_module);
  AudioCodingModule::Config acm_config;
  acm_config.decoder_factory = decoder_factory;
  acm_config.neteq_config.codec_pair_id = codec_pair_id;
  acm_config.neteq_config.max_packets_in_buffer = jitter_buffer_max_packets;
  acm_config.neteq_config.enable_fast_accelerate = jitter_buffer_fast_playout;
  acm_config.neteq_config.enable_muted_state = true;
  audio_coding_.reset(AudioCodingModule::Create(acm_config));

  _outputAudioLevel.Clear();

  rtp_receive_statistics_->EnableRetransmitDetection(remote_ssrc_, true);
  RtpRtcp::Configuration configuration;
  configuration.audio = true;
  // TODO(nisse): Also set receiver_only = true, but that seems to break RTT
  // estimation, resulting in test failures for
  // PeerConnectionIntegrationTest.GetCaptureStartNtpTimeWithOldStatsApi
  configuration.outgoing_transport = rtcp_send_transport;
  configuration.receive_statistics = rtp_receive_statistics_.get();

  configuration.event_log = event_log_;

  _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
  _rtpRtcpModule->SetSendingMediaStatus(false);
  _rtpRtcpModule->SetRemoteSSRC(remote_ssrc_);
  Init();
}

ChannelReceive::~ChannelReceive() {
  Terminate();
  RTC_DCHECK(!channel_state_.Get().playing);
}

void ChannelReceive::Init() {
  channel_state_.Reset();

  // --- Add modules to process thread (for periodic schedulation)
  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // --- ACM initialization
  int error = audio_coding_->InitializeReceiver();
  RTC_DCHECK_EQ(0, error);

  // --- RTP/RTCP module initialization

  // Ensure that RTCP is enabled by default for the created channel.
  // Note that, the module will keep generating RTCP until it is explicitly
  // disabled by the user.
  // After StopListen (when no sockets exists), RTCP packets will no longer
  // be transmitted since the Transport object will then be invalid.
  // RTCP is enabled by default.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);
}

void ChannelReceive::Terminate() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());
  // Must be called on the same thread as Init().
  rtp_receive_statistics_->RegisterRtcpStatisticsCallback(NULL);

  StopPlayout();

  // The order to safely shutdown modules in a channel is:
  // 1. De-register callbacks in modules
  // 2. De-register modules in process thread
  // 3. Destroy modules
  int error = audio_coding_->RegisterTransportCallback(NULL);
  RTC_DCHECK_EQ(0, error);

  // De-register modules in process thread
  if (_moduleProcessThreadPtr)
    _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());

  // End of modules shutdown
}

void ChannelReceive::SetSink(AudioSinkInterface* sink) {
  rtc::CritScope cs(&_callbackCritSect);
  audio_sink_ = sink;
}

int32_t ChannelReceive::StartPlayout() {
  if (channel_state_.Get().playing) {
    return 0;
  }

  channel_state_.SetPlaying(true);

  return 0;
}

int32_t ChannelReceive::StopPlayout() {
  if (!channel_state_.Get().playing) {
    return 0;
  }

  channel_state_.SetPlaying(false);
  _outputAudioLevel.Clear();

  return 0;
}

int32_t ChannelReceive::GetRecCodec(CodecInst& codec) {
  return (audio_coding_->ReceiveCodec(&codec));
}

std::vector<webrtc::RtpSource> ChannelReceive::GetSources() const {
  int64_t now_ms = rtc::TimeMillis();
  std::vector<RtpSource> sources;
  {
    rtc::CritScope cs(&rtp_sources_lock_);
    sources = contributing_sources_.GetSources(now_ms);
    if (last_received_rtp_system_time_ms_ >=
        now_ms - ContributingSources::kHistoryMs) {
      sources.emplace_back(*last_received_rtp_system_time_ms_, remote_ssrc_,
                           RtpSourceType::SSRC);
      sources.back().set_audio_level(last_received_rtp_audio_level_);
    }
  }
  return sources;
}

void ChannelReceive::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  for (const auto& kv : codecs) {
    RTC_DCHECK_GE(kv.second.clockrate_hz, 1000);
    payload_type_frequencies_[kv.first] = kv.second.clockrate_hz;
  }
  audio_coding_->SetReceiveCodecs(codecs);
}

// TODO(nisse): Move receive logic up to AudioReceiveStream.
void ChannelReceive::OnRtpPacket(const RtpPacketReceived& packet) {
  int64_t now_ms = rtc::TimeMillis();
  uint8_t audio_level;
  bool voice_activity;
  bool has_audio_level =
      packet.GetExtension<::webrtc::AudioLevel>(&voice_activity, &audio_level);

  {
    rtc::CritScope cs(&rtp_sources_lock_);
    last_received_rtp_timestamp_ = packet.Timestamp();
    last_received_rtp_system_time_ms_ = now_ms;
    if (has_audio_level)
      last_received_rtp_audio_level_ = audio_level;
    std::vector<uint32_t> csrcs = packet.Csrcs();
    contributing_sources_.Update(now_ms, csrcs);
  }

  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false);

  const auto& it = payload_type_frequencies_.find(packet.PayloadType());
  if (it == payload_type_frequencies_.end())
    return;
  // TODO(nisse): Set payload_type_frequency earlier, when packet is parsed.
  RtpPacketReceived packet_copy(packet);
  packet_copy.set_payload_type_frequency(it->second);

  rtp_receive_statistics_->OnRtpPacket(packet_copy);

  RTPHeader header;
  packet_copy.GetHeader(&header);

  ReceivePacket(packet_copy.data(), packet_copy.size(), header);
}

bool ChannelReceive::ReceivePacket(const uint8_t* packet,
                                   size_t packet_length,
                                   const RTPHeader& header) {
  const uint8_t* payload = packet + header.headerLength;
  assert(packet_length >= header.headerLength);
  size_t payload_length = packet_length - header.headerLength;
  WebRtcRTPHeader webrtc_rtp_header = {};
  webrtc_rtp_header.header = header;

  size_t payload_data_length = payload_length - header.paddingLength;

  // E2EE Custom Audio Frame Decryption (This is optional).
  // Keep this buffer around for the lifetime of the OnReceivedPayloadData call.
  rtc::Buffer decrypted_audio_payload;
  if (frame_decryptor_ != nullptr) {
    size_t max_plaintext_size = frame_decryptor_->GetMaxPlaintextByteSize(
        cricket::MEDIA_TYPE_AUDIO, payload_length);
    decrypted_audio_payload.SetSize(max_plaintext_size);

    size_t bytes_written = 0;
    std::vector<uint32_t> csrcs(header.arrOfCSRCs,
                                header.arrOfCSRCs + header.numCSRCs);
    int decrypt_status = frame_decryptor_->Decrypt(
        cricket::MEDIA_TYPE_AUDIO, csrcs,
        /*additional_data=*/nullptr,
        rtc::ArrayView<const uint8_t>(payload, payload_data_length),
        decrypted_audio_payload, &bytes_written);

    // In this case just interpret the failure as a silent frame.
    if (decrypt_status != 0) {
      bytes_written = 0;
    }

    // Resize the decrypted audio payload to the number of bytes actually
    // written.
    decrypted_audio_payload.SetSize(bytes_written);
    // Update the final payload.
    payload = decrypted_audio_payload.data();
    payload_data_length = decrypted_audio_payload.size();
  }

  if (payload_data_length == 0) {
    webrtc_rtp_header.frameType = kEmptyFrame;
    return OnReceivedPayloadData(nullptr, 0, &webrtc_rtp_header);
  }
  return OnReceivedPayloadData(payload, payload_data_length,
                               &webrtc_rtp_header);
}

int32_t ChannelReceive::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return 0;
  }

  int64_t nack_window_ms = rtt;
  if (nack_window_ms < kMinRetransmissionWindowMs) {
    nack_window_ms = kMinRetransmissionWindowMs;
  } else if (nack_window_ms > kMaxRetransmissionWindowMs) {
    nack_window_ms = kMaxRetransmissionWindowMs;
  }

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (0 != _rtpRtcpModule->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL,
                                     &rtp_timestamp)) {
    // Waiting for RTCP.
    return 0;
  }

  {
    rtc::CritScope lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
  return 0;
}

int ChannelReceive::GetSpeechOutputLevelFullRange() const {
  return _outputAudioLevel.LevelFullRange();
}

double ChannelReceive::GetTotalOutputEnergy() const {
  return _outputAudioLevel.TotalEnergy();
}

double ChannelReceive::GetTotalOutputDuration() const {
  return _outputAudioLevel.TotalDuration();
}

void ChannelReceive::SetChannelOutputVolumeScaling(float scaling) {
  rtc::CritScope cs(&volume_settings_critsect_);
  _outputGain = scaling;
}

int ChannelReceive::SetLocalSSRC(unsigned int ssrc) {
  _rtpRtcpModule->SetSSRC(ssrc);
  return 0;
}

// TODO(nisse): Pass ssrc in return value instead.
int ChannelReceive::GetRemoteSSRC(unsigned int& ssrc) {
  ssrc = remote_ssrc_;
  return 0;
}

void ChannelReceive::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  constexpr bool remb_candidate = false;
  packet_router->AddReceiveRtpModule(_rtpRtcpModule.get(), remb_candidate);
  packet_router_ = packet_router;
}

void ChannelReceive::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK(packet_router_);
  packet_router_->RemoveReceiveRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
}

int ChannelReceive::GetRTPStatistics(CallReceiveStatistics& stats) {
  // --- RtcpStatistics

  // The jitter statistics is updated for each received RTP packet and is
  // based on received packets.
  RtcpStatistics statistics;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(remote_ssrc_);
  if (statistician) {
    statistician->GetStatistics(&statistics,
                                _rtpRtcpModule->RTCP() == RtcpMode::kOff);
  }

  stats.fractionLost = statistics.fraction_lost;
  stats.cumulativeLost = statistics.packets_lost;
  stats.extendedMax = statistics.extended_highest_sequence_number;
  stats.jitterSamples = statistics.jitter;

  // --- RTT
  stats.rttMs = GetRTT();

  // --- Data counters

  size_t bytesReceived(0);
  uint32_t packetsReceived(0);

  if (statistician) {
    statistician->GetDataCounters(&bytesReceived, &packetsReceived);
  }

  stats.bytesReceived = bytesReceived;
  stats.packetsReceived = packetsReceived;

  // --- Timestamps
  {
    rtc::CritScope lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
  }
  return 0;
}

void ChannelReceive::SetNACKStatus(bool enable, int maxNumberOfPackets) {
  // None of these functions can fail.
  rtp_receive_statistics_->SetMaxReorderingThreshold(maxNumberOfPackets);
  if (enable)
    audio_coding_->EnableNack(maxNumberOfPackets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int ChannelReceive::ResendPackets(const uint16_t* sequence_numbers,
                                  int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

void ChannelReceive::SetAssociatedSendChannel(ChannelSend* channel) {
  rtc::CritScope lock(&assoc_send_channel_lock_);
  associated_send_channel_ = channel;
}

int ChannelReceive::GetNetworkStatistics(NetworkStatistics& stats) {
  return audio_coding_->GetNetworkStatistics(&stats);
}

void ChannelReceive::GetDecodingCallStatistics(
    AudioDecodingCallStats* stats) const {
  audio_coding_->GetDecodingCallStatistics(stats);
}

uint32_t ChannelReceive::GetDelayEstimate() const {
  rtc::CritScope lock(&video_sync_lock_);
  return audio_coding_->FilteredCurrentDelayMs() + playout_delay_ms_;
}

int ChannelReceive::SetMinimumPlayoutDelay(int delayMs) {
  if ((delayMs < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delayMs > kVoiceEngineMaxMinPlayoutDelayMs)) {
    RTC_DLOG(LS_ERROR) << "SetMinimumPlayoutDelay() invalid min delay";
    return -1;
  }
  if (audio_coding_->SetMinimumPlayoutDelay(delayMs) != 0) {
    RTC_DLOG(LS_ERROR)
        << "SetMinimumPlayoutDelay() failed to set min playout delay";
    return -1;
  }
  return 0;
}

int ChannelReceive::GetPlayoutTimestamp(unsigned int& timestamp) {
  uint32_t playout_timestamp_rtp = 0;
  {
    rtc::CritScope lock(&video_sync_lock_);
    playout_timestamp_rtp = playout_timestamp_rtp_;
  }
  if (playout_timestamp_rtp == 0) {
    RTC_DLOG(LS_ERROR) << "GetPlayoutTimestamp() failed to retrieve timestamp";
    return -1;
  }
  timestamp = playout_timestamp_rtp;
  return 0;
}

absl::optional<Syncable::Info> ChannelReceive::GetSyncInfo() const {
  Syncable::Info info;
  if (_rtpRtcpModule->RemoteNTP(&info.capture_time_ntp_secs,
                                &info.capture_time_ntp_frac, nullptr, nullptr,
                                &info.capture_time_source_clock) != 0) {
    return absl::nullopt;
  }
  {
    rtc::CritScope cs(&rtp_sources_lock_);
    if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_ms_) {
      return absl::nullopt;
    }
    info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
    info.latest_receive_time_ms = *last_received_rtp_system_time_ms_;
  }
  return info;
}

void ChannelReceive::UpdatePlayoutTimestamp(bool rtcp) {
  jitter_buffer_playout_timestamp_ = audio_coding_->PlayoutTimestamp();

  if (!jitter_buffer_playout_timestamp_) {
    // This can happen if this channel has not received any RTP packets. In
    // this case, NetEq is not capable of computing a playout timestamp.
    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    RTC_DLOG(LS_WARNING)
        << "ChannelReceive::UpdatePlayoutTimestamp() failed to read"
        << " playout delay from the ADM";
    return;
  }

  RTC_DCHECK(jitter_buffer_playout_timestamp_);
  uint32_t playout_timestamp = *jitter_buffer_playout_timestamp_;

  // Remove the playout delay.
  playout_timestamp -= (delay_ms * (GetRtpTimestampRateHz() / 1000));

  {
    rtc::CritScope lock(&video_sync_lock_);
    if (!rtcp) {
      playout_timestamp_rtp_ = playout_timestamp;
    }
    playout_delay_ms_ = delay_ms;
  }
}

int ChannelReceive::GetRtpTimestampRateHz() const {
  const auto format = audio_coding_->ReceiveFormat();
  // Default to the playout frequency if we've not gotten any packets yet.
  // TODO(ossu): Zero clockrate can only happen if we've added an external
  // decoder for a format we don't support internally. Remove once that way of
  // adding decoders is gone!
  return (format && format->clockrate_hz != 0)
             ? format->clockrate_hz
             : audio_coding_->PlayoutFrequency();
}

int64_t ChannelReceive::GetRTT() const {
  RtcpMode method = _rtpRtcpModule->RTCP();
  if (method == RtcpMode::kOff) {
    return 0;
  }
  std::vector<RTCPReportBlock> report_blocks;
  _rtpRtcpModule->RemoteRTCPStat(&report_blocks);

  // TODO(nisse): Could we check the return value from the ->RTT() call below,
  // instead of checking if we have any report blocks?
  if (report_blocks.empty()) {
    rtc::CritScope lock(&assoc_send_channel_lock_);
    // Tries to get RTT from an associated channel.
    if (!associated_send_channel_) {
      return 0;
    }
    return associated_send_channel_->GetRTT();
  }

  int64_t rtt = 0;
  int64_t avg_rtt = 0;
  int64_t max_rtt = 0;
  int64_t min_rtt = 0;
  if (_rtpRtcpModule->RTT(remote_ssrc_, &rtt, &avg_rtt, &min_rtt, &max_rtt) !=
      0) {
    return 0;
  }
  return rtt;
}

}  // namespace voe
}  // namespace webrtc
