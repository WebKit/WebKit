/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/decision_logic.h"

#include <assert.h>
#include <stdio.h>
#include <string>

#include "modules/audio_coding/neteq/buffer_level_filter.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/delay_manager.h"
#include "modules/audio_coding/neteq/expand.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/sync_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/field_trial.h"

namespace {
constexpr char kPostponeDecodingFieldTrial[] =
    "WebRTC-Audio-NetEqPostponeDecodingAfterExpand";

int GetPostponeDecodingLevel() {
  const bool enabled =
      webrtc::field_trial::IsEnabled(kPostponeDecodingFieldTrial);
  if (!enabled)
    return 0;

  constexpr int kDefaultPostponeDecodingLevel = 50;
  const std::string field_trial_string =
      webrtc::field_trial::FindFullName(kPostponeDecodingFieldTrial);
  int value = -1;
  if (sscanf(field_trial_string.c_str(), "Enabled-%d", &value) == 1) {
    if (value >= 0 && value <= 100) {
      return value;
    } else {
      RTC_LOG(LS_WARNING)
          << "Wrong value (" << value
          << ") for postpone decoding after expand, using default ("
          << kDefaultPostponeDecodingLevel << ")";
    }
  }
  return kDefaultPostponeDecodingLevel;
}

}  // namespace

