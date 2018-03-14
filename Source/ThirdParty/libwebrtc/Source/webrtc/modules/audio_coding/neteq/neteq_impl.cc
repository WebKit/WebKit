/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/neteq_impl.h"

#include <assert.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "api/audio_codecs/audio_decoder.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/neteq/accelerate.h"
#include "modules/audio_coding/neteq/background_noise.h"
#include "modules/audio_coding/neteq/buffer_level_filter.h"
#include "modules/audio_coding/neteq/comfort_noise.h"
#include "modules/audio_coding/neteq/decision_logic.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/defines.h"
#include "modules/audio_coding/neteq/delay_manager.h"
#include "modules/audio_coding/neteq/delay_peak_detector.h"
#include "modules/audio_coding/neteq/dtmf_buffer.h"
#include "modules/audio_coding/neteq/dtmf_tone_generator.h"
#include "modules/audio_coding/neteq/expand.h"
#include "modules/audio_coding/neteq/merge.h"
#include "modules/audio_coding/neteq/nack_tracker.h"
#include "modules/audio_coding/neteq/normal.h"
#include "modules/audio_coding/neteq/packet.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/post_decode_vad.h"
#include "modules/audio_coding/neteq/preemptive_expand.h"
#include "modules/audio_coding/neteq/red_payload_splitter.h"
#include "modules/audio_coding/neteq/sync_buffer.h"
#include "modules/audio_coding/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/timestamp_scaler.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

NetEqImpl::Dependencies::Dependencies(
    const NetEq::Config& config,
    const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory)
    : tick_timer(new TickTimer),
      buffer_level_filter(new BufferLevelFilter),
      decoder_database(new DecoderDatabase(decoder_factory)),
      delay_peak_detector(new DelayPeakDetector(tick_timer.get())),
      delay_manager(new DelayManager(config.max_packets_in_buffer,
                                     delay_peak_detector.get(),
                                     tick_timer.get())),
      dtmf_buffer(new DtmfBuffer(config.sample_rate_hz)),
      dtmf_tone_generator(new DtmfToneGenerator),
      packet_buffer(
          new PacketBuffer(config.max_packets_in_buffer, tick_timer.get())),
      red_payload_splitter(new RedPayloadSplitter),
      timestamp_scaler(new TimestampScaler(*decoder_database)),
      accelerate_factory(new AccelerateFactory),
      expand_factory(new ExpandFactory),
      preemptive_expand_factory(new PreemptiveExpandFactory) {}

NetEqImpl::Dependencies::~Dependencies() = default;

NetEqImpl::NetEqImpl(const NetEq::Config& config,
                     Dependencies&& deps,
                     bool create_components)
    : tick_timer_(std::move(deps.tick_timer)),
      buffer_level_filter_(std::move(deps.buffer_level_filter)),
      decoder_database_(std::move(deps.decoder_database)),
      delay_manager_(std::move(deps.delay_manager)),
      delay_peak_detector_(std::move(deps.delay_peak_detector)),
      dtmf_buffer_(std::move(deps.dtmf_buffer)),
      dtmf_tone_generator_(std::move(deps.dtmf_tone_generator)),
      packet_buffer_(std::move(deps.packet_buffer)),
      red_payload_splitter_(std::move(deps.red_payload_splitter)),
      timestamp_scaler_(std::move(deps.timestamp_scaler)),
      vad_(new PostDecodeVad()),
      expand_factory_(std::move(deps.expand_factory)),
      accelerate_factory_(std::move(deps.accelerate_factory)),
      preemptive_expand_factory_(std::move(deps.preemptive_expand_factory)),
      last_mode_(kModeNormal),
      decoded_buffer_length_(kMaxFrameSize),
      decoded_buffer_(new int16_t[decoded_buffer_length_]),
      playout_timestamp_(0),
      new_codec_(false),
      timestamp_(0),
      reset_decoder_(false),
      ssrc_(0),
      first_packet_(true),
      background_noise_mode_(config.background_noise_mode),
      playout_mode_(config.playout_mode),
      enable_fast_accelerate_(config.enable_fast_accelerate),
      nack_enabled_(false),
      enable_muted_state_(config.enable_muted_state),
      use_dtx_delay_fix_(
          field_trial::IsEnabled("WebRTC-NetEqOpusDtxDelayFix")) {
  RTC_LOG(LS_INFO) << "NetEq config: " << config.ToString();
  int fs = config.sample_rate_hz;
  if (fs != 8000 && fs != 16000 && fs != 32000 && fs != 48000) {
    RTC_LOG(LS_ERROR) << "Sample rate " << fs << " Hz not supported. "
                      << "Changing to 8000 Hz.";
    fs = 8000;
  }
  delay_manager_->SetMaximumDelay(config.max_delay_ms);
  fs_hz_ = fs;
  fs_mult_ = fs / 8000;
  last_output_sample_rate_hz_ = fs;
  output_size_samples_ = static_cast<size_t>(kOutputSizeMs * 8 * fs_mult_);
  decoder_frame_length_ = 3 * output_size_samples_;
  WebRtcSpl_Init();
  if (create_components) {
    SetSampleRateAndChannels(fs, 1);  // Default is 1 channel.
  }
  RTC_DCHECK(!vad_->enabled());
  if (config.enable_post_decode_vad) {
    vad_->Enable();
  }
}

NetEqImpl::~NetEqImpl() = default;

int NetEqImpl::InsertPacket(const RTPHeader& rtp_header,
                            rtc::ArrayView<const uint8_t> payload,
                            uint32_t receive_timestamp) {
  rtc::MsanCheckInitialized(payload);
  TRACE_EVENT0("webrtc", "NetEqImpl::InsertPacket");
  rtc::CritScope lock(&crit_sect_);
  if (InsertPacketInternal(rtp_header, payload, receive_timestamp) != 0) {
    return kFail;
  }
  return kOK;
}

void NetEqImpl::InsertEmptyPacket(const RTPHeader& /*rtp_header*/) {
  // TODO(henrik.lundin) Handle NACK as well. This will make use of the
  // rtp_header parameter.
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=7611
  rtc::CritScope lock(&crit_sect_);
  delay_manager_->RegisterEmptyPacket();
}

namespace {
void SetAudioFrameActivityAndType(bool vad_enabled,
                                  NetEqImpl::OutputType type,
                                  AudioFrame::VADActivity last_vad_activity,
                                  AudioFrame* audio_frame) {
  switch (type) {
    case NetEqImpl::OutputType::kNormalSpeech: {
      audio_frame->speech_type_ = AudioFrame::kNormalSpeech;
      audio_frame->vad_activity_ = AudioFrame::kVadActive;
      break;
    }
    case NetEqImpl::OutputType::kVadPassive: {
      // This should only be reached if the VAD is enabled.
      RTC_DCHECK(vad_enabled);
      audio_frame->speech_type_ = AudioFrame::kNormalSpeech;
      audio_frame->vad_activity_ = AudioFrame::kVadPassive;
      break;
    }
    case NetEqImpl::OutputType::kCNG: {
      audio_frame->speech_type_ = AudioFrame::kCNG;
      audio_frame->vad_activity_ = AudioFrame::kVadPassive;
      break;
    }
    case NetEqImpl::OutputType::kPLC: {
      audio_frame->speech_type_ = AudioFrame::kPLC;
      audio_frame->vad_activity_ = last_vad_activity;
      break;
    }
    case NetEqImpl::OutputType::kPLCCNG: {
      audio_frame->speech_type_ = AudioFrame::kPLCCNG;
      audio_frame->vad_activity_ = AudioFrame::kVadPassive;
      break;
    }
    default:
      RTC_NOTREACHED();
  }
  if (!vad_enabled) {
    // Always set kVadUnknown when receive VAD is inactive.
    audio_frame->vad_activity_ = AudioFrame::kVadUnknown;
  }
}
}  // namespace

int NetEqImpl::GetAudio(AudioFrame* audio_frame, bool* muted) {
  TRACE_EVENT0("webrtc", "NetEqImpl::GetAudio");
  rtc::CritScope lock(&crit_sect_);
  if (GetAudioInternal(audio_frame, muted) != 0) {
    return kFail;
  }
  RTC_DCHECK_EQ(
      audio_frame->sample_rate_hz_,
      rtc::dchecked_cast<int>(audio_frame->samples_per_channel_ * 100));
  RTC_DCHECK_EQ(*muted, audio_frame->muted());
  SetAudioFrameActivityAndType(vad_->enabled(), LastOutputType(),
                               last_vad_activity_, audio_frame);
  last_vad_activity_ = audio_frame->vad_activity_;
  last_output_sample_rate_hz_ = audio_frame->sample_rate_hz_;
  RTC_DCHECK(last_output_sample_rate_hz_ == 8000 ||
             last_output_sample_rate_hz_ == 16000 ||
             last_output_sample_rate_hz_ == 32000 ||
             last_output_sample_rate_hz_ == 48000)
      << "Unexpected sample rate " << last_output_sample_rate_hz_;
  return kOK;
}

void NetEqImpl::SetCodecs(const std::map<int, SdpAudioFormat>& codecs) {
  rtc::CritScope lock(&crit_sect_);
  const std::vector<int> changed_payload_types =
      decoder_database_->SetCodecs(codecs);
  for (const int pt : changed_payload_types) {
    packet_buffer_->DiscardPacketsWithPayloadType(pt, &stats_);
  }
}

int NetEqImpl::RegisterPayloadType(NetEqDecoder codec,
                                   const std::string& name,
                                   uint8_t rtp_payload_type) {
  rtc::CritScope lock(&crit_sect_);
  RTC_LOG(LS_VERBOSE) << "RegisterPayloadType "
                      << static_cast<int>(rtp_payload_type) << " "
                      << static_cast<int>(codec);
  if (decoder_database_->RegisterPayload(rtp_payload_type, codec, name) !=
      DecoderDatabase::kOK) {
    return kFail;
  }
  return kOK;
}

int NetEqImpl::RegisterExternalDecoder(AudioDecoder* decoder,
                                       NetEqDecoder codec,
                                       const std::string& codec_name,
                                       uint8_t rtp_payload_type) {
  rtc::CritScope lock(&crit_sect_);
  RTC_LOG(LS_VERBOSE) << "RegisterExternalDecoder "
                      << static_cast<int>(rtp_payload_type) << " "
                      << static_cast<int>(codec);
  if (!decoder) {
    RTC_LOG(LS_ERROR) << "Cannot register external decoder with NULL pointer";
    assert(false);
    return kFail;
  }
  if (decoder_database_->InsertExternal(rtp_payload_type, codec, codec_name,
                                        decoder) != DecoderDatabase::kOK) {
    return kFail;
  }
  return kOK;
}

