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
#include "api/crypto/frame_decryptor_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "audio/audio_level.h"
#include "audio/channel_send.h"
#include "audio/utility/audio_frame_operations.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace voe {

namespace {

constexpr double kAudioSampleDurationSeconds = 0.01;

// Video Sync.
constexpr int kVoiceEngineMinMinPlayoutDelayMs = 0;
constexpr int kVoiceEngineMaxMinPlayoutDelayMs = 10000;

// Field trial which controls whether to report standard-compliant bytes
// sent/received per stream.  If enabled, padding and headers are not included
// in bytes sent or received.
constexpr char kUseStandardBytesStats[] = "WebRTC-UseStandardBytesStats";

RTPHeader CreateRTPHeaderForMediaTransportFrame(
    const MediaTransportEncodedAudioFrame& frame,
    uint64_t channel_id) {
  webrtc::RTPHeader rtp_header;
  rtp_header.payloadType = frame.payload_type();
  rtp_header.payload_type_frequency = frame.sampling_rate_hz();
  rtp_header.timestamp = frame.starting_sample_index();
  rtp_header.sequenceNumber = frame.sequence_number();

  rtp_header.ssrc = static_cast<uint32_t>(channel_id);

  // The rest are initialized by the RTPHeader constructor.
  return rtp_header;
}

AudioCodingModule::Config AcmConfig(
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout) {
  AudioCodingModule::Config acm_config;
  acm_config.decoder_factory = decoder_factory;
  acm_config.neteq_config.codec_pair_id = codec_pair_id;
  acm_config.neteq_config.max_packets_in_buffer = jitter_buffer_max_packets;
  acm_config.neteq_config.enable_fast_accelerate = jitter_buffer_fast_playout;
  acm_config.neteq_config.enable_muted_state = true;

  return acm_config;
}

class ChannelReceive : public ChannelReceiveInterface,
                       public MediaTransportAudioSinkInterface {
 public:
  // Used for receive streams.
  ChannelReceive(Clock* clock,
                 ProcessThread* module_process_thread,
                 AudioDeviceModule* audio_device_module,
                 const MediaTransportConfig& media_transport_config,
                 Transport* rtcp_send_transport,
                 RtcEventLog* rtc_event_log,
                 uint32_t local_ssrc,
                 uint32_t remote_ssrc,
                 size_t jitter_buffer_max_packets,
                 bool jitter_buffer_fast_playout,
                 int jitter_buffer_min_delay_ms,
                 bool jitter_buffer_enable_rtx_handling,
                 rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
                 absl::optional<AudioCodecPairId> codec_pair_id,
                 rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
                 const webrtc::CryptoOptions& crypto_options);
  ~ChannelReceive() override;

  void SetSink(AudioSinkInterface* sink) override;

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs) override;

  // API methods

  void StartPlayout() override;
  void StopPlayout() override;

  // Codecs
  absl::optional<std::pair<int, SdpAudioFormat>> GetReceiveCodec()
      const override;

  void ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

  // RtpPacketSinkInterface.
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // Muting, Volume and Level.
  void SetChannelOutputVolumeScaling(float scaling) override;
  int GetSpeechOutputLevelFullRange() const override;
  // See description of "totalAudioEnergy" in the WebRTC stats spec:
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
  double GetTotalOutputEnergy() const override;
  double GetTotalOutputDuration() const override;

  // Stats.
  NetworkStatistics GetNetworkStatistics() const override;
  AudioDecodingCallStats GetDecodingCallStatistics() const override;

  // Audio+Video Sync.
  uint32_t GetDelayEstimate() const override;
  void SetMinimumPlayoutDelay(int delayMs) override;
  uint32_t GetPlayoutTimestamp() const override;

  // Audio quality.
  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override;
  int GetBaseMinimumPlayoutDelayMs() const override;

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  absl::optional<Syncable::Info> GetSyncInfo() const override;

  void RegisterReceiverCongestionControlObjects(
      PacketRouter* packet_router) override;
  void ResetReceiverCongestionControlObjects() override;

