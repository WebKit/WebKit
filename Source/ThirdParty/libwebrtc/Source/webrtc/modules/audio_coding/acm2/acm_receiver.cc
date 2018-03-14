/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_receiver.h"

#include <stdlib.h>  // malloc

#include <algorithm>  // sort
#include <vector>

#include "api/audio_codecs/audio_decoder.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/acm2/acm_resampler.h"
#include "modules/audio_coding/acm2/call_statistics.h"
#include "modules/audio_coding/acm2/rent_a_codec.h"
#include "modules/audio_coding/neteq/include/neteq.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace acm2 {

AcmReceiver::AcmReceiver(const AudioCodingModule::Config& config)
    : last_audio_buffer_(new int16_t[AudioFrame::kMaxDataSizeSamples]),
      neteq_(NetEq::Create(config.neteq_config, config.decoder_factory)),
      clock_(config.clock),
      resampled_last_output_frame_(true) {
  RTC_DCHECK(clock_);
  memset(last_audio_buffer_.get(), 0, AudioFrame::kMaxDataSizeSamples);
}

AcmReceiver::~AcmReceiver() = default;

int AcmReceiver::SetMinimumDelay(int delay_ms) {
  if (neteq_->SetMinimumDelay(delay_ms))
    return 0;
  RTC_LOG(LERROR) << "AcmReceiver::SetExtraDelay " << delay_ms;
  return -1;
}

int AcmReceiver::SetMaximumDelay(int delay_ms) {
  if (neteq_->SetMaximumDelay(delay_ms))
    return 0;
  RTC_LOG(LERROR) << "AcmReceiver::SetExtraDelay " << delay_ms;
  return -1;
}

int AcmReceiver::LeastRequiredDelayMs() const {
  return neteq_->LeastRequiredDelayMs();
}

rtc::Optional<int> AcmReceiver::last_packet_sample_rate_hz() const {
  rtc::CritScope lock(&crit_sect_);
  return last_packet_sample_rate_hz_;
}

int AcmReceiver::last_output_sample_rate_hz() const {
  return neteq_->last_output_sample_rate_hz();
}

int AcmReceiver::InsertPacket(const WebRtcRTPHeader& rtp_header,
                              rtc::ArrayView<const uint8_t> incoming_payload) {
  uint32_t receive_timestamp = 0;
  const RTPHeader* header = &rtp_header.header;  // Just a shorthand.

  if (incoming_payload.empty()) {
    neteq_->InsertEmptyPacket(rtp_header.header);
    return 0;
  }

  {
    rtc::CritScope lock(&crit_sect_);

    const rtc::Optional<CodecInst> ci =
        RtpHeaderToDecoder(*header, incoming_payload[0]);
    if (!ci) {
      RTC_LOG_F(LS_ERROR) << "Payload-type "
                          << static_cast<int>(header->payloadType)
                          << " is not registered.";
      return -1;
    }
    receive_timestamp = NowInTimestamp(ci->plfreq);

    if (STR_CASE_CMP(ci->plname, "cn") == 0) {
      if (last_audio_decoder_ && last_audio_decoder_->channels > 1) {
        // This is a CNG and the audio codec is not mono, so skip pushing in
        // packets into NetEq.
        return 0;
      }
    } else {
      last_audio_decoder_ = ci;
      last_audio_format_ = neteq_->GetDecoderFormat(ci->pltype);
      RTC_DCHECK(last_audio_format_);
      last_packet_sample_rate_hz_ = ci->plfreq;
    }
  }  // |crit_sect_| is released.

  if (neteq_->InsertPacket(rtp_header.header, incoming_payload,
                           receive_timestamp) < 0) {
    RTC_LOG(LERROR) << "AcmReceiver::InsertPacket "
                    << static_cast<int>(header->payloadType)
                    << " Failed to insert packet";
    return -1;
  }
  return 0;
}