bool NetEqImpl::RegisterPayloadType(int rtp_payload_type,
                                    const SdpAudioFormat& audio_format) {
  RTC_LOG(LS_VERBOSE) << "NetEqImpl::RegisterPayloadType: payload type "
                      << rtp_payload_type << ", codec " << audio_format;
  rtc::CritScope lock(&crit_sect_);
  return decoder_database_->RegisterPayload(rtp_payload_type, audio_format) ==
         DecoderDatabase::kOK;
}

int NetEqImpl::RemovePayloadType(uint8_t rtp_payload_type) {
  rtc::CritScope lock(&crit_sect_);
  int ret = decoder_database_->Remove(rtp_payload_type);
  if (ret == DecoderDatabase::kOK || ret == DecoderDatabase::kDecoderNotFound) {
    packet_buffer_->DiscardPacketsWithPayloadType(rtp_payload_type, &stats_);
    return kOK;
  }
  return kFail;
}

void NetEqImpl::RemoveAllPayloadTypes() {
  rtc::CritScope lock(&crit_sect_);
  decoder_database_->RemoveAll();
}

bool NetEqImpl::SetMinimumDelay(int delay_ms) {
  rtc::CritScope lock(&crit_sect_);
  if (delay_ms >= 0 && delay_ms <= 10000) {
    assert(delay_manager_.get());
    return delay_manager_->SetMinimumDelay(delay_ms);
  }
  return false;
}

bool NetEqImpl::SetMaximumDelay(int delay_ms) {
  rtc::CritScope lock(&crit_sect_);
  if (delay_ms >= 0 && delay_ms <= 10000) {
    assert(delay_manager_.get());
    return delay_manager_->SetMaximumDelay(delay_ms);
  }
  return false;
}

int NetEqImpl::LeastRequiredDelayMs() const {
  rtc::CritScope lock(&crit_sect_);
  assert(delay_manager_.get());
  return delay_manager_->least_required_delay_ms();
}

int NetEqImpl::SetTargetDelay() {
  return kNotImplemented;
}

int NetEqImpl::TargetDelayMs() const {
  rtc::CritScope lock(&crit_sect_);
  RTC_DCHECK(delay_manager_.get());
  // The value from TargetLevel() is in number of packets, represented in Q8.
  const size_t target_delay_samples =
      (delay_manager_->TargetLevel() * decoder_frame_length_) >> 8;
  return static_cast<int>(target_delay_samples) /
         rtc::CheckedDivExact(fs_hz_, 1000);
}

int NetEqImpl::CurrentDelayMs() const {
  rtc::CritScope lock(&crit_sect_);
  if (fs_hz_ == 0)
    return 0;
  // Sum up the samples in the packet buffer with the future length of the sync
  // buffer, and divide the sum by the sample rate.
  const size_t delay_samples =
      packet_buffer_->NumSamplesInBuffer(decoder_frame_length_) +
      sync_buffer_->FutureLength();
  // The division below will truncate.
  const int delay_ms =
      static_cast<int>(delay_samples) / rtc::CheckedDivExact(fs_hz_, 1000);
  return delay_ms;
}

int NetEqImpl::FilteredCurrentDelayMs() const {
  rtc::CritScope lock(&crit_sect_);
  // Calculate the filtered packet buffer level in samples. The value from
  // |buffer_level_filter_| is in number of packets, represented in Q8.
  const size_t packet_buffer_samples =
      (buffer_level_filter_->filtered_current_level() *
       decoder_frame_length_) >>
      8;
  // Sum up the filtered packet buffer level with the future length of the sync
  // buffer, and divide the sum by the sample rate.
  const size_t delay_samples =
      packet_buffer_samples + sync_buffer_->FutureLength();
  // The division below will truncate. The return value is in ms.
  return static_cast<int>(delay_samples) / rtc::CheckedDivExact(fs_hz_, 1000);
}

// Deprecated.
// TODO(henrik.lundin) Delete.
void NetEqImpl::SetPlayoutMode(NetEqPlayoutMode mode) {
  rtc::CritScope lock(&crit_sect_);
  if (mode != playout_mode_) {
    playout_mode_ = mode;
    CreateDecisionLogic();
  }
}

// Deprecated.
// TODO(henrik.lundin) Delete.
NetEqPlayoutMode NetEqImpl::PlayoutMode() const {
  rtc::CritScope lock(&crit_sect_);
  return playout_mode_;
}

int NetEqImpl::NetworkStatistics(NetEqNetworkStatistics* stats) {
  rtc::CritScope lock(&crit_sect_);
  assert(decoder_database_.get());
  const size_t total_samples_in_buffers =
      packet_buffer_->NumSamplesInBuffer(decoder_frame_length_) +
      sync_buffer_->FutureLength();
  assert(delay_manager_.get());
  assert(decision_logic_.get());
  const int ms_per_packet = rtc::dchecked_cast<int>(
      decision_logic_->packet_length_samples() / (fs_hz_ / 1000));
  stats_.PopulateDelayManagerStats(ms_per_packet, *delay_manager_.get(), stats);
  stats_.GetNetworkStatistics(fs_hz_, total_samples_in_buffers,
                              decoder_frame_length_, stats);
  return 0;
}

NetEqLifetimeStatistics NetEqImpl::GetLifetimeStatistics() const {
  rtc::CritScope lock(&crit_sect_);
  return stats_.GetLifetimeStatistics();
}

void NetEqImpl::GetRtcpStatistics(RtcpStatistics* stats) {
  rtc::CritScope lock(&crit_sect_);
  if (stats) {
    rtcp_.GetStatistics(false, stats);
  }
}

void NetEqImpl::GetRtcpStatisticsNoReset(RtcpStatistics* stats) {
  rtc::CritScope lock(&crit_sect_);
  if (stats) {
    rtcp_.GetStatistics(true, stats);
  }
}

void NetEqImpl::EnableVad() {
  rtc::CritScope lock(&crit_sect_);
  assert(vad_.get());
  vad_->Enable();
}

void NetEqImpl::DisableVad() {
  rtc::CritScope lock(&crit_sect_);
  assert(vad_.get());
  vad_->Disable();
}

rtc::Optional<uint32_t> NetEqImpl::GetPlayoutTimestamp() const {
  rtc::CritScope lock(&crit_sect_);
  if (first_packet_ || last_mode_ == kModeRfc3389Cng ||
      last_mode_ == kModeCodecInternalCng) {
    // We don't have a valid RTP timestamp until we have decoded our first
    // RTP packet. Also, the RTP timestamp is not accurate while playing CNG,
    // which is indicated by returning an empty value.
    return rtc::nullopt;
  }
  return timestamp_scaler_->ToExternal(playout_timestamp_);
}

int NetEqImpl::last_output_sample_rate_hz() const {
  rtc::CritScope lock(&crit_sect_);
  return last_output_sample_rate_hz_;
}

rtc::Optional<CodecInst> NetEqImpl::GetDecoder(int payload_type) const {
  rtc::CritScope lock(&crit_sect_);
  const DecoderDatabase::DecoderInfo* di =
      decoder_database_->GetDecoderInfo(payload_type);
  if (!di) {
    return rtc::nullopt;
  }

  // Create a CodecInst with some fields set. The remaining fields are zeroed,
  // but we tell MSan to consider them uninitialized.
  CodecInst ci = {0};
  rtc::MsanMarkUninitialized(rtc::MakeArrayView(&ci, 1));
  ci.pltype = payload_type;
  std::strncpy(ci.plname, di->get_name().c_str(), sizeof(ci.plname));
  ci.plname[sizeof(ci.plname) - 1] = '\0';
  ci.plfreq = di->IsRed() ? 8000 : di->SampleRateHz();
  AudioDecoder* const decoder = di->GetDecoder();
  ci.channels = decoder ? decoder->Channels() : 1;
  return ci;
}

rtc::Optional<SdpAudioFormat> NetEqImpl::GetDecoderFormat(
    int payload_type) const {
  rtc::CritScope lock(&crit_sect_);
  const DecoderDatabase::DecoderInfo* const di =
      decoder_database_->GetDecoderInfo(payload_type);
  if (!di) {
    return rtc::nullopt;  // Payload type not registered.
  }
  return di->GetFormat();
}

int NetEqImpl::SetTargetNumberOfChannels() {
  return kNotImplemented;
}

int NetEqImpl::SetTargetSampleRate() {
  return kNotImplemented;
}

void NetEqImpl::FlushBuffers() {
  rtc::CritScope lock(&crit_sect_);
  RTC_LOG(LS_VERBOSE) << "FlushBuffers";
  packet_buffer_->Flush();
  assert(sync_buffer_.get());
  assert(expand_.get());
  sync_buffer_->Flush();
  sync_buffer_->set_next_index(sync_buffer_->next_index() -
                               expand_->overlap_length());
  // Set to wait for new codec.
  first_packet_ = true;
}

void NetEqImpl::PacketBufferStatistics(int* current_num_packets,
                                       int* max_num_packets) const {
  rtc::CritScope lock(&crit_sect_);
  packet_buffer_->BufferStat(current_num_packets, max_num_packets);
}

void NetEqImpl::EnableNack(size_t max_nack_list_size) {
  rtc::CritScope lock(&crit_sect_);
  if (!nack_enabled_) {
    const int kNackThresholdPackets = 2;
    nack_.reset(NackTracker::Create(kNackThresholdPackets));
    nack_enabled_ = true;
    nack_->UpdateSampleRate(fs_hz_);
  }
  nack_->SetMaxNackListSize(max_nack_list_size);
}

void NetEqImpl::DisableNack() {
  rtc::CritScope lock(&crit_sect_);
  nack_.reset();
  nack_enabled_ = false;
}

std::vector<uint16_t> NetEqImpl::GetNackList(int64_t round_trip_time_ms) const {
  rtc::CritScope lock(&crit_sect_);
  if (!nack_enabled_) {
    return std::vector<uint16_t>();
  }
  RTC_DCHECK(nack_.get());
  return nack_->GetNackList(round_trip_time_ms);
}

std::vector<uint32_t> NetEqImpl::LastDecodedTimestamps() const {
  rtc::CritScope lock(&crit_sect_);
  return last_decoded_timestamps_;
}

int NetEqImpl::SyncBufferSizeMs() const {
  rtc::CritScope lock(&crit_sect_);
  return rtc::dchecked_cast<int>(sync_buffer_->FutureLength() /
                                 rtc::CheckedDivExact(fs_hz_, 1000));
}

const SyncBuffer* NetEqImpl::sync_buffer_for_test() const {
  rtc::CritScope lock(&crit_sect_);
  return sync_buffer_.get();
}