  CallReceiveStatistics GetRTCPStatistics() const override;
  void SetNACKStatus(bool enable, int maxNumberOfPackets) override;

  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) override;

  int PreferredSampleRate() const override;

  // Associate to a send channel.
  // Used for obtaining RTT for a receive-only channel.
  void SetAssociatedSendChannel(const ChannelSendInterface* channel) override;

  // TODO(sukhanov): Return const pointer. It requires making media transport
  // getters like GetLatestTargetTransferRate to be also const.
  MediaTransportInterface* media_transport() const {
    return media_transport_config_.media_transport;
  }

 private:
  void ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header);
  int ResendPackets(const uint16_t* sequence_numbers, int length);
  void UpdatePlayoutTimestamp(bool rtcp);

  int GetRtpTimestampRateHz() const;
  int64_t GetRTT() const;

  // MediaTransportAudioSinkInterface override;
  void OnData(uint64_t channel_id,
              MediaTransportEncodedAudioFrame frame) override;

  void OnReceivedPayloadData(rtc::ArrayView<const uint8_t> payload,
                             const RTPHeader& rtpHeader);

  bool Playing() const {
    rtc::CritScope lock(&playing_lock_);
    return playing_;
  }

  // Thread checkers document and lock usage of some methods to specific threads
  // we know about. The goal is to eventually split up voe::ChannelReceive into
  // parts with single-threaded semantics, and thereby reduce the need for
  // locks.
  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker module_process_thread_checker_;
  // Methods accessed from audio and video threads are checked for sequential-
  // only access. We don't necessarily own and control these threads, so thread
  // checkers cannot be used. E.g. Chromium may transfer "ownership" from one
  // audio thread to another, but access is still sequential.
  rtc::RaceChecker audio_thread_race_checker_;
  rtc::RaceChecker video_capture_thread_race_checker_;
  rtc::CriticalSection _callbackCritSect;
  rtc::CriticalSection volume_settings_critsect_;

  rtc::CriticalSection playing_lock_;
  bool playing_ RTC_GUARDED_BY(&playing_lock_) = false;

  RtcEventLog* const event_log_;

  // Indexed by payload type.
  std::map<uint8_t, int> payload_type_frequencies_;

  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<RtpRtcp> _rtpRtcpModule;
  const uint32_t remote_ssrc_;

  // Info for GetSyncInfo is updated on network or worker thread, and queried on
  // the worker thread.
  rtc::CriticalSection sync_info_lock_;
  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(&sync_info_lock_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(&sync_info_lock_);

  // The AcmReceiver is thread safe, using its own lock.
  acm2::AcmReceiver acm_receiver_;
  AudioSinkInterface* audio_sink_ = nullptr;
  AudioLevel _outputAudioLevel;

  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(ts_stats_lock_);

  // Timestamp of the audio pulled from NetEq.
  absl::optional<uint32_t> jitter_buffer_playout_timestamp_;

  rtc::CriticalSection video_sync_lock_;
  uint32_t playout_timestamp_rtp_ RTC_GUARDED_BY(video_sync_lock_);
  uint32_t playout_delay_ms_ RTC_GUARDED_BY(video_sync_lock_);

  rtc::CriticalSection ts_stats_lock_;

  std::unique_ptr<rtc::TimestampWrapAroundHandler> rtp_ts_wraparound_handler_;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_ RTC_GUARDED_BY(ts_stats_lock_);

  // uses
  ProcessThread* _moduleProcessThreadPtr;
  AudioDeviceModule* _audioDeviceModulePtr;
  float _outputGain RTC_GUARDED_BY(volume_settings_critsect_);

  // An associated send channel.
  rtc::CriticalSection assoc_send_channel_lock_;
  const ChannelSendInterface* associated_send_channel_
      RTC_GUARDED_BY(assoc_send_channel_lock_);

  PacketRouter* packet_router_ = nullptr;

  rtc::ThreadChecker construction_thread_;

  MediaTransportConfig media_transport_config_;

  // E2EE Audio Frame Decryption
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_;
  webrtc::CryptoOptions crypto_options_;

  const bool use_standard_bytes_stats_;
};

