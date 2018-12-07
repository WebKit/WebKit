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
#include "audio/audio_level.h"
#include "audio/channel_send.h"
#include "audio/utility/audio_frame_operations.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/contributing_sources.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/race_checker.h"
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

webrtc::FrameType WebrtcFrameTypeForMediaTransportFrameType(
    MediaTransportEncodedAudioFrame::FrameType frame_type) {
  switch (frame_type) {
    case MediaTransportEncodedAudioFrame::FrameType::kSpeech:
      return kAudioFrameSpeech;
      break;

    case MediaTransportEncodedAudioFrame::FrameType::
        kDiscountinuousTransmission:
      return kAudioFrameCN;
      break;
  }
}

WebRtcRTPHeader CreateWebrtcRTPHeaderForMediaTransportFrame(
    const MediaTransportEncodedAudioFrame& frame,
    uint64_t channel_id) {
  webrtc::WebRtcRTPHeader webrtc_header = {};
  webrtc_header.header.payloadType = frame.payload_type();
  webrtc_header.header.payload_type_frequency = frame.sampling_rate_hz();
  webrtc_header.header.timestamp = frame.starting_sample_index();
  webrtc_header.header.sequenceNumber = frame.sequence_number();

  webrtc_header.frameType =
      WebrtcFrameTypeForMediaTransportFrameType(frame.frame_type());

  webrtc_header.header.ssrc = static_cast<uint32_t>(channel_id);

  // The rest are initialized by the RTPHeader constructor.
  return webrtc_header;
}

class ChannelReceive : public ChannelReceiveInterface,
                       public MediaTransportAudioSinkInterface {
 public:
  // Used for receive streams.
  ChannelReceive(ProcessThread* module_process_thread,
                 AudioDeviceModule* audio_device_module,
                 MediaTransportInterface* media_transport,
                 Transport* rtcp_send_transport,
                 RtcEventLog* rtc_event_log,
                 uint32_t remote_ssrc,
                 size_t jitter_buffer_max_packets,
                 bool jitter_buffer_fast_playout,
                 int jitter_buffer_min_delay_ms,
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
  bool GetRecCodec(CodecInst* codec) const override;

  bool ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

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

  // Produces the transport-related timestamps; current_delay_ms is left unset.
  absl::optional<Syncable::Info> GetSyncInfo() const override;

  // RTP+RTCP
  void SetLocalSSRC(unsigned int ssrc) override;

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

  std::vector<RtpSource> GetSources() const override;

 private:
  bool ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header);
  int ResendPackets(const uint16_t* sequence_numbers, int length);
  void UpdatePlayoutTimestamp(bool rtcp);

  int GetRtpTimestampRateHz() const;
  int64_t GetRTT() const;

  // MediaTransportAudioSinkInterface override;
  void OnData(uint64_t channel_id,
              MediaTransportEncodedAudioFrame frame) override;

  int32_t OnReceivedPayloadData(const uint8_t* payloadData,
                                size_t payloadSize,
                                const WebRtcRTPHeader* rtpHeader);

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

  // Info for GetSources and GetSyncInfo is updated on network or worker thread,
  // queried on the worker thread.
  rtc::CriticalSection rtp_sources_lock_;
  ContributingSources contributing_sources_ RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(&rtp_sources_lock_);
  absl::optional<uint8_t> last_received_rtp_audio_level_
      RTC_GUARDED_BY(&rtp_sources_lock_);

  std::unique_ptr<AudioCodingModule> audio_coding_;
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

  MediaTransportInterface* const media_transport_;

  // E2EE Audio Frame Decryption
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_;
  webrtc::CryptoOptions crypto_options_;
};