Operations NetEqImpl::last_operation_for_test() const {
  rtc::CritScope lock(&crit_sect_);
  return last_operation_;
}

// Methods below this line are private.

int NetEqImpl::InsertPacketInternal(const RTPHeader& rtp_header,
                                    rtc::ArrayView<const uint8_t> payload,
                                    uint32_t receive_timestamp) {
  if (payload.empty()) {
    RTC_LOG_F(LS_ERROR) << "payload is empty";
    return kInvalidPointer;
  }

  PacketList packet_list;
  // Insert packet in a packet list.
  packet_list.push_back([&rtp_header, &payload] {
    // Convert to Packet.
    Packet packet;
    packet.payload_type = rtp_header.payloadType;
    packet.sequence_number = rtp_header.sequenceNumber;
    packet.timestamp = rtp_header.timestamp;
    packet.payload.SetData(payload.data(), payload.size());
    // Waiting time will be set upon inserting the packet in the buffer.
    RTC_DCHECK(!packet.waiting_time);
    return packet;
  }());

  bool update_sample_rate_and_channels =
      first_packet_ || (rtp_header.ssrc != ssrc_);

  if (update_sample_rate_and_channels) {
    // Reset timestamp scaling.
    timestamp_scaler_->Reset();
  }

  if (!decoder_database_->IsRed(rtp_header.payloadType)) {
    // Scale timestamp to internal domain (only for some codecs).
    timestamp_scaler_->ToInternal(&packet_list);
  }

  // Store these for later use, since the first packet may very well disappear
  // before we need these values.
  uint32_t main_timestamp = packet_list.front().timestamp;
  uint8_t main_payload_type = packet_list.front().payload_type;
  uint16_t main_sequence_number = packet_list.front().sequence_number;

  // Reinitialize NetEq if it's needed (changed SSRC or first call).
  if (update_sample_rate_and_channels) {
    // Note: |first_packet_| will be cleared further down in this method, once
    // the packet has been successfully inserted into the packet buffer.

    rtcp_.Init(rtp_header.sequenceNumber);

    // Flush the packet buffer and DTMF buffer.
    packet_buffer_->Flush();
    dtmf_buffer_->Flush();

    // Store new SSRC.
    ssrc_ = rtp_header.ssrc;

    // Update audio buffer timestamp.
    sync_buffer_->IncreaseEndTimestamp(main_timestamp - timestamp_);

    // Update codecs.
    timestamp_ = main_timestamp;
  }

  // Update RTCP statistics, only for regular packets.
  rtcp_.Update(rtp_header, receive_timestamp);

  if (nack_enabled_) {
    RTC_DCHECK(nack_);
    if (update_sample_rate_and_channels) {
      nack_->Reset();
    }
    nack_->UpdateLastReceivedPacket(rtp_header.sequenceNumber,
                                    rtp_header.timestamp);
  }

  // Check for RED payload type, and separate payloads into several packets.
  if (decoder_database_->IsRed(rtp_header.payloadType)) {
    if (!red_payload_splitter_->SplitRed(&packet_list)) {
      return kRedundancySplitError;
    }
    // Only accept a few RED payloads of the same type as the main data,
    // DTMF events and CNG.
    red_payload_splitter_->CheckRedPayloads(&packet_list, *decoder_database_);
  }

  // Check payload types.
  if (decoder_database_->CheckPayloadTypes(packet_list) ==
      DecoderDatabase::kDecoderNotFound) {
    return kUnknownRtpPayloadType;
  }

  RTC_DCHECK(!packet_list.empty());

  // Update main_timestamp, if new packets appear in the list
  // after RED splitting.
  if (decoder_database_->IsRed(rtp_header.payloadType)) {
    timestamp_scaler_->ToInternal(&packet_list);
    main_timestamp = packet_list.front().timestamp;
    main_payload_type = packet_list.front().payload_type;
    main_sequence_number = packet_list.front().sequence_number;
  }

  // Process DTMF payloads. Cycle through the list of packets, and pick out any
  // DTMF payloads found.
  PacketList::iterator it = packet_list.begin();
  while (it != packet_list.end()) {
    const Packet& current_packet = (*it);
    RTC_DCHECK(!current_packet.payload.empty());
    if (decoder_database_->IsDtmf(current_packet.payload_type)) {
      DtmfEvent event;
      int ret = DtmfBuffer::ParseEvent(current_packet.timestamp,
                                       current_packet.payload.data(),
                                       current_packet.payload.size(), &event);
      if (ret != DtmfBuffer::kOK) {
        return kDtmfParsingError;
      }
      if (dtmf_buffer_->InsertEvent(event) != DtmfBuffer::kOK) {
        return kDtmfInsertError;
      }
      it = packet_list.erase(it);
    } else {
      ++it;
    }
  }

  // Update bandwidth estimate, if the packet is not comfort noise.
  if (!packet_list.empty() &&
      !decoder_database_->IsComfortNoise(main_payload_type)) {
    // The list can be empty here if we got nothing but DTMF payloads.
    AudioDecoder* decoder = decoder_database_->GetDecoder(main_payload_type);
    RTC_DCHECK(decoder);  // Should always get a valid object, since we have
                          // already checked that the payload types are known.
    decoder->IncomingPacket(packet_list.front().payload.data(),
                            packet_list.front().payload.size(),
                            packet_list.front().sequence_number,
                            packet_list.front().timestamp,
                            receive_timestamp);
  }

  PacketList parsed_packet_list;
  while (!packet_list.empty()) {
    Packet& packet = packet_list.front();
    const DecoderDatabase::DecoderInfo* info =
        decoder_database_->GetDecoderInfo(packet.payload_type);
    if (!info) {
      RTC_LOG(LS_WARNING) << "SplitAudio unknown payload type";
      return kUnknownRtpPayloadType;
    }

    if (info->IsComfortNoise()) {
      // Carry comfort noise packets along.
      parsed_packet_list.splice(parsed_packet_list.end(), packet_list,
                                packet_list.begin());
    } else {
      const auto sequence_number = packet.sequence_number;
      const auto payload_type = packet.payload_type;
      const Packet::Priority original_priority = packet.priority;
      auto packet_from_result = [&] (AudioDecoder::ParseResult& result) {
        Packet new_packet;
        new_packet.sequence_number = sequence_number;
        new_packet.payload_type = payload_type;
        new_packet.timestamp = result.timestamp;
        new_packet.priority.codec_level = result.priority;
        new_packet.priority.red_level = original_priority.red_level;
        new_packet.frame = std::move(result.frame);
        return new_packet;
      };

      std::vector<AudioDecoder::ParseResult> results =
          info->GetDecoder()->ParsePayload(std::move(packet.payload),
                                           packet.timestamp);
      if (results.empty()) {
        packet_list.pop_front();
      } else {
        bool first = true;
        for (auto& result : results) {
          RTC_DCHECK(result.frame);
          RTC_DCHECK_GE(result.priority, 0);
          if (first) {
            // Re-use the node and move it to parsed_packet_list.
            packet_list.front() = packet_from_result(result);
            parsed_packet_list.splice(parsed_packet_list.end(), packet_list,
                                      packet_list.begin());
            first = false;
          } else {
            parsed_packet_list.push_back(packet_from_result(result));
          }
        }
      }
    }
  }

  // Calculate the number of primary (non-FEC/RED) packets.
  const int number_of_primary_packets = std::count_if(
      parsed_packet_list.begin(), parsed_packet_list.end(),
      [](const Packet& in) { return in.priority.codec_level == 0; });

  // Insert packets in buffer.
  const int ret = packet_buffer_->InsertPacketList(
      &parsed_packet_list, *decoder_database_, &current_rtp_payload_type_,
      &current_cng_rtp_payload_type_, &stats_);
  if (ret == PacketBuffer::kFlushed) {
    // Reset DSP timestamp etc. if packet buffer flushed.
    new_codec_ = true;
    update_sample_rate_and_channels = true;
  } else if (ret != PacketBuffer::kOK) {
    return kOtherError;
  }

  if (first_packet_) {
    first_packet_ = false;
    // Update the codec on the next GetAudio call.
    new_codec_ = true;
  }

  if (current_rtp_payload_type_) {
    RTC_DCHECK(decoder_database_->GetDecoderInfo(*current_rtp_payload_type_))
        << "Payload type " << static_cast<int>(*current_rtp_payload_type_)
        << " is unknown where it shouldn't be";
  }

  if (update_sample_rate_and_channels && !packet_buffer_->Empty()) {
    // We do not use |current_rtp_payload_type_| to |set payload_type|, but
    // get the next RTP header from |packet_buffer_| to obtain the payload type.
    // The reason for it is the following corner case. If NetEq receives a
    // CNG packet with a sample rate different than the current CNG then it
    // flushes its buffer, assuming send codec must have been changed. However,
    // payload type of the hypothetically new send codec is not known.
    const Packet* next_packet = packet_buffer_->PeekNextPacket();
    RTC_DCHECK(next_packet);
    const int payload_type = next_packet->payload_type;
    size_t channels = 1;
    if (!decoder_database_->IsComfortNoise(payload_type)) {
      AudioDecoder* decoder = decoder_database_->GetDecoder(payload_type);
      assert(decoder);  // Payloads are already checked to be valid.
      channels = decoder->Channels();
    }
    const DecoderDatabase::DecoderInfo* decoder_info =
        decoder_database_->GetDecoderInfo(payload_type);
    assert(decoder_info);
    if (decoder_info->SampleRateHz() != fs_hz_ ||
        channels != algorithm_buffer_->Channels()) {
      SetSampleRateAndChannels(decoder_info->SampleRateHz(),
                               channels);
    }
    if (nack_enabled_) {
      RTC_DCHECK(nack_);
      // Update the sample rate even if the rate is not new, because of Reset().
      nack_->UpdateSampleRate(fs_hz_);
    }
  }

  // TODO(hlundin): Move this code to DelayManager class.
  const DecoderDatabase::DecoderInfo* dec_info =
      decoder_database_->GetDecoderInfo(main_payload_type);
  assert(dec_info);  // Already checked that the payload type is known.
  delay_manager_->LastDecodedWasCngOrDtmf(dec_info->IsComfortNoise() ||
                                          dec_info->IsDtmf());
  if (delay_manager_->last_pack_cng_or_dtmf() == 0) {
    // Calculate the total speech length carried in each packet.
    if (number_of_primary_packets > 0) {
      const size_t packet_length_samples =
          number_of_primary_packets * decoder_frame_length_;
      if (packet_length_samples != decision_logic_->packet_length_samples()) {
        decision_logic_->set_packet_length_samples(packet_length_samples);
        delay_manager_->SetPacketAudioLength(
            rtc::dchecked_cast<int>((1000 * packet_length_samples) / fs_hz_));
      }
    }

    // Update statistics.
    if ((int32_t)(main_timestamp - timestamp_) >= 0 && !new_codec_) {
      // Only update statistics if incoming packet is not older than last played
      // out packet, and if new codec flag is not set.
      delay_manager_->Update(main_sequence_number, main_timestamp, fs_hz_);
    }
  } else if (delay_manager_->last_pack_cng_or_dtmf() == -1) {
    // This is first "normal" packet after CNG or DTMF.
    // Reset packet time counter and measure time until next packet,
    // but don't update statistics.
    delay_manager_->set_last_pack_cng_or_dtmf(0);
    delay_manager_->ResetPacketIatCount();
  }
  return 0;
}