void ChannelReceive::OnReceivedPayloadData(
    rtc::ArrayView<const uint8_t> payload,
    const RTPHeader& rtpHeader) {
  // We should not be receiving any RTP packets if media_transport is set.
  RTC_CHECK(!media_transport());

  if (!Playing()) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    return;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (acm_receiver_.InsertPacket(rtpHeader, payload) != 0) {
    RTC_DLOG(LS_ERROR) << "ChannelReceive::OnReceivedPayloadData() unable to "
                          "push data to the ACM";
    return;
  }

  int64_t round_trip_time = 0;
  _rtpRtcpModule->RTT(remote_ssrc_, &round_trip_time, NULL, NULL, NULL);

  std::vector<uint16_t> nack_list = acm_receiver_.GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Can't use nack_list.data() since it's not supported by all
    // compilers.
    ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
}

// MediaTransportAudioSinkInterface override.
void ChannelReceive::OnData(uint64_t channel_id,
                            MediaTransportEncodedAudioFrame frame) {
  RTC_CHECK(media_transport());

  if (!Playing()) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    return;
  }

  // Send encoded audio frame to Decoder / NetEq.
  if (acm_receiver_.InsertPacket(
          CreateRTPHeaderForMediaTransportFrame(frame, channel_id),
          frame.encoded_data()) != 0) {
    RTC_DLOG(LS_ERROR) << "ChannelReceive::OnData: unable to "
                          "push data to the ACM";
  }
}

AudioMixer::Source::AudioFrameInfo ChannelReceive::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  event_log_->Log(absl::make_unique<RtcEventAudioPlayout>(remote_ssrc_));

  // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
  bool muted;
  if (acm_receiver_.GetAudio(audio_frame->sample_rate_hz_, audio_frame,
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
                              acm_receiver_.TargetDelayMs());
    const int jitter_buffer_delay = acm_receiver_.FilteredCurrentDelayMs();
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
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(acm_receiver_.last_packet_sample_rate_hz().value_or(0),
                  acm_receiver_.last_output_sample_rate_hz());
}

ChannelReceive::ChannelReceive(
    Clock* clock,
    ProcessThread* module_process_thread,
    AudioDeviceModule* audio_device_module,
    const MediaTransportConfig& media_transport_config,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t local_ssrc,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    bool jitter_buffer_enable_rtx_handling,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options)
    : event_log_(rtc_event_log),
      rtp_receive_statistics_(ReceiveStatistics::Create(clock)),
      remote_ssrc_(remote_ssrc),
      acm_receiver_(AcmConfig(decoder_factory,
                              codec_pair_id,
                              jitter_buffer_max_packets,
                              jitter_buffer_fast_playout)),
      _outputAudioLevel(),
      ntp_estimator_(clock),
      playout_timestamp_rtp_(0),
      playout_delay_ms_(0),
      rtp_ts_wraparound_handler_(new rtc::TimestampWrapAroundHandler()),
      capture_start_rtp_time_stamp_(-1),
      capture_start_ntp_time_ms_(-1),
      _moduleProcessThreadPtr(module_process_thread),
      _audioDeviceModulePtr(audio_device_module),
      _outputGain(1.0f),
      associated_send_channel_(nullptr),
      media_transport_config_(media_transport_config),
      frame_decryptor_(frame_decryptor),
      crypto_options_(crypto_options),
      use_standard_bytes_stats_(
          webrtc::field_trial::IsEnabled(kUseStandardBytesStats)) {
  // TODO(nisse): Use _moduleProcessThreadPtr instead?
  module_process_thread_checker_.Detach();

  RTC_DCHECK(module_process_thread);
  RTC_DCHECK(audio_device_module);

  acm_receiver_.ResetInitialDelay();
  acm_receiver_.SetMinimumDelay(0);
  acm_receiver_.SetMaximumDelay(0);
  acm_receiver_.FlushBuffers();

  _outputAudioLevel.ResetLevelFullRange();

  rtp_receive_statistics_->EnableRetransmitDetection(remote_ssrc_, true);
  RtpRtcp::Configuration configuration;
  configuration.clock = clock;
  configuration.audio = true;
  configuration.receiver_only = true;
  configuration.outgoing_transport = rtcp_send_transport;
  configuration.receive_statistics = rtp_receive_statistics_.get();
  configuration.event_log = event_log_;
  configuration.local_media_ssrc = local_ssrc;

  _rtpRtcpModule = RtpRtcp::Create(configuration);
  _rtpRtcpModule->SetSendingMediaStatus(false);
  _rtpRtcpModule->SetRemoteSSRC(remote_ssrc_);

  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // Ensure that RTCP is enabled for the created channel.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);

  if (media_transport()) {
    media_transport()->SetReceiveAudioSink(this);
  }
}