int32_t ChannelReceive::OnReceivedPayloadData(
    const uint8_t* payloadData,
    size_t payloadSize,
    const WebRtcRTPHeader* rtpHeader) {
  // We should not be receiving any RTP packets if media_transport is set.
  RTC_CHECK(!media_transport_);

  if (!Playing()) {
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

// MediaTransportAudioSinkInterface override.
void ChannelReceive::OnData(uint64_t channel_id,
                            MediaTransportEncodedAudioFrame frame) {
  RTC_CHECK(media_transport_);

  if (!Playing()) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    return;
  }

  // Send encoded audio frame to Decoder / NetEq.
  if (audio_coding_->IncomingPacket(
          frame.encoded_data().data(), frame.encoded_data().size(),
          CreateWebrtcRTPHeaderForMediaTransportFrame(frame, channel_id)) !=
      0) {
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
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(audio_coding_->ReceiveFrequency(),
                  audio_coding_->PlayoutFrequency());
}

ChannelReceive::ChannelReceive(
    ProcessThread* module_process_thread,
    AudioDeviceModule* audio_device_module,
    MediaTransportInterface* media_transport,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options)
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
      media_transport_(media_transport),
      frame_decryptor_(frame_decryptor),
      crypto_options_(crypto_options) {
  // TODO(nisse): Use _moduleProcessThreadPtr instead?
  module_process_thread_checker_.DetachFromThread();

  RTC_DCHECK(module_process_thread);
  RTC_DCHECK(audio_device_module);
  AudioCodingModule::Config acm_config;
  acm_config.decoder_factory = decoder_factory;
  acm_config.neteq_config.codec_pair_id = codec_pair_id;
  acm_config.neteq_config.max_packets_in_buffer = jitter_buffer_max_packets;
  acm_config.neteq_config.enable_fast_accelerate = jitter_buffer_fast_playout;
  acm_config.neteq_config.min_delay_ms = jitter_buffer_min_delay_ms;
  acm_config.neteq_config.enable_muted_state = true;
  audio_coding_.reset(AudioCodingModule::Create(acm_config));

  _outputAudioLevel.Clear();

  rtp_receive_statistics_->EnableRetransmitDetection(remote_ssrc_, true);
  RtpRtcp::Configuration configuration;
  configuration.audio = true;
  configuration.receiver_only = true;
  configuration.outgoing_transport = rtcp_send_transport;
  configuration.receive_statistics = rtp_receive_statistics_.get();

  configuration.event_log = event_log_;

  _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
  _rtpRtcpModule->SetSendingMediaStatus(false);
  _rtpRtcpModule->SetRemoteSSRC(remote_ssrc_);

  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // Ensure that RTCP is enabled by default for the created channel.
  // Note that, the module will keep generating RTCP until it is explicitly
  // disabled by the user.
  // After StopListen (when no sockets exists), RTCP packets will no longer
  // be transmitted since the Transport object will then be invalid.
  // RTCP is enabled by default.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);

  if (media_transport_) {
    media_transport_->SetReceiveAudioSink(this);
  }
}

ChannelReceive::~ChannelReceive() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());

  if (media_transport_) {
    media_transport_->SetReceiveAudioSink(nullptr);
  }

  StopPlayout();

  int error = audio_coding_->RegisterTransportCallback(NULL);
  RTC_DCHECK_EQ(0, error);

  if (_moduleProcessThreadPtr)
    _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());
}

void ChannelReceive::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::CritScope cs(&_callbackCritSect);
  audio_sink_ = sink;
}

void ChannelReceive::StartPlayout() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::CritScope lock(&playing_lock_);
  playing_ = true;
}

void ChannelReceive::StopPlayout() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::CritScope lock(&playing_lock_);
  playing_ = false;
  _outputAudioLevel.Clear();
}

bool ChannelReceive::GetRecCodec(CodecInst* codec) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return (audio_coding_->ReceiveCodec(codec) == 0);
}

std::vector<webrtc::RtpSource> ChannelReceive::GetSources() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
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
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  for (const auto& kv : codecs) {
    RTC_DCHECK_GE(kv.second.clockrate_hz, 1000);
    payload_type_frequencies_[kv.first] = kv.second.clockrate_hz;
  }
  audio_coding_->SetReceiveCodecs(codecs);
}

// May be called on either worker thread or network thread.
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
    contributing_sources_.Update(
        now_ms, csrcs,
        has_audio_level ? absl::optional<uint8_t>(audio_level) : absl::nullopt);
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
  } else if (crypto_options_.sframe.require_frame_encryption) {
    RTC_DLOG(LS_ERROR)
        << "FrameDecryptor required but not set, dropping packet";
    payload_data_length = 0;
  }

  if (payload_data_length == 0) {
    webrtc_rtp_header.frameType = kEmptyFrame;
    return OnReceivedPayloadData(nullptr, 0, &webrtc_rtp_header);
  }
  return OnReceivedPayloadData(payload, payload_data_length,
                               &webrtc_rtp_header);
}