int NetEqImpl::GetAudioInternal(AudioFrame* audio_frame, bool* muted) {
  PacketList packet_list;
  DtmfEvent dtmf_event;
  Operations operation;
  bool play_dtmf;
  *muted = false;
  last_decoded_timestamps_.clear();
  tick_timer_->Increment();
  stats_.IncreaseCounter(output_size_samples_, fs_hz_);

  // Check for muted state.
  if (enable_muted_state_ && expand_->Muted() && packet_buffer_->Empty()) {
    RTC_DCHECK_EQ(last_mode_, kModeExpand);
    audio_frame->Reset();
    RTC_DCHECK(audio_frame->muted());  // Reset() should mute the frame.
    playout_timestamp_ += static_cast<uint32_t>(output_size_samples_);
    audio_frame->sample_rate_hz_ = fs_hz_;
    audio_frame->samples_per_channel_ = output_size_samples_;
    audio_frame->timestamp_ =
        first_packet_
            ? 0
            : timestamp_scaler_->ToExternal(playout_timestamp_) -
                  static_cast<uint32_t>(audio_frame->samples_per_channel_);
    audio_frame->num_channels_ = sync_buffer_->Channels();
    stats_.ExpandedNoiseSamples(output_size_samples_, false);
    *muted = true;
    return 0;
  }

  int return_value = GetDecision(&operation, &packet_list, &dtmf_event,
                                 &play_dtmf);
  if (return_value != 0) {
    last_mode_ = kModeError;
    return return_value;
  }

  AudioDecoder::SpeechType speech_type;
  int length = 0;
  const size_t start_num_packets = packet_list.size();
  int decode_return_value = Decode(&packet_list, &operation,
                                   &length, &speech_type);

  assert(vad_.get());
  bool sid_frame_available =
      (operation == kRfc3389Cng && !packet_list.empty());
  vad_->Update(decoded_buffer_.get(), static_cast<size_t>(length), speech_type,
               sid_frame_available, fs_hz_);

  // This is the criterion that we did decode some data through the speech
  // decoder, and the operation resulted in comfort noise.
  const bool codec_internal_sid_frame =
      use_dtx_delay_fix_ ? (speech_type == AudioDecoder::kComfortNoise &&
                            start_num_packets > packet_list.size())
                         : (speech_type == AudioDecoder::kComfortNoise);

  if (sid_frame_available || codec_internal_sid_frame) {
    // Start a new stopwatch since we are decoding a new CNG packet.
    generated_noise_stopwatch_ = tick_timer_->GetNewStopwatch();
  }

  algorithm_buffer_->Clear();
  switch (operation) {
    case kNormal: {
      DoNormal(decoded_buffer_.get(), length, speech_type, play_dtmf);
      break;
    }
    case kMerge: {
      DoMerge(decoded_buffer_.get(), length, speech_type, play_dtmf);
      break;
    }
    case kExpand: {
      return_value = DoExpand(play_dtmf);
      break;
    }
    case kAccelerate:
    case kFastAccelerate: {
      const bool fast_accelerate =
          enable_fast_accelerate_ && (operation == kFastAccelerate);
      return_value = DoAccelerate(decoded_buffer_.get(), length, speech_type,
                                  play_dtmf, fast_accelerate);
      break;
    }
    case kPreemptiveExpand: {
      return_value = DoPreemptiveExpand(decoded_buffer_.get(), length,
                                        speech_type, play_dtmf);
      break;
    }
    case kRfc3389Cng:
    case kRfc3389CngNoPacket: {
      return_value = DoRfc3389Cng(&packet_list, play_dtmf);
      break;
    }
    case kCodecInternalCng: {
      // This handles the case when there is no transmission and the decoder
      // should produce internal comfort noise.
      // TODO(hlundin): Write test for codec-internal CNG.
      DoCodecInternalCng(decoded_buffer_.get(), length);
      break;
    }
    case kDtmf: {
      // TODO(hlundin): Write test for this.
      return_value = DoDtmf(dtmf_event, &play_dtmf);
      break;
    }
    case kAlternativePlc: {
      // TODO(hlundin): Write test for this.
      DoAlternativePlc(false);
      break;
    }
    case kAlternativePlcIncreaseTimestamp: {
      // TODO(hlundin): Write test for this.
      DoAlternativePlc(true);
      break;
    }
    case kAudioRepetitionIncreaseTimestamp: {
      // TODO(hlundin): Write test for this.
      sync_buffer_->IncreaseEndTimestamp(
          static_cast<uint32_t>(output_size_samples_));
      // Skipping break on purpose. Execution should move on into the
      // next case.
      FALLTHROUGH();
    }
    case kAudioRepetition: {
      // TODO(hlundin): Write test for this.
      // Copy last |output_size_samples_| from |sync_buffer_| to
      // |algorithm_buffer|.
      algorithm_buffer_->PushBackFromIndex(
          *sync_buffer_, sync_buffer_->Size() - output_size_samples_);
      expand_->Reset();
      break;
    }
    case kUndefined: {
      RTC_LOG(LS_ERROR) << "Invalid operation kUndefined.";
      assert(false);  // This should not happen.
      last_mode_ = kModeError;
      return kInvalidOperation;
    }
  }  // End of switch.
  last_operation_ = operation;
  if (return_value < 0) {
    return return_value;
  }

  if (last_mode_ != kModeRfc3389Cng) {
    comfort_noise_->Reset();
  }

  // Copy from |algorithm_buffer| to |sync_buffer_|.
  sync_buffer_->PushBack(*algorithm_buffer_);

  // Extract data from |sync_buffer_| to |output|.
  size_t num_output_samples_per_channel = output_size_samples_;
  size_t num_output_samples = output_size_samples_ * sync_buffer_->Channels();
  if (num_output_samples > AudioFrame::kMaxDataSizeSamples) {
    RTC_LOG(LS_WARNING) << "Output array is too short. "
                        << AudioFrame::kMaxDataSizeSamples << " < "
                        << output_size_samples_ << " * "
                        << sync_buffer_->Channels();
    num_output_samples = AudioFrame::kMaxDataSizeSamples;
    num_output_samples_per_channel =
        AudioFrame::kMaxDataSizeSamples / sync_buffer_->Channels();
  }
  sync_buffer_->GetNextAudioInterleaved(num_output_samples_per_channel,
                                        audio_frame);
  audio_frame->sample_rate_hz_ = fs_hz_;
  if (sync_buffer_->FutureLength() < expand_->overlap_length()) {
    // The sync buffer should always contain |overlap_length| samples, but now
    // too many samples have been extracted. Reinstall the |overlap_length|
    // lookahead by moving the index.
    const size_t missing_lookahead_samples =
        expand_->overlap_length() - sync_buffer_->FutureLength();
    RTC_DCHECK_GE(sync_buffer_->next_index(), missing_lookahead_samples);
    sync_buffer_->set_next_index(sync_buffer_->next_index() -
                                 missing_lookahead_samples);
  }
  if (audio_frame->samples_per_channel_ != output_size_samples_) {
    RTC_LOG(LS_ERROR) << "audio_frame->samples_per_channel_ ("
                      << audio_frame->samples_per_channel_
                      << ") != output_size_samples_ (" << output_size_samples_
                      << ")";
    // TODO(minyue): treatment of under-run, filling zeros
    audio_frame->Mute();
    return kSampleUnderrun;
  }

  // Should always have overlap samples left in the |sync_buffer_|.
  RTC_DCHECK_GE(sync_buffer_->FutureLength(), expand_->overlap_length());

  // TODO(yujo): For muted frames, this can be a copy rather than an addition.
  if (play_dtmf) {
    return_value = DtmfOverdub(dtmf_event, sync_buffer_->Channels(),
                               audio_frame->mutable_data());
  }

  // Update the background noise parameters if last operation wrote data
  // straight from the decoder to the |sync_buffer_|. That is, none of the
  // operations that modify the signal can be followed by a parameter update.
  if ((last_mode_ == kModeNormal) ||
      (last_mode_ == kModeAccelerateFail) ||
      (last_mode_ == kModePreemptiveExpandFail) ||
      (last_mode_ == kModeRfc3389Cng) ||
      (last_mode_ == kModeCodecInternalCng)) {
    background_noise_->Update(*sync_buffer_, *vad_.get());
  }

  if (operation == kDtmf) {
    // DTMF data was written the end of |sync_buffer_|.
    // Update index to end of DTMF data in |sync_buffer_|.
    sync_buffer_->set_dtmf_index(sync_buffer_->Size());
  }

  if (last_mode_ != kModeExpand) {
    // If last operation was not expand, calculate the |playout_timestamp_| from
    // the |sync_buffer_|. However, do not update the |playout_timestamp_| if it
    // would be moved "backwards".
    uint32_t temp_timestamp = sync_buffer_->end_timestamp() -
        static_cast<uint32_t>(sync_buffer_->FutureLength());
    if (static_cast<int32_t>(temp_timestamp - playout_timestamp_) > 0) {
      playout_timestamp_ = temp_timestamp;
    }
  } else {
    // Use dead reckoning to estimate the |playout_timestamp_|.
    playout_timestamp_ += static_cast<uint32_t>(output_size_samples_);
  }
  // Set the timestamp in the audio frame to zero before the first packet has
  // been inserted. Otherwise, subtract the frame size in samples to get the
  // timestamp of the first sample in the frame (playout_timestamp_ is the
  // last + 1).
  audio_frame->timestamp_ =
      first_packet_
          ? 0
          : timestamp_scaler_->ToExternal(playout_timestamp_) -
                static_cast<uint32_t>(audio_frame->samples_per_channel_);

  if (!(last_mode_ == kModeRfc3389Cng ||
      last_mode_ == kModeCodecInternalCng ||
      last_mode_ == kModeExpand)) {
    generated_noise_stopwatch_.reset();
  }

  if (decode_return_value) return decode_return_value;
  return return_value;
}