ChannelReceive::~ChannelReceive() {
  RTC_DCHECK(construction_thread_.IsCurrent());

  if (media_transport()) {
    media_transport()->SetReceiveAudioSink(nullptr);
  }

  StopPlayout();

  if (_moduleProcessThreadPtr)
    _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());
}

void ChannelReceive::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  rtc::CritScope cs(&_callbackCritSect);
  audio_sink_ = sink;
}

void ChannelReceive::StartPlayout() {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  rtc::CritScope lock(&playing_lock_);
  playing_ = true;
}

void ChannelReceive::StopPlayout() {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  rtc::CritScope lock(&playing_lock_);
  playing_ = false;
  _outputAudioLevel.ResetLevelFullRange();
}

absl::optional<std::pair<int, SdpAudioFormat>> ChannelReceive::GetReceiveCodec()
    const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  return acm_receiver_.LastDecoder();
}

void ChannelReceive::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  for (const auto& kv : codecs) {
    RTC_DCHECK_GE(kv.second.clockrate_hz, 1000);
    payload_type_frequencies_[kv.first] = kv.second.clockrate_hz;
  }
  acm_receiver_.SetCodecs(codecs);
}

// May be called on either worker thread or network thread.
void ChannelReceive::OnRtpPacket(const RtpPacketReceived& packet) {
  int64_t now_ms = rtc::TimeMillis();

  {
    rtc::CritScope cs(&sync_info_lock_);
    last_received_rtp_timestamp_ = packet.Timestamp();
    last_received_rtp_system_time_ms_ = now_ms;
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

void ChannelReceive::ReceivePacket(const uint8_t* packet,
                                   size_t packet_length,
                                   const RTPHeader& header) {
  const uint8_t* payload = packet + header.headerLength;
  assert(packet_length >= header.headerLength);
  size_t payload_length = packet_length - header.headerLength;

  size_t payload_data_length = payload_length - header.paddingLength;

  // E2EE Custom Audio Frame Decryption (This is optional).
  // Keep this buffer around for the lifetime of the OnReceivedPayloadData call.
  rtc::Buffer decrypted_audio_payload;
  if (frame_decryptor_ != nullptr) {
    const size_t max_plaintext_size = frame_decryptor_->GetMaxPlaintextByteSize(
        cricket::MEDIA_TYPE_AUDIO, payload_length);
    decrypted_audio_payload.SetSize(max_plaintext_size);

    const std::vector<uint32_t> csrcs(header.arrOfCSRCs,
                                      header.arrOfCSRCs + header.numCSRCs);
    const FrameDecryptorInterface::Result decrypt_result =
        frame_decryptor_->Decrypt(
            cricket::MEDIA_TYPE_AUDIO, csrcs,
            /*additional_data=*/nullptr,
            rtc::ArrayView<const uint8_t>(payload, payload_data_length),
            decrypted_audio_payload);

    if (decrypt_result.IsOk()) {
      decrypted_audio_payload.SetSize(decrypt_result.bytes_written);
    } else {
      // Interpret failures as a silent frame.
      decrypted_audio_payload.SetSize(0);
    }

    payload = decrypted_audio_payload.data();
    payload_data_length = decrypted_audio_payload.size();
  } else if (crypto_options_.sframe.require_frame_encryption) {
    RTC_DLOG(LS_ERROR)
        << "FrameDecryptor required but not set, dropping packet";
    payload_data_length = 0;
  }

  OnReceivedPayloadData(
      rtc::ArrayView<const uint8_t>(payload, payload_data_length), header);
}

// May be called on either worker thread or network thread.
void ChannelReceive::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return;
  }

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (0 != _rtpRtcpModule->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL,
                                     &rtp_timestamp)) {
    // Waiting for RTCP.
    return;
  }

  {
    rtc::CritScope lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
}