// May be called on either worker thread or network thread.
// TODO(nisse): Drop always-true return value.
bool ChannelReceive::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return true;
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
    return true;
  }

  {
    rtc::CritScope lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
  return true;
}

int ChannelReceive::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return _outputAudioLevel.LevelFullRange();
}

double ChannelReceive::GetTotalOutputEnergy() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return _outputAudioLevel.TotalEnergy();
}

double ChannelReceive::GetTotalOutputDuration() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return _outputAudioLevel.TotalDuration();
}

void ChannelReceive::SetChannelOutputVolumeScaling(float scaling) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::CritScope cs(&volume_settings_critsect_);
  _outputGain = scaling;
}

void ChannelReceive::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  _rtpRtcpModule->SetSSRC(ssrc);
}

void ChannelReceive::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  constexpr bool remb_candidate = false;
  packet_router->AddReceiveRtpModule(_rtpRtcpModule.get(), remb_candidate);
  packet_router_ = packet_router;
}

void ChannelReceive::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(packet_router_);
  packet_router_->RemoveReceiveRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
}

CallReceiveStatistics ChannelReceive::GetRTCPStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // --- RtcpStatistics
  CallReceiveStatistics stats;

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
  return stats;
}

void ChannelReceive::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // None of these functions can fail.
  rtp_receive_statistics_->SetMaxReorderingThreshold(max_packets);
  if (enable)
    audio_coding_->EnableNack(max_packets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int ChannelReceive::ResendPackets(const uint16_t* sequence_numbers,
                                  int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

void ChannelReceive::SetAssociatedSendChannel(
    const ChannelSendInterface* channel) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  rtc::CritScope lock(&assoc_send_channel_lock_);
  associated_send_channel_ = channel;
}

NetworkStatistics ChannelReceive::GetNetworkStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  NetworkStatistics stats;
  int error = audio_coding_->GetNetworkStatistics(&stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

AudioDecodingCallStats ChannelReceive::GetDecodingCallStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  AudioDecodingCallStats stats;
  audio_coding_->GetDecodingCallStatistics(&stats);
  return stats;
}

uint32_t ChannelReceive::GetDelayEstimate() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread() ||
             module_process_thread_checker_.CalledOnValidThread());
  rtc::CritScope lock(&video_sync_lock_);
  return audio_coding_->FilteredCurrentDelayMs() + playout_delay_ms_;
}

void ChannelReceive::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  // Limit to range accepted by both VoE and ACM, so we're at least getting as
  // close as possible, instead of failing.
  delay_ms = rtc::SafeClamp(delay_ms, 0, 10000);
  if ((delay_ms < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delay_ms > kVoiceEngineMaxMinPlayoutDelayMs)) {
    RTC_DLOG(LS_ERROR) << "SetMinimumPlayoutDelay() invalid min delay";
    return;
  }
  if (audio_coding_->SetMinimumPlayoutDelay(delay_ms) != 0) {
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

absl::optional<Syncable::Info> ChannelReceive::GetSyncInfo() const {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
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
  if (media_transport_) {
    auto target_rate = media_transport_->GetLatestTargetTransferRate();
    if (target_rate.has_value()) {
      return target_rate->network_estimate.round_trip_time.ms();
    }

    return 0;
  }
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
    ProcessThread* module_process_thread,
    AudioDeviceModule* audio_device_module,
    MediaTransportInterface* media_transport,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options) {
  return absl::make_unique<ChannelReceive>(
      module_process_thread, audio_device_module, media_transport,
      rtcp_send_transport, rtc_event_log, remote_ssrc,
      jitter_buffer_max_packets, jitter_buffer_fast_playout,
      jitter_buffer_min_delay_ms, decoder_factory, codec_pair_id,
      frame_decryptor, crypto_options);
}

}  // namespace voe
}  // namespace webrtc