int NetEqImpl::GetDecision(Operations* operation,
                           PacketList* packet_list,
                           DtmfEvent* dtmf_event,
                           bool* play_dtmf) {
  // Initialize output variables.
  *play_dtmf = false;
  *operation = kUndefined;

  assert(sync_buffer_.get());
  uint32_t end_timestamp = sync_buffer_->end_timestamp();
  if (!new_codec_) {
    const uint32_t five_seconds_samples = 5 * fs_hz_;
    packet_buffer_->DiscardOldPackets(end_timestamp, five_seconds_samples,
                                      &stats_);
  }
  const Packet* packet = packet_buffer_->PeekNextPacket();

  RTC_DCHECK(!generated_noise_stopwatch_ ||
             generated_noise_stopwatch_->ElapsedTicks() >= 1);
  uint64_t generated_noise_samples =
      generated_noise_stopwatch_
          ? (generated_noise_stopwatch_->ElapsedTicks() - 1) *
                    output_size_samples_ +
                decision_logic_->noise_fast_forward()
          : 0;

  if (decision_logic_->CngRfc3389On() || last_mode_ == kModeRfc3389Cng) {
    // Because of timestamp peculiarities, we have to "manually" disallow using
    // a CNG packet with the same timestamp as the one that was last played.
    // This can happen when using redundancy and will cause the timing to shift.
    while (packet && decoder_database_->IsComfortNoise(packet->payload_type) &&
           (end_timestamp >= packet->timestamp ||
            end_timestamp + generated_noise_samples > packet->timestamp)) {
      // Don't use this packet, discard it.
      if (packet_buffer_->DiscardNextPacket(&stats_) != PacketBuffer::kOK) {
        assert(false);  // Must be ok by design.
      }
      // Check buffer again.
      if (!new_codec_) {
        packet_buffer_->DiscardOldPackets(end_timestamp, 5 * fs_hz_, &stats_);
      }
      packet = packet_buffer_->PeekNextPacket();
    }
  }

  assert(expand_.get());
  const int samples_left = static_cast<int>(sync_buffer_->FutureLength() -
      expand_->overlap_length());
  if (last_mode_ == kModeAccelerateSuccess ||
      last_mode_ == kModeAccelerateLowEnergy ||
      last_mode_ == kModePreemptiveExpandSuccess ||
      last_mode_ == kModePreemptiveExpandLowEnergy) {
    // Subtract (samples_left + output_size_samples_) from sampleMemory.
    decision_logic_->AddSampleMemory(
        -(samples_left + rtc::dchecked_cast<int>(output_size_samples_)));
  }

  // Check if it is time to play a DTMF event.
  if (dtmf_buffer_->GetEvent(
      static_cast<uint32_t>(
          end_timestamp + generated_noise_samples),
      dtmf_event)) {
    *play_dtmf = true;
  }

  // Get instruction.
  assert(sync_buffer_.get());
  assert(expand_.get());
  generated_noise_samples =
      generated_noise_stopwatch_
          ? generated_noise_stopwatch_->ElapsedTicks() * output_size_samples_ +
                decision_logic_->noise_fast_forward()
          : 0;
  *operation = decision_logic_->GetDecision(
      *sync_buffer_, *expand_, decoder_frame_length_, packet, last_mode_,
      *play_dtmf, generated_noise_samples, &reset_decoder_);

  // Check if we already have enough samples in the |sync_buffer_|. If so,
  // change decision to normal, unless the decision was merge, accelerate, or
  // preemptive expand.
  if (samples_left >= rtc::dchecked_cast<int>(output_size_samples_) &&
      *operation != kMerge && *operation != kAccelerate &&
      *operation != kFastAccelerate && *operation != kPreemptiveExpand) {
    *operation = kNormal;
    return 0;
  }

  decision_logic_->ExpandDecision(*operation);

  // Check conditions for reset.
  if (new_codec_ || *operation == kUndefined) {
    // The only valid reason to get kUndefined is that new_codec_ is set.
    assert(new_codec_);
    if (*play_dtmf && !packet) {
      timestamp_ = dtmf_event->timestamp;
    } else {
      if (!packet) {
        RTC_LOG(LS_ERROR) << "Packet missing where it shouldn't.";
        return -1;
      }
      timestamp_ = packet->timestamp;
      if (*operation == kRfc3389CngNoPacket &&
          decoder_database_->IsComfortNoise(packet->payload_type)) {
        // Change decision to CNG packet, since we do have a CNG packet, but it
        // was considered too early to use. Now, use it anyway.
        *operation = kRfc3389Cng;
      } else if (*operation != kRfc3389Cng) {
        *operation = kNormal;
      }
    }
    // Adjust |sync_buffer_| timestamp before setting |end_timestamp| to the
    // new value.
    sync_buffer_->IncreaseEndTimestamp(timestamp_ - end_timestamp);
    end_timestamp = timestamp_;
    new_codec_ = false;
    decision_logic_->SoftReset();
    buffer_level_filter_->Reset();
    delay_manager_->Reset();
    stats_.ResetMcu();
  }

  size_t required_samples = output_size_samples_;
  const size_t samples_10_ms = static_cast<size_t>(80 * fs_mult_);
  const size_t samples_20_ms = 2 * samples_10_ms;
  const size_t samples_30_ms = 3 * samples_10_ms;

  switch (*operation) {
    case kExpand: {
      timestamp_ = end_timestamp;
      return 0;
    }
    case kRfc3389CngNoPacket:
    case kCodecInternalCng: {
      return 0;
    }
    case kDtmf: {
      // TODO(hlundin): Write test for this.
      // Update timestamp.
      timestamp_ = end_timestamp;
      const uint64_t generated_noise_samples =
          generated_noise_stopwatch_
              ? generated_noise_stopwatch_->ElapsedTicks() *
                        output_size_samples_ +
                    decision_logic_->noise_fast_forward()
              : 0;
      if (generated_noise_samples > 0 && last_mode_ != kModeDtmf) {
        // Make a jump in timestamp due to the recently played comfort noise.
        uint32_t timestamp_jump =
            static_cast<uint32_t>(generated_noise_samples);
        sync_buffer_->IncreaseEndTimestamp(timestamp_jump);
        timestamp_ += timestamp_jump;
      }
      return 0;
    }
    case kAccelerate:
    case kFastAccelerate: {
      // In order to do an accelerate we need at least 30 ms of audio data.
      if (samples_left >= static_cast<int>(samples_30_ms)) {
        // Already have enough data, so we do not need to extract any more.
        decision_logic_->set_sample_memory(samples_left);
        decision_logic_->set_prev_time_scale(true);
        return 0;
      } else if (samples_left >= static_cast<int>(samples_10_ms) &&
          decoder_frame_length_ >= samples_30_ms) {
        // Avoid decoding more data as it might overflow the playout buffer.
        *operation = kNormal;
        return 0;
      } else if (samples_left < static_cast<int>(samples_20_ms) &&
          decoder_frame_length_ < samples_30_ms) {
        // Build up decoded data by decoding at least 20 ms of audio data. Do
        // not perform accelerate yet, but wait until we only need to do one
        // decoding.
        required_samples = 2 * output_size_samples_;
        *operation = kNormal;
      }
      // If none of the above is true, we have one of two possible situations:
      // (1) 20 ms <= samples_left < 30 ms and decoder_frame_length_ < 30 ms; or
      // (2) samples_left < 10 ms and decoder_frame_length_ >= 30 ms.
      // In either case, we move on with the accelerate decision, and decode one
      // frame now.
      break;
    }
    case kPreemptiveExpand: {
      // In order to do a preemptive expand we need at least 30 ms of decoded
      // audio data.
      if ((samples_left >= static_cast<int>(samples_30_ms)) ||
          (samples_left >= static_cast<int>(samples_10_ms) &&
              decoder_frame_length_ >= samples_30_ms)) {
        // Already have enough data, so we do not need to extract any more.
        // Or, avoid decoding more data as it might overflow the playout buffer.
        // Still try preemptive expand, though.
        decision_logic_->set_sample_memory(samples_left);
        decision_logic_->set_prev_time_scale(true);
        return 0;
      }
      if (samples_left < static_cast<int>(samples_20_ms) &&
          decoder_frame_length_ < samples_30_ms) {
        // Build up decoded data by decoding at least 20 ms of audio data.
        // Still try to perform preemptive expand.
        required_samples = 2 * output_size_samples_;
      }
      // Move on with the preemptive expand decision.
      break;
    }
    case kMerge: {
      required_samples =
          std::max(merge_->RequiredFutureSamples(), required_samples);
      break;
    }
    default: {
      // Do nothing.
    }
  }

  // Get packets from buffer.
  int extracted_samples = 0;
  if (packet && *operation != kAlternativePlc &&
      *operation != kAlternativePlcIncreaseTimestamp &&
      *operation != kAudioRepetition &&
      *operation != kAudioRepetitionIncreaseTimestamp) {
    sync_buffer_->IncreaseEndTimestamp(packet->timestamp - end_timestamp);
    if (decision_logic_->CngOff()) {
      // Adjustment of timestamp only corresponds to an actual packet loss
      // if comfort noise is not played. If comfort noise was just played,
      // this adjustment of timestamp is only done to get back in sync with the
      // stream timestamp; no loss to report.
      stats_.LostSamples(packet->timestamp - end_timestamp);
    }

    if (*operation != kRfc3389Cng) {
      // We are about to decode and use a non-CNG packet.
      decision_logic_->SetCngOff();
    }

    extracted_samples = ExtractPackets(required_samples, packet_list);
    if (extracted_samples < 0) {
      return kPacketBufferCorruption;
    }
  }

  if (*operation == kAccelerate || *operation == kFastAccelerate ||
      *operation == kPreemptiveExpand) {
    decision_logic_->set_sample_memory(samples_left + extracted_samples);
    decision_logic_->set_prev_time_scale(true);
  }

  if (*operation == kAccelerate || *operation == kFastAccelerate) {
    // Check that we have enough data (30ms) to do accelerate.
    if (extracted_samples + samples_left < static_cast<int>(samples_30_ms)) {
      // TODO(hlundin): Write test for this.
      // Not enough, do normal operation instead.
      *operation = kNormal;
    }
  }

  timestamp_ = end_timestamp;
  return 0;
}