int ChannelReceive::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  return _outputAudioLevel.LevelFullRange();
}

double ChannelReceive::GetTotalOutputEnergy() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  return _outputAudioLevel.TotalEnergy();
}

double ChannelReceive::GetTotalOutputDuration() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  return _outputAudioLevel.TotalDuration();
}

void ChannelReceive::SetChannelOutputVolumeScaling(float scaling) {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  rtc::CritScope cs(&volume_settings_critsect_);
  _outputGain = scaling;
}

void ChannelReceive::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  constexpr bool remb_candidate = false;
  packet_router->AddReceiveRtpModule(_rtpRtcpModule.get(), remb_candidate);
  packet_router_ = packet_router;
}

void ChannelReceive::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  RTC_DCHECK(packet_router_);
  packet_router_->RemoveReceiveRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
}

CallReceiveStatistics ChannelReceive::GetRTCPStatistics() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  // --- RtcpStatistics
  CallReceiveStatistics stats;

  // The jitter statistics is updated for each received RTP packet and is
  // based on received packets.
  RtpReceiveStats rtp_stats;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(remote_ssrc_);
  if (statistician) {
    rtp_stats = statistician->GetStats();
  }

  stats.cumulativeLost = rtp_stats.packets_lost;
  stats.jitterSamples = rtp_stats.jitter;

  // --- RTT
  stats.rttMs = GetRTT();

  // --- Data counters
  if (statistician) {
    if (use_standard_bytes_stats_) {
      stats.bytesReceived = rtp_stats.packet_counter.payload_bytes;
    } else {
      stats.bytesReceived = rtp_stats.packet_counter.TotalBytes();
    }
    stats.packetsReceived = rtp_stats.packet_counter.packets;
    stats.last_packet_received_timestamp_ms =
        rtp_stats.last_packet_received_timestamp_ms;
  } else {
    stats.bytesReceived = 0;
    stats.packetsReceived = 0;
    stats.last_packet_received_timestamp_ms = absl::nullopt;
  }

  // --- Timestamps
  {
    rtc::CritScope lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
  }
  return stats;
}

void ChannelReceive::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  // None of these functions can fail.
  if (enable) {
    rtp_receive_statistics_->SetMaxReorderingThreshold(max_packets);
    acm_receiver_.EnableNack(max_packets);
  } else {
    rtp_receive_statistics_->SetMaxReorderingThreshold(
        kDefaultMaxReorderingThreshold);
    acm_receiver_.DisableNack();
  }
}

// Called when we are missing one or more packets.
int ChannelReceive::ResendPackets(const uint16_t* sequence_numbers,
                                  int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

void ChannelReceive::SetAssociatedSendChannel(
    const ChannelSendInterface* channel) {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  rtc::CritScope lock(&assoc_send_channel_lock_);
  associated_send_channel_ = channel;
}

NetworkStatistics ChannelReceive::GetNetworkStatistics() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  NetworkStatistics stats;
  acm_receiver_.GetNetworkStatistics(&stats);
  return stats;
}

AudioDecodingCallStats ChannelReceive::GetDecodingCallStatistics() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent());
  AudioDecodingCallStats stats;
  acm_receiver_.GetDecodingCallStatistics(&stats);
  return stats;
}

uint32_t ChannelReceive::GetDelayEstimate() const {
  RTC_DCHECK(worker_thread_checker_.IsCurrent() ||
             module_process_thread_checker_.IsCurrent());
  rtc::CritScope lock(&video_sync_lock_);
  return acm_receiver_.FilteredCurrentDelayMs() + playout_delay_ms_;
}