namespace webrtc {

DecisionLogic* DecisionLogic::Create(int fs_hz,
                                     size_t output_size_samples,
                                     bool disallow_time_stretching,
                                     DecoderDatabase* decoder_database,
                                     const PacketBuffer& packet_buffer,
                                     DelayManager* delay_manager,
                                     BufferLevelFilter* buffer_level_filter,
                                     const TickTimer* tick_timer) {
  return new DecisionLogic(fs_hz, output_size_samples, disallow_time_stretching,
                           decoder_database, packet_buffer, delay_manager,
                           buffer_level_filter, tick_timer);
}

DecisionLogic::DecisionLogic(int fs_hz,
                             size_t output_size_samples,
                             bool disallow_time_stretching,
                             DecoderDatabase* decoder_database,
                             const PacketBuffer& packet_buffer,
                             DelayManager* delay_manager,
                             BufferLevelFilter* buffer_level_filter,
                             const TickTimer* tick_timer)
    : decoder_database_(decoder_database),
      packet_buffer_(packet_buffer),
      delay_manager_(delay_manager),
      buffer_level_filter_(buffer_level_filter),
      tick_timer_(tick_timer),
      cng_state_(kCngOff),
      packet_length_samples_(0),
      sample_memory_(0),
      prev_time_scale_(false),
      disallow_time_stretching_(disallow_time_stretching),
      timescale_countdown_(
          tick_timer_->GetNewCountdown(kMinTimescaleInterval + 1)),
      num_consecutive_expands_(0),
      postpone_decoding_level_(GetPostponeDecodingLevel()) {
  delay_manager_->set_streaming_mode(false);
  SetSampleRate(fs_hz, output_size_samples);
}

DecisionLogic::~DecisionLogic() = default;

void DecisionLogic::Reset() {
  cng_state_ = kCngOff;
  noise_fast_forward_ = 0;
  packet_length_samples_ = 0;
  sample_memory_ = 0;
  prev_time_scale_ = false;
  timescale_countdown_.reset();
  num_consecutive_expands_ = 0;
}

void DecisionLogic::SoftReset() {
  packet_length_samples_ = 0;
  sample_memory_ = 0;
  prev_time_scale_ = false;
  timescale_countdown_ =
      tick_timer_->GetNewCountdown(kMinTimescaleInterval + 1);
}

void DecisionLogic::SetSampleRate(int fs_hz, size_t output_size_samples) {
  // TODO(hlundin): Change to an enumerator and skip assert.
  assert(fs_hz == 8000 || fs_hz == 16000 || fs_hz == 32000 || fs_hz == 48000);
  fs_mult_ = fs_hz / 8000;
  output_size_samples_ = output_size_samples;
}

Operations DecisionLogic::GetDecision(const SyncBuffer& sync_buffer,
                                      const Expand& expand,
                                      size_t decoder_frame_length,
                                      const Packet* next_packet,
                                      Modes prev_mode,
                                      bool play_dtmf,
                                      size_t generated_noise_samples,
                                      bool* reset_decoder) {
  // If last mode was CNG (or Expand, since this could be covering up for
  // a lost CNG packet), remember that CNG is on. This is needed if comfort
  // noise is interrupted by DTMF.
  if (prev_mode == kModeRfc3389Cng) {
    cng_state_ = kCngRfc3389On;
  } else if (prev_mode == kModeCodecInternalCng) {
    cng_state_ = kCngInternalOn;
  }

  const size_t samples_left =
      sync_buffer.FutureLength() - expand.overlap_length();
  const size_t cur_size_samples =
      samples_left + packet_buffer_.NumSamplesInBuffer(decoder_frame_length);

  prev_time_scale_ =
      prev_time_scale_ && (prev_mode == kModeAccelerateSuccess ||
                           prev_mode == kModeAccelerateLowEnergy ||
                           prev_mode == kModePreemptiveExpandSuccess ||
                           prev_mode == kModePreemptiveExpandLowEnergy);

  FilterBufferLevel(cur_size_samples, prev_mode);

  // Guard for errors, to avoid getting stuck in error mode.
  if (prev_mode == kModeError) {
    if (!next_packet) {
      return kExpand;
    } else {
      return kUndefined;  // Use kUndefined to flag for a reset.
    }
  }

  uint32_t target_timestamp = sync_buffer.end_timestamp();
  uint32_t available_timestamp = 0;
  bool is_cng_packet = false;
  if (next_packet) {
    available_timestamp = next_packet->timestamp;
    is_cng_packet =
        decoder_database_->IsComfortNoise(next_packet->payload_type);
  }

  if (is_cng_packet) {
    return CngOperation(prev_mode, target_timestamp, available_timestamp,
                        generated_noise_samples);
  }

  // Handle the case with no packet at all available (except maybe DTMF).
  if (!next_packet) {
    return NoPacket(play_dtmf);
  }

  // If the expand period was very long, reset NetEQ since it is likely that the
  // sender was restarted.
  if (num_consecutive_expands_ > kReinitAfterExpands) {
    *reset_decoder = true;
    return kNormal;
  }

  // Make sure we don't restart audio too soon after an expansion to avoid
  // running out of data right away again. We should only wait if there are no
  // DTX or CNG packets in the buffer (otherwise we should just play out what we
  // have, since we cannot know the exact duration of DTX or CNG packets), and
  // if the mute factor is low enough (otherwise the expansion was short enough
  // to not be noticable).
  // Note that the MuteFactor is in Q14, so a value of 16384 corresponds to 1.
  if ((prev_mode == kModeExpand || prev_mode == kModeCodecPlc) &&
      expand.MuteFactor(0) < 16384 / 2 &&
      cur_size_samples < static_cast<size_t>(
              delay_manager_->TargetLevel() * packet_length_samples_ *
              postpone_decoding_level_ / 100) >> 8 &&
      !packet_buffer_.ContainsDtxOrCngPacket(decoder_database_)) {
    RTC_DCHECK(webrtc::field_trial::IsEnabled(kPostponeDecodingFieldTrial));
    return kExpand;
  }

  const uint32_t five_seconds_samples =
      static_cast<uint32_t>(5 * 8000 * fs_mult_);
  // Check if the required packet is available.
  if (target_timestamp == available_timestamp) {
    return ExpectedPacketAvailable(prev_mode, play_dtmf);
  } else if (!PacketBuffer::IsObsoleteTimestamp(
                 available_timestamp, target_timestamp, five_seconds_samples)) {
    return FuturePacketAvailable(
        sync_buffer, expand, decoder_frame_length, prev_mode, target_timestamp,
        available_timestamp, play_dtmf, generated_noise_samples);
  } else {
    // This implies that available_timestamp < target_timestamp, which can
    // happen when a new stream or codec is received. Signal for a reset.
    return kUndefined;
  }
}

void DecisionLogic::ExpandDecision(Operations operation) {
  if (operation == kExpand) {
    num_consecutive_expands_++;
  } else {
    num_consecutive_expands_ = 0;
  }
}

void DecisionLogic::FilterBufferLevel(size_t buffer_size_samples,
                                      Modes prev_mode) {
  // Do not update buffer history if currently playing CNG since it will bias
  // the filtered buffer level.
  if ((prev_mode != kModeRfc3389Cng) && (prev_mode != kModeCodecInternalCng)) {
    buffer_level_filter_->SetTargetBufferLevel(
        delay_manager_->base_target_level());

    size_t buffer_size_packets = 0;
    if (packet_length_samples_ > 0) {
      // Calculate size in packets.
      buffer_size_packets = buffer_size_samples / packet_length_samples_;
    }
    int sample_memory_local = 0;
    if (prev_time_scale_) {
      sample_memory_local = sample_memory_;
      timescale_countdown_ =
          tick_timer_->GetNewCountdown(kMinTimescaleInterval);
    }
    buffer_level_filter_->Update(buffer_size_packets, sample_memory_local,
                                 packet_length_samples_);
    prev_time_scale_ = false;
  }
}

Operations DecisionLogic::CngOperation(Modes prev_mode,
                                       uint32_t target_timestamp,
                                       uint32_t available_timestamp,
                                       size_t generated_noise_samples) {
  // Signed difference between target and available timestamp.
  int32_t timestamp_diff = static_cast<int32_t>(
      static_cast<uint32_t>(generated_noise_samples + target_timestamp) -
      available_timestamp);
  int32_t optimal_level_samp = static_cast<int32_t>(
      (delay_manager_->TargetLevel() * packet_length_samples_) >> 8);
  const int64_t excess_waiting_time_samp =
      -static_cast<int64_t>(timestamp_diff) - optimal_level_samp;

  if (excess_waiting_time_samp > optimal_level_samp / 2) {
    // The waiting time for this packet will be longer than 1.5
    // times the wanted buffer delay. Apply fast-forward to cut the
    // waiting time down to the optimal.
    noise_fast_forward_ = rtc::dchecked_cast<size_t>(noise_fast_forward_ +
                                                     excess_waiting_time_samp);
    timestamp_diff =
        rtc::saturated_cast<int32_t>(timestamp_diff + excess_waiting_time_samp);
  }

  if (timestamp_diff < 0 && prev_mode == kModeRfc3389Cng) {
    // Not time to play this packet yet. Wait another round before using this
    // packet. Keep on playing CNG from previous CNG parameters.
    return kRfc3389CngNoPacket;
  } else {
    // Otherwise, go for the CNG packet now.
    noise_fast_forward_ = 0;
    return kRfc3389Cng;
  }
}

Operations DecisionLogic::NoPacket(bool play_dtmf) {
  if (cng_state_ == kCngRfc3389On) {
    // Keep on playing comfort noise.
    return kRfc3389CngNoPacket;
  } else if (cng_state_ == kCngInternalOn) {
    // Keep on playing codec internal comfort noise.
    return kCodecInternalCng;
  } else if (play_dtmf) {
    return kDtmf;
  } else {
    // Nothing to play, do expand.
    return kExpand;
  }
}

Operations DecisionLogic::ExpectedPacketAvailable(Modes prev_mode,
                                                  bool play_dtmf) {
  if (!disallow_time_stretching_ && prev_mode != kModeExpand && !play_dtmf) {
    // Check criterion for time-stretching.
    int low_limit, high_limit;
    delay_manager_->BufferLimits(&low_limit, &high_limit);
    if (buffer_level_filter_->filtered_current_level() >= high_limit << 2)
      return kFastAccelerate;
    if (TimescaleAllowed()) {
      if (buffer_level_filter_->filtered_current_level() >= high_limit)
        return kAccelerate;
      if (buffer_level_filter_->filtered_current_level() < low_limit)
        return kPreemptiveExpand;
    }
  }
  return kNormal;
}

Operations DecisionLogic::FuturePacketAvailable(
    const SyncBuffer& sync_buffer,
    const Expand& expand,
    size_t decoder_frame_length,
    Modes prev_mode,
    uint32_t target_timestamp,
    uint32_t available_timestamp,
    bool play_dtmf,
    size_t generated_noise_samples) {
  // Required packet is not available, but a future packet is.
  // Check if we should continue with an ongoing expand because the new packet
  // is too far into the future.
  uint32_t timestamp_leap = available_timestamp - target_timestamp;
  if ((prev_mode == kModeExpand || prev_mode == kModeCodecPlc) &&
      !ReinitAfterExpands(timestamp_leap) && !MaxWaitForPacket() &&
      PacketTooEarly(timestamp_leap) && UnderTargetLevel()) {
    if (play_dtmf) {
      // Still have DTMF to play, so do not do expand.
      return kDtmf;
    } else {
      // Nothing to play.
      return kExpand;
    }
  }

  if (prev_mode == kModeCodecPlc) {
    return kNormal;
  }

  const size_t samples_left =
      sync_buffer.FutureLength() - expand.overlap_length();
  const size_t cur_size_samples =
      samples_left + packet_buffer_.NumPacketsInBuffer() * decoder_frame_length;

  // If previous was comfort noise, then no merge is needed.
  if (prev_mode == kModeRfc3389Cng || prev_mode == kModeCodecInternalCng) {
    // Keep the same delay as before the CNG, but make sure that the number of
    // samples in buffer is no higher than 4 times the optimal level. (Note that
    // TargetLevel() is in Q8.)
    if (static_cast<uint32_t>(generated_noise_samples + target_timestamp) >=
            available_timestamp ||
        cur_size_samples >
            ((delay_manager_->TargetLevel() * packet_length_samples_) >> 8) *
                4) {
      // Time to play this new packet.
      return kNormal;
    } else {
      // Too early to play this new packet; keep on playing comfort noise.
      if (prev_mode == kModeRfc3389Cng) {
        return kRfc3389CngNoPacket;
      } else {  // prevPlayMode == kModeCodecInternalCng.
        return kCodecInternalCng;
      }
    }
  }
  // Do not merge unless we have done an expand before.
  if (prev_mode == kModeExpand) {
    return kMerge;
  } else if (play_dtmf) {
    // Play DTMF instead of expand.
    return kDtmf;
  } else {
    return kExpand;
  }
}

bool DecisionLogic::UnderTargetLevel() const {
  return buffer_level_filter_->filtered_current_level() <=
         delay_manager_->TargetLevel();
}

bool DecisionLogic::ReinitAfterExpands(uint32_t timestamp_leap) const {
  return timestamp_leap >=
         static_cast<uint32_t>(output_size_samples_ * kReinitAfterExpands);
}

bool DecisionLogic::PacketTooEarly(uint32_t timestamp_leap) const {
  return timestamp_leap >
         static_cast<uint32_t>(output_size_samples_ * num_consecutive_expands_);
}

bool DecisionLogic::MaxWaitForPacket() const {
  return num_consecutive_expands_ >= kMaxWaitForPacket;
}

}  // namespace webrtc