int NetEqImpl::Decode(PacketList* packet_list, Operations* operation,
                      int* decoded_length,
                      AudioDecoder::SpeechType* speech_type) {
  *speech_type = AudioDecoder::kSpeech;

  // When packet_list is empty, we may be in kCodecInternalCng mode, and for
  // that we use current active decoder.
  AudioDecoder* decoder = decoder_database_->GetActiveDecoder();

  if (!packet_list->empty()) {
    const Packet& packet = packet_list->front();
    uint8_t payload_type = packet.payload_type;
    if (!decoder_database_->IsComfortNoise(payload_type)) {
      decoder = decoder_database_->GetDecoder(payload_type);
      assert(decoder);
      if (!decoder) {
        RTC_LOG(LS_WARNING)
            << "Unknown payload type " << static_cast<int>(payload_type);
        packet_list->clear();
        return kDecoderNotFound;
      }
      bool decoder_changed;
      decoder_database_->SetActiveDecoder(payload_type, &decoder_changed);
      if (decoder_changed) {
        // We have a new decoder. Re-init some values.
        const DecoderDatabase::DecoderInfo* decoder_info = decoder_database_
            ->GetDecoderInfo(payload_type);
        assert(decoder_info);
        if (!decoder_info) {
          RTC_LOG(LS_WARNING)
              << "Unknown payload type " << static_cast<int>(payload_type);
          packet_list->clear();
          return kDecoderNotFound;
        }
        // If sampling rate or number of channels has changed, we need to make
        // a reset.
        if (decoder_info->SampleRateHz() != fs_hz_ ||
            decoder->Channels() != algorithm_buffer_->Channels()) {
          // TODO(tlegrand): Add unittest to cover this event.
          SetSampleRateAndChannels(decoder_info->SampleRateHz(),
                                   decoder->Channels());
        }
        sync_buffer_->set_end_timestamp(timestamp_);
        playout_timestamp_ = timestamp_;
      }
    }
  }

  if (reset_decoder_) {
    // TODO(hlundin): Write test for this.
    if (decoder)
      decoder->Reset();

    // Reset comfort noise decoder.
    ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();
    if (cng_decoder)
      cng_decoder->Reset();

    reset_decoder_ = false;
  }

  *decoded_length = 0;
  // Update codec-internal PLC state.
  if ((*operation == kMerge) && decoder && decoder->HasDecodePlc()) {
    decoder->DecodePlc(1, &decoded_buffer_[*decoded_length]);
  }

  int return_value;
  if (*operation == kCodecInternalCng) {
    RTC_DCHECK(packet_list->empty());
    return_value = DecodeCng(decoder, decoded_length, speech_type);
  } else {
    return_value = DecodeLoop(packet_list, *operation, decoder,
                              decoded_length, speech_type);
  }

  if (*decoded_length < 0) {
    // Error returned from the decoder.
    *decoded_length = 0;
    sync_buffer_->IncreaseEndTimestamp(
        static_cast<uint32_t>(decoder_frame_length_));
    int error_code = 0;
    if (decoder)
      error_code = decoder->ErrorCode();
    if (error_code != 0) {
      // Got some error code from the decoder.
      return_value = kDecoderErrorCode;
      RTC_LOG(LS_WARNING) << "Decoder returned error code: " << error_code;
    } else {
      // Decoder does not implement error codes. Return generic error.
      return_value = kOtherDecoderError;
      RTC_LOG(LS_WARNING) << "Decoder error (no error code)";
    }
    *operation = kExpand;  // Do expansion to get data instead.
  }
  if (*speech_type != AudioDecoder::kComfortNoise) {
    // Don't increment timestamp if codec returned CNG speech type
    // since in this case, the we will increment the CNGplayedTS counter.
    // Increase with number of samples per channel.
    assert(*decoded_length == 0 ||
           (decoder && decoder->Channels() == sync_buffer_->Channels()));
    sync_buffer_->IncreaseEndTimestamp(
        *decoded_length / static_cast<int>(sync_buffer_->Channels()));
  }
  return return_value;
}

int NetEqImpl::DecodeCng(AudioDecoder* decoder, int* decoded_length,
                         AudioDecoder::SpeechType* speech_type) {
  if (!decoder) {
    // This happens when active decoder is not defined.
    *decoded_length = -1;
    return 0;
  }

  while (*decoded_length < rtc::dchecked_cast<int>(output_size_samples_)) {
    const int length = decoder->Decode(
            nullptr, 0, fs_hz_,
            (decoded_buffer_length_ - *decoded_length) * sizeof(int16_t),
            &decoded_buffer_[*decoded_length], speech_type);
    if (length > 0) {
      *decoded_length += length;
    } else {
      // Error.
      RTC_LOG(LS_WARNING) << "Failed to decode CNG";
      *decoded_length = -1;
      break;
    }
    if (*decoded_length > static_cast<int>(decoded_buffer_length_)) {
      // Guard against overflow.
      RTC_LOG(LS_WARNING) << "Decoded too much CNG.";
      return kDecodedTooMuch;
    }
  }
  return 0;
}

int NetEqImpl::DecodeLoop(PacketList* packet_list, const Operations& operation,
                          AudioDecoder* decoder, int* decoded_length,
                          AudioDecoder::SpeechType* speech_type) {
  RTC_DCHECK(last_decoded_timestamps_.empty());

  // Do decoding.
  while (
      !packet_list->empty() &&
      !decoder_database_->IsComfortNoise(packet_list->front().payload_type)) {
    assert(decoder);  // At this point, we must have a decoder object.
    // The number of channels in the |sync_buffer_| should be the same as the
    // number decoder channels.
    assert(sync_buffer_->Channels() == decoder->Channels());
    assert(decoded_buffer_length_ >= kMaxFrameSize * decoder->Channels());
    assert(operation == kNormal || operation == kAccelerate ||
           operation == kFastAccelerate || operation == kMerge ||
           operation == kPreemptiveExpand);

    auto opt_result = packet_list->front().frame->Decode(
        rtc::ArrayView<int16_t>(&decoded_buffer_[*decoded_length],
                                decoded_buffer_length_ - *decoded_length));
    last_decoded_timestamps_.push_back(packet_list->front().timestamp);
    packet_list->pop_front();
    if (opt_result) {
      const auto& result = *opt_result;
      *speech_type = result.speech_type;
      if (result.num_decoded_samples > 0) {
        *decoded_length += rtc::dchecked_cast<int>(result.num_decoded_samples);
        // Update |decoder_frame_length_| with number of samples per channel.
        decoder_frame_length_ =
            result.num_decoded_samples / decoder->Channels();
      }
    } else {
      // Error.
      // TODO(ossu): What to put here?
      RTC_LOG(LS_WARNING) << "Decode error";
      *decoded_length = -1;
      packet_list->clear();
      break;
    }
    if (*decoded_length > rtc::dchecked_cast<int>(decoded_buffer_length_)) {
      // Guard against overflow.
      RTC_LOG(LS_WARNING) << "Decoded too much.";
      packet_list->clear();
      return kDecodedTooMuch;
    }
  }  // End of decode loop.

  // If the list is not empty at this point, either a decoding error terminated
  // the while-loop, or list must hold exactly one CNG packet.
  assert(
      packet_list->empty() || *decoded_length < 0 ||
      (packet_list->size() == 1 &&
       decoder_database_->IsComfortNoise(packet_list->front().payload_type)));
  return 0;
}

void NetEqImpl::DoNormal(const int16_t* decoded_buffer, size_t decoded_length,
                         AudioDecoder::SpeechType speech_type, bool play_dtmf) {
  assert(normal_.get());
  assert(mute_factor_array_.get());
  normal_->Process(decoded_buffer, decoded_length, last_mode_,
                   mute_factor_array_.get(), algorithm_buffer_.get());
  if (decoded_length != 0) {
    last_mode_ = kModeNormal;
  }

  // If last packet was decoded as an inband CNG, set mode to CNG instead.
  if ((speech_type == AudioDecoder::kComfortNoise)
      || ((last_mode_ == kModeCodecInternalCng)
          && (decoded_length == 0))) {
    // TODO(hlundin): Remove second part of || statement above.
    last_mode_ = kModeCodecInternalCng;
  }

  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
}

void NetEqImpl::DoMerge(int16_t* decoded_buffer, size_t decoded_length,
                        AudioDecoder::SpeechType speech_type, bool play_dtmf) {
  assert(mute_factor_array_.get());
  assert(merge_.get());
  size_t new_length = merge_->Process(decoded_buffer, decoded_length,
                                      mute_factor_array_.get(),
                                      algorithm_buffer_.get());
  // Correction can be negative.
  int expand_length_correction =
      rtc::dchecked_cast<int>(new_length) -
      rtc::dchecked_cast<int>(decoded_length / algorithm_buffer_->Channels());

  // Update in-call and post-call statistics.
  if (expand_->MuteFactor(0) == 0) {
    // Expand generates only noise.
    stats_.ExpandedNoiseSamplesCorrection(expand_length_correction);
  } else {
    // Expansion generates more than only noise.
    stats_.ExpandedVoiceSamplesCorrection(expand_length_correction);
  }

  last_mode_ = kModeMerge;
  // If last packet was decoded as an inband CNG, set mode to CNG instead.
  if (speech_type == AudioDecoder::kComfortNoise) {
    last_mode_ = kModeCodecInternalCng;
  }
  expand_->Reset();
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
}

int NetEqImpl::DoExpand(bool play_dtmf) {
  while ((sync_buffer_->FutureLength() - expand_->overlap_length()) <
      output_size_samples_) {
    algorithm_buffer_->Clear();
    int return_value = expand_->Process(algorithm_buffer_.get());
    size_t length = algorithm_buffer_->Size();
    bool is_new_concealment_event = (last_mode_ != kModeExpand);

    // Update in-call and post-call statistics.
    if (expand_->MuteFactor(0) == 0) {
      // Expand operation generates only noise.
      stats_.ExpandedNoiseSamples(length, is_new_concealment_event);
    } else {
      // Expand operation generates more than only noise.
      stats_.ExpandedVoiceSamples(length, is_new_concealment_event);
    }

    last_mode_ = kModeExpand;

    if (return_value < 0) {
      return return_value;
    }

    sync_buffer_->PushBack(*algorithm_buffer_);
    algorithm_buffer_->Clear();
  }
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }

  if (!generated_noise_stopwatch_) {
    // Start a new stopwatch since we may be covering for a lost CNG packet.
    generated_noise_stopwatch_ = tick_timer_->GetNewStopwatch();
  }

  return 0;
}