int AcmReceiver::GetAudio(int desired_freq_hz,
                          AudioFrame* audio_frame,
                          bool* muted) {
  RTC_DCHECK(muted);
  // Accessing members, take the lock.
  rtc::CritScope lock(&crit_sect_);

  if (neteq_->GetAudio(audio_frame, muted) != NetEq::kOK) {
    RTC_LOG(LERROR) << "AcmReceiver::GetAudio - NetEq Failed.";
    return -1;
  }

  const int current_sample_rate_hz = neteq_->last_output_sample_rate_hz();

  // Update if resampling is required.
  const bool need_resampling =
      (desired_freq_hz != -1) && (current_sample_rate_hz != desired_freq_hz);

  if (need_resampling && !resampled_last_output_frame_) {
    // Prime the resampler with the last frame.
    int16_t temp_output[AudioFrame::kMaxDataSizeSamples];
    int samples_per_channel_int = resampler_.Resample10Msec(
        last_audio_buffer_.get(), current_sample_rate_hz, desired_freq_hz,
        audio_frame->num_channels_, AudioFrame::kMaxDataSizeSamples,
        temp_output);
    if (samples_per_channel_int < 0) {
      RTC_LOG(LERROR) << "AcmReceiver::GetAudio - "
                         "Resampling last_audio_buffer_ failed.";
      return -1;
    }
  }

  // TODO(henrik.lundin) Glitches in the output may appear if the output rate
  // from NetEq changes. See WebRTC issue 3923.
  if (need_resampling) {
    // TODO(yujo): handle this more efficiently for muted frames.
    int samples_per_channel_int = resampler_.Resample10Msec(
        audio_frame->data(), current_sample_rate_hz, desired_freq_hz,
        audio_frame->num_channels_, AudioFrame::kMaxDataSizeSamples,
        audio_frame->mutable_data());
    if (samples_per_channel_int < 0) {
      RTC_LOG(LERROR)
          << "AcmReceiver::GetAudio - Resampling audio_buffer_ failed.";
      return -1;
    }
    audio_frame->samples_per_channel_ =
        static_cast<size_t>(samples_per_channel_int);
    audio_frame->sample_rate_hz_ = desired_freq_hz;
    RTC_DCHECK_EQ(
        audio_frame->sample_rate_hz_,
        rtc::dchecked_cast<int>(audio_frame->samples_per_channel_ * 100));
    resampled_last_output_frame_ = true;
  } else {
    resampled_last_output_frame_ = false;
    // We might end up here ONLY if codec is changed.
  }

  // Store current audio in |last_audio_buffer_| for next time.
  memcpy(last_audio_buffer_.get(), audio_frame->data(),
         sizeof(int16_t) * audio_frame->samples_per_channel_ *
             audio_frame->num_channels_);

  call_stats_.DecodedByNetEq(audio_frame->speech_type_, *muted);
  return 0;
}

void AcmReceiver::SetCodecs(const std::map<int, SdpAudioFormat>& codecs) {
  neteq_->SetCodecs(codecs);
}

int32_t AcmReceiver::AddCodec(int acm_codec_id,
                              uint8_t payload_type,
                              size_t channels,
                              int /*sample_rate_hz*/,
                              AudioDecoder* audio_decoder,
                              const std::string& name) {
  // TODO(kwiberg): This function has been ignoring the |sample_rate_hz|
  // argument for a long time. Arguably, it should simply be removed.

  const auto neteq_decoder = [acm_codec_id, channels]() -> NetEqDecoder {
    if (acm_codec_id == -1)
      return NetEqDecoder::kDecoderArbitrary;  // External decoder.
    const rtc::Optional<RentACodec::CodecId> cid =
        RentACodec::CodecIdFromIndex(acm_codec_id);
    RTC_DCHECK(cid) << "Invalid codec index: " << acm_codec_id;
    const rtc::Optional<NetEqDecoder> ned =
        RentACodec::NetEqDecoderFromCodecId(*cid, channels);
    RTC_DCHECK(ned) << "Invalid codec ID: " << static_cast<int>(*cid);
    return *ned;
  }();
  const rtc::Optional<SdpAudioFormat> new_format =
      NetEqDecoderToSdpAudioFormat(neteq_decoder);

  rtc::CritScope lock(&crit_sect_);

  const auto old_format = neteq_->GetDecoderFormat(payload_type);
  if (old_format && new_format && *old_format == *new_format) {
    // Re-registering the same codec. Do nothing and return.
    return 0;
  }

  if (neteq_->RemovePayloadType(payload_type) != NetEq::kOK) {
    RTC_LOG(LERROR) << "Cannot remove payload "
                    << static_cast<int>(payload_type);
    return -1;
  }

  int ret_val;
  if (!audio_decoder) {
    ret_val = neteq_->RegisterPayloadType(neteq_decoder, name, payload_type);
  } else {
    ret_val = neteq_->RegisterExternalDecoder(
        audio_decoder, neteq_decoder, name, payload_type);
  }
  if (ret_val != NetEq::kOK) {
    RTC_LOG(LERROR) << "AcmReceiver::AddCodec " << acm_codec_id
                    << static_cast<int>(payload_type)
                    << " channels: " << channels;
    return -1;
  }
  return 0;
}