void ChannelReceive::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK(module_process_thread_checker_.IsCurrent());
  // Limit to range accepted by both VoE and ACM, so we're at least getting as
  // close as possible, instead of failing.
  delay_ms = rtc::SafeClamp(delay_ms, kVoiceEngineMinMinPlayoutDelayMs,
                            kVoiceEngineMaxMinPlayoutDelayMs);
  if (acm_receiver_.SetMinimumDelay(delay_ms) != 0) {
    RTC_DLOG(LS_ERROR)
        << "SetMinimumPlayoutDelay() failed to set min playout delay";
  }
}

uint32_t ChannelReceive::GetPlayoutTimestamp() const {
  RTC_DCHECK_RUNS_SERIALIZED(&video_capture_thread_race_checker_);
  {
    rtc::CritScope lock(&video_sync_lock_);
    return playout_timestamp_rtp_;
  }
}

bool ChannelReceive::SetBaseMinimumPlayoutDelayMs(int delay_ms) {
  return acm_receiver_.SetBaseMinimumDelayMs(delay_ms);
}

int ChannelReceive::GetBaseMinimumPlayoutDelayMs() const {
  return acm_receiver_.GetBaseMinimumDelayMs();
}

absl::optional<Syncable::Info> ChannelReceive::GetSyncInfo() const {
  RTC_DCHECK(module_process_thread_checker_.IsCurrent());
  Syncable::Info info;
  if (_rtpRtcpModule->RemoteNTP(&info.capture_time_ntp_secs,
                                &info.capture_time_ntp_frac, nullptr, nullptr,
                                &info.capture_time_source_clock) != 0) {
    return absl::nullopt;
  }
  {
    rtc::CritScope cs(&sync_info_lock_);
    if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_ms_) {
      return absl::nullopt;
    }
    info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
    info.latest_receive_time_ms = *last_received_rtp_system_time_ms_;
  }
  return info;
}

void ChannelReceive::UpdatePlayoutTimestamp(bool rtcp) {
  jitter_buffer_playout_timestamp_ = acm_receiver_.GetPlayoutTimestamp();

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
  const auto decoder = acm_receiver_.LastDecoder();
  // Default to the playout frequency if we've not gotten any packets yet.
  // TODO(ossu): Zero clockrate can only happen if we've added an external
  // decoder for a format we don't support internally. Remove once that way of
  // adding decoders is gone!
  return (decoder && decoder->second.clockrate_hz != 0)
             ? decoder->second.clockrate_hz
             : acm_receiver_.last_output_sample_rate_hz();
}

int64_t ChannelReceive::GetRTT() const {
  if (media_transport()) {
    auto target_rate = media_transport()->GetLatestTargetTransferRate();
    if (target_rate.has_value()) {
      return target_rate->network_estimate.round_trip_time.ms();
    }

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
  // TODO(nisse): This method computes RTT based on sender reports, even though
  // a receive stream is not supposed to do that.
  if (_rtpRtcpModule->RTT(remote_ssrc_, &rtt, &avg_rtt, &min_rtt, &max_rtt) !=
      0) {
    return 0;
  }
  return rtt;
}

}  // namespace

std::unique_ptr<ChannelReceiveInterface> CreateChannelReceive(
    Clock* clock,
    ProcessThread* module_process_thread,
    AudioDeviceModule* audio_device_module,
    const MediaTransportConfig& media_transport_config,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t local_ssrc,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    bool jitter_buffer_enable_rtx_handling,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options) {
  return absl::make_unique<ChannelReceive>(
      clock, module_process_thread, audio_device_module, media_transport_config,
      rtcp_send_transport, rtc_event_log, local_ssrc, remote_ssrc,
      jitter_buffer_max_packets, jitter_buffer_fast_playout,
      jitter_buffer_min_delay_ms, jitter_buffer_enable_rtx_handling,
      decoder_factory, codec_pair_id, frame_decryptor, crypto_options);
}

}  // namespace voe
}  // namespace webrtc