int NetEqImpl::DoAccelerate(int16_t* decoded_buffer,
                            size_t decoded_length,
                            AudioDecoder::SpeechType speech_type,
                            bool play_dtmf,
                            bool fast_accelerate) {
  const size_t required_samples =
      static_cast<size_t>(240 * fs_mult_);  // Must have 30 ms.
  size_t borrowed_samples_per_channel = 0;
  size_t num_channels = algorithm_buffer_->Channels();
  size_t decoded_length_per_channel = decoded_length / num_channels;
  if (decoded_length_per_channel < required_samples) {
    // Must move data from the |sync_buffer_| in order to get 30 ms.
    borrowed_samples_per_channel = static_cast<int>(required_samples -
        decoded_length_per_channel);
    memmove(&decoded_buffer[borrowed_samples_per_channel * num_channels],
            decoded_buffer,
            sizeof(int16_t) * decoded_length);
    sync_buffer_->ReadInterleavedFromEnd(borrowed_samples_per_channel,
                                         decoded_buffer);
    decoded_length = required_samples * num_channels;
  }

  size_t samples_removed;
  Accelerate::ReturnCodes return_code =
      accelerate_->Process(decoded_buffer, decoded_length, fast_accelerate,
                           algorithm_buffer_.get(), &samples_removed);
  stats_.AcceleratedSamples(samples_removed);
  switch (return_code) {
    case Accelerate::kSuccess:
      last_mode_ = kModeAccelerateSuccess;
      break;
    case Accelerate::kSuccessLowEnergy:
      last_mode_ = kModeAccelerateLowEnergy;
      break;
    case Accelerate::kNoStretch:
      last_mode_ = kModeAccelerateFail;
      break;
    case Accelerate::kError:
      // TODO(hlundin): Map to kModeError instead?
      last_mode_ = kModeAccelerateFail;
      return kAccelerateError;
  }

  if (borrowed_samples_per_channel > 0) {
    // Copy borrowed samples back to the |sync_buffer_|.
    size_t length = algorithm_buffer_->Size();
    if (length < borrowed_samples_per_channel) {
      // This destroys the beginning of the buffer, but will not cause any
      // problems.
      sync_buffer_->ReplaceAtIndex(*algorithm_buffer_,
                                   sync_buffer_->Size() -
                                   borrowed_samples_per_channel);
      sync_buffer_->PushFrontZeros(borrowed_samples_per_channel - length);
      algorithm_buffer_->PopFront(length);
      assert(algorithm_buffer_->Empty());
    } else {
      sync_buffer_->ReplaceAtIndex(*algorithm_buffer_,
                                   borrowed_samples_per_channel,
                                   sync_buffer_->Size() -
                                   borrowed_samples_per_channel);
      algorithm_buffer_->PopFront(borrowed_samples_per_channel);
    }
  }

  // If last packet was decoded as an inband CNG, set mode to CNG instead.
  if (speech_type == AudioDecoder::kComfortNoise) {
    last_mode_ = kModeCodecInternalCng;
  }
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
  expand_->Reset();
  return 0;
}

int NetEqImpl::DoPreemptiveExpand(int16_t* decoded_buffer,
                                  size_t decoded_length,
                                  AudioDecoder::SpeechType speech_type,
                                  bool play_dtmf) {
  const size_t required_samples =
      static_cast<size_t>(240 * fs_mult_);  // Must have 30 ms.
  size_t num_channels = algorithm_buffer_->Channels();
  size_t borrowed_samples_per_channel = 0;
  size_t old_borrowed_samples_per_channel = 0;
  size_t decoded_length_per_channel = decoded_length / num_channels;
  if (decoded_length_per_channel < required_samples) {
    // Must move data from the |sync_buffer_| in order to get 30 ms.
    borrowed_samples_per_channel =
        required_samples - decoded_length_per_channel;
    // Calculate how many of these were already played out.
    old_borrowed_samples_per_channel =
        (borrowed_samples_per_channel > sync_buffer_->FutureLength()) ?
        (borrowed_samples_per_channel - sync_buffer_->FutureLength()) : 0;
    memmove(&decoded_buffer[borrowed_samples_per_channel * num_channels],
            decoded_buffer,
            sizeof(int16_t) * decoded_length);
    sync_buffer_->ReadInterleavedFromEnd(borrowed_samples_per_channel,
                                         decoded_buffer);
    decoded_length = required_samples * num_channels;
  }

  size_t samples_added;
  PreemptiveExpand::ReturnCodes return_code = preemptive_expand_->Process(
      decoded_buffer, decoded_length,
      old_borrowed_samples_per_channel,
      algorithm_buffer_.get(), &samples_added);
  stats_.PreemptiveExpandedSamples(samples_added);
  switch (return_code) {
    case PreemptiveExpand::kSuccess:
      last_mode_ = kModePreemptiveExpandSuccess;
      break;
    case PreemptiveExpand::kSuccessLowEnergy:
      last_mode_ = kModePreemptiveExpandLowEnergy;
      break;
    case PreemptiveExpand::kNoStretch:
      last_mode_ = kModePreemptiveExpandFail;
      break;
    case PreemptiveExpand::kError:
      // TODO(hlundin): Map to kModeError instead?
      last_mode_ = kModePreemptiveExpandFail;
      return kPreemptiveExpandError;
  }

  if (borrowed_samples_per_channel > 0) {
    // Copy borrowed samples back to the |sync_buffer_|.
    sync_buffer_->ReplaceAtIndex(
        *algorithm_buffer_, borrowed_samples_per_channel,
        sync_buffer_->Size() - borrowed_samples_per_channel);
    algorithm_buffer_->PopFront(borrowed_samples_per_channel);
  }

  // If last packet was decoded as an inband CNG, set mode to CNG instead.
  if (speech_type == AudioDecoder::kComfortNoise) {
    last_mode_ = kModeCodecInternalCng;
  }
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
  expand_->Reset();
  return 0;
}

int NetEqImpl::DoRfc3389Cng(PacketList* packet_list, bool play_dtmf) {
  if (!packet_list->empty()) {
    // Must have exactly one SID frame at this point.
    assert(packet_list->size() == 1);
    const Packet& packet = packet_list->front();
    if (!decoder_database_->IsComfortNoise(packet.payload_type)) {
      RTC_LOG(LS_ERROR) << "Trying to decode non-CNG payload as CNG.";
      return kOtherError;
    }
    if (comfort_noise_->UpdateParameters(packet) ==
        ComfortNoise::kInternalError) {
      algorithm_buffer_->Zeros(output_size_samples_);
      return -comfort_noise_->internal_error_code();
    }
  }
  int cn_return = comfort_noise_->Generate(output_size_samples_,
                                           algorithm_buffer_.get());
  expand_->Reset();
  last_mode_ = kModeRfc3389Cng;
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
  if (cn_return == ComfortNoise::kInternalError) {
    RTC_LOG(LS_WARNING) << "Comfort noise generator returned error code: "
                        << comfort_noise_->internal_error_code();
    return kComfortNoiseErrorCode;
  } else if (cn_return == ComfortNoise::kUnknownPayloadType) {
    return kUnknownRtpPayloadType;
  }
  return 0;
}

void NetEqImpl::DoCodecInternalCng(const int16_t* decoded_buffer,
                                   size_t decoded_length) {
  RTC_DCHECK(normal_.get());
  RTC_DCHECK(mute_factor_array_.get());
  normal_->Process(decoded_buffer, decoded_length, last_mode_,
                   mute_factor_array_.get(), algorithm_buffer_.get());
  last_mode_ = kModeCodecInternalCng;
  expand_->Reset();
}

int NetEqImpl::DoDtmf(const DtmfEvent& dtmf_event, bool* play_dtmf) {
  // This block of the code and the block further down, handling |dtmf_switch|
  // are commented out. Otherwise playing out-of-band DTMF would fail in VoE
  // test, DtmfTest.ManualSuccessfullySendsOutOfBandTelephoneEvents. This is
  // equivalent to |dtmf_switch| always be false.
  //
  // See http://webrtc-codereview.appspot.com/1195004/ for discussion
  // On this issue. This change might cause some glitches at the point of
  // switch from audio to DTMF. Issue 1545 is filed to track this.
  //
  //  bool dtmf_switch = false;
  //  if ((last_mode_ != kModeDtmf) && dtmf_tone_generator_->initialized()) {
  //    // Special case; see below.
  //    // We must catch this before calling Generate, since |initialized| is
  //    // modified in that call.
  //    dtmf_switch = true;
  //  }

  int dtmf_return_value = 0;
  if (!dtmf_tone_generator_->initialized()) {
    // Initialize if not already done.
    dtmf_return_value = dtmf_tone_generator_->Init(fs_hz_, dtmf_event.event_no,
                                                   dtmf_event.volume);
  }

  if (dtmf_return_value == 0) {
    // Generate DTMF signal.
    dtmf_return_value = dtmf_tone_generator_->Generate(output_size_samples_,
                                                       algorithm_buffer_.get());
  }

  if (dtmf_return_value < 0) {
    algorithm_buffer_->Zeros(output_size_samples_);
    return dtmf_return_value;
  }

  //  if (dtmf_switch) {
  //    // This is the special case where the previous operation was DTMF
  //    // overdub, but the current instruction is "regular" DTMF. We must make
  //    // sure that the DTMF does not have any discontinuities. The first DTMF
  //    // sample that we generate now must be played out immediately, therefore
  //    // it must be copied to the speech buffer.
  //    // TODO(hlundin): This code seems incorrect. (Legacy.) Write test and
  //    // verify correct operation.
  //    assert(false);
  //    // Must generate enough data to replace all of the |sync_buffer_|
  //    // "future".
  //    int required_length = sync_buffer_->FutureLength();
  //    assert(dtmf_tone_generator_->initialized());
  //    dtmf_return_value = dtmf_tone_generator_->Generate(required_length,
  //                                                       algorithm_buffer_);
  //    assert((size_t) required_length == algorithm_buffer_->Size());
  //    if (dtmf_return_value < 0) {
  //      algorithm_buffer_->Zeros(output_size_samples_);
  //      return dtmf_return_value;
  //    }
  //
  //    // Overwrite the "future" part of the speech buffer with the new DTMF
  //    // data.
  //    // TODO(hlundin): It seems that this overwriting has gone lost.
  //    // Not adapted for multi-channel yet.
  //    assert(algorithm_buffer_->Channels() == 1);
  //    if (algorithm_buffer_->Channels() != 1) {
  //      RTC_LOG(LS_WARNING) << "DTMF not supported for more than one channel";
  //      return kStereoNotSupported;
  //    }
  //    // Shuffle the remaining data to the beginning of algorithm buffer.
  //    algorithm_buffer_->PopFront(sync_buffer_->FutureLength());
  //  }

  sync_buffer_->IncreaseEndTimestamp(
      static_cast<uint32_t>(output_size_samples_));
  expand_->Reset();
  last_mode_ = kModeDtmf;

  // Set to false because the DTMF is already in the algorithm buffer.
  *play_dtmf = false;
  return 0;
}