bool AcmReceiver::AddCodec(int rtp_payload_type,
                           const SdpAudioFormat& audio_format) {
  const auto old_format = neteq_->GetDecoderFormat(rtp_payload_type);
  if (old_format && *old_format == audio_format) {
    // Re-registering the same codec. Do nothing and return.
    return true;
  }

  if (neteq_->RemovePayloadType(rtp_payload_type) != NetEq::kOK) {
    RTC_LOG(LERROR)
        << "AcmReceiver::AddCodec: Could not remove existing decoder"
           " for payload type "
        << rtp_payload_type;
    return false;
  }

  const bool success =
      neteq_->RegisterPayloadType(rtp_payload_type, audio_format);
  if (!success) {
    RTC_LOG(LERROR) << "AcmReceiver::AddCodec failed for payload type "
                    << rtp_payload_type << ", decoder format " << audio_format;
  }
  return success;
}

void AcmReceiver::FlushBuffers() {
  neteq_->FlushBuffers();
}

void AcmReceiver::RemoveAllCodecs() {
  rtc::CritScope lock(&crit_sect_);
  neteq_->RemoveAllPayloadTypes();
  last_audio_decoder_ = rtc::nullopt;
  last_audio_format_ = rtc::nullopt;
  last_packet_sample_rate_hz_ = rtc::nullopt;
}

int AcmReceiver::RemoveCodec(uint8_t payload_type) {
  rtc::CritScope lock(&crit_sect_);
  if (neteq_->RemovePayloadType(payload_type) != NetEq::kOK) {
    RTC_LOG(LERROR) << "AcmReceiver::RemoveCodec "
                    << static_cast<int>(payload_type);
    return -1;
  }
  if (last_audio_decoder_ && payload_type == last_audio_decoder_->pltype) {
    last_audio_decoder_ = rtc::nullopt;
    last_audio_format_ = rtc::nullopt;
    last_packet_sample_rate_hz_ = rtc::nullopt;
  }
  return 0;
}

rtc::Optional<uint32_t> AcmReceiver::GetPlayoutTimestamp() {
  return neteq_->GetPlayoutTimestamp();
}

int AcmReceiver::FilteredCurrentDelayMs() const {
  return neteq_->FilteredCurrentDelayMs();
}

int AcmReceiver::TargetDelayMs() const {
  return neteq_->TargetDelayMs();
}

int AcmReceiver::LastAudioCodec(CodecInst* codec) const {
  rtc::CritScope lock(&crit_sect_);
  if (!last_audio_decoder_) {
    return -1;
  }
  *codec = *last_audio_decoder_;
  return 0;
}

rtc::Optional<SdpAudioFormat> AcmReceiver::LastAudioFormat() const {
  rtc::CritScope lock(&crit_sect_);
  return last_audio_format_;
}