void NetEqImpl::DoAlternativePlc(bool increase_timestamp) {
  AudioDecoder* decoder = decoder_database_->GetActiveDecoder();
  size_t length;
  if (decoder && decoder->HasDecodePlc()) {
    // Use the decoder's packet-loss concealment.
    // TODO(hlundin): Will probably need a longer buffer for multi-channel.
    int16_t decoded_buffer[kMaxFrameSize];
    length = decoder->DecodePlc(1, decoded_buffer);
    if (length > 0)
      algorithm_buffer_->PushBackInterleaved(decoded_buffer, length);
  } else {
    // Do simple zero-stuffing.
    length = output_size_samples_;
    algorithm_buffer_->Zeros(length);
    // By not advancing the timestamp, NetEq inserts samples.
    stats_.AddZeros(length);
  }
  if (increase_timestamp) {
    sync_buffer_->IncreaseEndTimestamp(static_cast<uint32_t>(length));
  }
  expand_->Reset();
}

int NetEqImpl::DtmfOverdub(const DtmfEvent& dtmf_event, size_t num_channels,
                           int16_t* output) const {
  size_t out_index = 0;
  size_t overdub_length = output_size_samples_;  // Default value.

  if (sync_buffer_->dtmf_index() > sync_buffer_->next_index()) {
    // Special operation for transition from "DTMF only" to "DTMF overdub".
    out_index = std::min(
        sync_buffer_->dtmf_index() - sync_buffer_->next_index(),
        output_size_samples_);
    overdub_length = output_size_samples_ - out_index;
  }

  AudioMultiVector dtmf_output(num_channels);
  int dtmf_return_value = 0;
  if (!dtmf_tone_generator_->initialized()) {
    dtmf_return_value = dtmf_tone_generator_->Init(fs_hz_, dtmf_event.event_no,
                                                   dtmf_event.volume);
  }
  if (dtmf_return_value == 0) {
    dtmf_return_value = dtmf_tone_generator_->Generate(overdub_length,
                                                       &dtmf_output);
    assert(overdub_length == dtmf_output.Size());
  }
  dtmf_output.ReadInterleaved(overdub_length, &output[out_index]);
  return dtmf_return_value < 0 ? dtmf_return_value : 0;
}

int NetEqImpl::ExtractPackets(size_t required_samples,
                              PacketList* packet_list) {
  bool first_packet = true;
  uint8_t prev_payload_type = 0;
  uint32_t prev_timestamp = 0;
  uint16_t prev_sequence_number = 0;
  bool next_packet_available = false;

  const Packet* next_packet = packet_buffer_->PeekNextPacket();
  RTC_DCHECK(next_packet);
  if (!next_packet) {
    RTC_LOG(LS_ERROR) << "Packet buffer unexpectedly empty.";
    return -1;
  }
  uint32_t first_timestamp = next_packet->timestamp;
  size_t extracted_samples = 0;

  // Packet extraction loop.
  do {
    timestamp_ = next_packet->timestamp;
    rtc::Optional<Packet> packet = packet_buffer_->GetNextPacket();
    // |next_packet| may be invalid after the |packet_buffer_| operation.
    next_packet = nullptr;
    if (!packet) {
      RTC_LOG(LS_ERROR) << "Should always be able to extract a packet here";
      assert(false);  // Should always be able to extract a packet here.
      return -1;
    }
    const uint64_t waiting_time_ms = packet->waiting_time->ElapsedMs();
    stats_.StoreWaitingTime(waiting_time_ms);
    RTC_DCHECK(!packet->empty());

    if (first_packet) {
      first_packet = false;
      if (nack_enabled_) {
        RTC_DCHECK(nack_);
        // TODO(henrik.lundin): Should we update this for all decoded packets?
        nack_->UpdateLastDecodedPacket(packet->sequence_number,
                                       packet->timestamp);
      }
      prev_sequence_number = packet->sequence_number;
      prev_timestamp = packet->timestamp;
      prev_payload_type = packet->payload_type;
    }

    const bool has_cng_packet =
        decoder_database_->IsComfortNoise(packet->payload_type);
    // Store number of extracted samples.
    size_t packet_duration = 0;
    if (packet->frame) {
      packet_duration = packet->frame->Duration();
      // TODO(ossu): Is this the correct way to track Opus FEC packets?
      if (packet->priority.codec_level > 0) {
        stats_.SecondaryDecodedSamples(
            rtc::dchecked_cast<int>(packet_duration));
      }
    } else if (!has_cng_packet) {
      RTC_LOG(LS_WARNING) << "Unknown payload type "
                          << static_cast<int>(packet->payload_type);
      RTC_NOTREACHED();
    }

    if (packet_duration == 0) {
      // Decoder did not return a packet duration. Assume that the packet
      // contains the same number of samples as the previous one.
      packet_duration = decoder_frame_length_;
    }
    extracted_samples = packet->timestamp - first_timestamp + packet_duration;

    stats_.JitterBufferDelay(extracted_samples, waiting_time_ms);

    packet_list->push_back(std::move(*packet));  // Store packet in list.
    packet = rtc::nullopt;  // Ensure it's never used after the move.

    // Check what packet is available next.
    next_packet = packet_buffer_->PeekNextPacket();
    next_packet_available = false;
    if (next_packet && prev_payload_type == next_packet->payload_type &&
        !has_cng_packet) {
      int16_t seq_no_diff = next_packet->sequence_number - prev_sequence_number;
      size_t ts_diff = next_packet->timestamp - prev_timestamp;
      if (seq_no_diff == 1 ||
          (seq_no_diff == 0 && ts_diff == decoder_frame_length_)) {
        // The next sequence number is available, or the next part of a packet
        // that was split into pieces upon insertion.
        next_packet_available = true;
      }
      prev_sequence_number = next_packet->sequence_number;
    }
  } while (extracted_samples < required_samples && next_packet_available);

  if (extracted_samples > 0) {
    // Delete old packets only when we are going to decode something. Otherwise,
    // we could end up in the situation where we never decode anything, since
    // all incoming packets are considered too old but the buffer will also
    // never be flooded and flushed.
    packet_buffer_->DiscardAllOldPackets(timestamp_, &stats_);
  }

  return rtc::dchecked_cast<int>(extracted_samples);
}

void NetEqImpl::UpdatePlcComponents(int fs_hz, size_t channels) {
  // Delete objects and create new ones.
  expand_.reset(expand_factory_->Create(background_noise_.get(),
                                        sync_buffer_.get(), &random_vector_,
                                        &stats_, fs_hz, channels));
  merge_.reset(new Merge(fs_hz, channels, expand_.get(), sync_buffer_.get()));
}

void NetEqImpl::SetSampleRateAndChannels(int fs_hz, size_t channels) {
  RTC_LOG(LS_VERBOSE) << "SetSampleRateAndChannels " << fs_hz << " "
                      << channels;
  // TODO(hlundin): Change to an enumerator and skip assert.
  assert(fs_hz == 8000 || fs_hz == 16000 || fs_hz ==  32000 || fs_hz == 48000);
  assert(channels > 0);

  fs_hz_ = fs_hz;
  fs_mult_ = fs_hz / 8000;
  output_size_samples_ = static_cast<size_t>(kOutputSizeMs * 8 * fs_mult_);
  decoder_frame_length_ = 3 * output_size_samples_;  // Initialize to 30ms.

  last_mode_ = kModeNormal;

  // Create a new array of mute factors and set all to 1.
  mute_factor_array_.reset(new int16_t[channels]);
  for (size_t i = 0; i < channels; ++i) {
    mute_factor_array_[i] = 16384;  // 1.0 in Q14.
  }

  ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();
  if (cng_decoder)
    cng_decoder->Reset();

  // Reinit post-decode VAD with new sample rate.
  assert(vad_.get());  // Cannot be NULL here.
  vad_->Init();

  // Delete algorithm buffer and create a new one.
  algorithm_buffer_.reset(new AudioMultiVector(channels));

  // Delete sync buffer and create a new one.
  sync_buffer_.reset(new SyncBuffer(channels, kSyncBufferSize * fs_mult_));

  // Delete BackgroundNoise object and create a new one.
  background_noise_.reset(new BackgroundNoise(channels));
  background_noise_->set_mode(background_noise_mode_);

  // Reset random vector.
  random_vector_.Reset();

  UpdatePlcComponents(fs_hz, channels);

  // Move index so that we create a small set of future samples (all 0).
  sync_buffer_->set_next_index(sync_buffer_->next_index() -
      expand_->overlap_length());

  normal_.reset(new Normal(fs_hz, decoder_database_.get(), *background_noise_,
                           expand_.get()));
  accelerate_.reset(
      accelerate_factory_->Create(fs_hz, channels, *background_noise_));
  preemptive_expand_.reset(preemptive_expand_factory_->Create(
      fs_hz, channels, *background_noise_, expand_->overlap_length()));

  // Delete ComfortNoise object and create a new one.
  comfort_noise_.reset(new ComfortNoise(fs_hz, decoder_database_.get(),
                                        sync_buffer_.get()));

  // Verify that |decoded_buffer_| is long enough.
  if (decoded_buffer_length_ < kMaxFrameSize * channels) {
    // Reallocate to larger size.
    decoded_buffer_length_ = kMaxFrameSize * channels;
    decoded_buffer_.reset(new int16_t[decoded_buffer_length_]);
  }

  // Create DecisionLogic if it is not created yet, then communicate new sample
  // rate and output size to DecisionLogic object.
  if (!decision_logic_.get()) {
    CreateDecisionLogic();
  }
  decision_logic_->SetSampleRate(fs_hz_, output_size_samples_);
}

NetEqImpl::OutputType NetEqImpl::LastOutputType() {
  assert(vad_.get());
  assert(expand_.get());
  if (last_mode_ == kModeCodecInternalCng || last_mode_ == kModeRfc3389Cng) {
    return OutputType::kCNG;
  } else if (last_mode_ == kModeExpand && expand_->MuteFactor(0) == 0) {
    // Expand mode has faded down to background noise only (very long expand).
    return OutputType::kPLCCNG;
  } else if (last_mode_ == kModeExpand) {
    return OutputType::kPLC;
  } else if (vad_->running() && !vad_->active_speech()) {
    return OutputType::kVadPassive;
  } else {
    return OutputType::kNormalSpeech;
  }
}

void NetEqImpl::CreateDecisionLogic() {
  decision_logic_.reset(DecisionLogic::Create(
      fs_hz_, output_size_samples_, playout_mode_, decoder_database_.get(),
      *packet_buffer_.get(), delay_manager_.get(), buffer_level_filter_.get(),
      tick_timer_.get()));
}
}  // namespace webrtc