void AcmReceiver::GetNetworkStatistics(NetworkStatistics* acm_stat) {
  NetEqNetworkStatistics neteq_stat;
  // NetEq function always returns zero, so we don't check the return value.
  neteq_->NetworkStatistics(&neteq_stat);

  acm_stat->currentBufferSize = neteq_stat.current_buffer_size_ms;
  acm_stat->preferredBufferSize = neteq_stat.preferred_buffer_size_ms;
  acm_stat->jitterPeaksFound = neteq_stat.jitter_peaks_found ? true : false;
  acm_stat->currentPacketLossRate = neteq_stat.packet_loss_rate;
  acm_stat->currentExpandRate = neteq_stat.expand_rate;
  acm_stat->currentSpeechExpandRate = neteq_stat.speech_expand_rate;
  acm_stat->currentPreemptiveRate = neteq_stat.preemptive_rate;
  acm_stat->currentAccelerateRate = neteq_stat.accelerate_rate;
  acm_stat->currentSecondaryDecodedRate = neteq_stat.secondary_decoded_rate;
  acm_stat->currentSecondaryDiscardedRate = neteq_stat.secondary_discarded_rate;
  acm_stat->clockDriftPPM = neteq_stat.clockdrift_ppm;
  acm_stat->addedSamples = neteq_stat.added_zero_samples;
  acm_stat->meanWaitingTimeMs = neteq_stat.mean_waiting_time_ms;
  acm_stat->medianWaitingTimeMs = neteq_stat.median_waiting_time_ms;
  acm_stat->minWaitingTimeMs = neteq_stat.min_waiting_time_ms;
  acm_stat->maxWaitingTimeMs = neteq_stat.max_waiting_time_ms;

  NetEqLifetimeStatistics neteq_lifetime_stat = neteq_->GetLifetimeStatistics();
  acm_stat->totalSamplesReceived = neteq_lifetime_stat.total_samples_received;
  acm_stat->concealedSamples = neteq_lifetime_stat.concealed_samples;
  acm_stat->concealmentEvents = neteq_lifetime_stat.concealment_events;
  acm_stat->jitterBufferDelayMs = neteq_lifetime_stat.jitter_buffer_delay_ms;
}

int AcmReceiver::DecoderByPayloadType(uint8_t payload_type,
                                      CodecInst* codec) const {
  rtc::CritScope lock(&crit_sect_);
  const rtc::Optional<CodecInst> ci = neteq_->GetDecoder(payload_type);
  if (ci) {
    *codec = *ci;
    return 0;
  } else {
    RTC_LOG(LERROR) << "AcmReceiver::DecoderByPayloadType "
                    << static_cast<int>(payload_type);
    return -1;
  }
}

int AcmReceiver::EnableNack(size_t max_nack_list_size) {
  neteq_->EnableNack(max_nack_list_size);
  return 0;
}

void AcmReceiver::DisableNack() {
  neteq_->DisableNack();
}

std::vector<uint16_t> AcmReceiver::GetNackList(
    int64_t round_trip_time_ms) const {
  return neteq_->GetNackList(round_trip_time_ms);
}

void AcmReceiver::ResetInitialDelay() {
  neteq_->SetMinimumDelay(0);
  // TODO(turajs): Should NetEq Buffer be flushed?
}

const rtc::Optional<CodecInst> AcmReceiver::RtpHeaderToDecoder(
    const RTPHeader& rtp_header,
    uint8_t first_payload_byte) const {
  const rtc::Optional<CodecInst> ci =
      neteq_->GetDecoder(rtp_header.payloadType);
  if (ci && STR_CASE_CMP(ci->plname, "red") == 0) {
    // This is a RED packet. Get the payload of the audio codec.
    return neteq_->GetDecoder(first_payload_byte & 0x7f);
  } else {
    return ci;
  }
}

uint32_t AcmReceiver::NowInTimestamp(int decoder_sampling_rate) const {
  // Down-cast the time to (32-6)-bit since we only care about
  // the least significant bits. (32-6) bits cover 2^(32-6) = 67108864 ms.
  // We masked 6 most significant bits of 32-bit so there is no overflow in
  // the conversion from milliseconds to timestamp.
  const uint32_t now_in_ms = static_cast<uint32_t>(
      clock_->TimeInMilliseconds() & 0x03ffffff);
  return static_cast<uint32_t>(
      (decoder_sampling_rate / 1000) * now_in_ms);
}

void AcmReceiver::GetDecodingCallStatistics(
    AudioDecodingCallStats* stats) const {
  rtc::CritScope lock(&crit_sect_);
  *stats = call_stats_.GetDecodingStatistics();
}

}  // namespace acm2

}  // namespace webrtc
