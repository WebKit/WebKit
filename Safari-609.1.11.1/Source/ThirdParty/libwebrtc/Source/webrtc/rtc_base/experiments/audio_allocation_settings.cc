/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/audio_allocation_settings.h"

#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
// OverheadPerPacket = Ipv4(20B) + UDP(8B) + SRTP(10B) + RTP(12)
constexpr int kOverheadPerPacket = 20 + 8 + 10 + 12;
}  // namespace
AudioAllocationSettings::AudioAllocationSettings()
    : audio_send_side_bwe_(field_trial::IsEnabled("WebRTC-Audio-SendSideBwe")),
      allocate_audio_without_feedback_(
          field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")),
      force_no_audio_feedback_(
          field_trial::IsEnabled("WebRTC-Audio-ForceNoTWCC")),
      enable_audio_alr_probing_(
          !field_trial::IsDisabled("WebRTC-Audio-AlrProbing")),
      send_side_bwe_with_overhead_(
          field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      min_bitrate_("min"),
      max_bitrate_("max"),
      priority_bitrate_("prio_rate", DataRate::Zero()),
      priority_bitrate_raw_("prio_rate_raw"),
      bitrate_priority_("rate_prio") {
  ParseFieldTrial({&min_bitrate_, &max_bitrate_, &priority_bitrate_,
                   &priority_bitrate_raw_, &bitrate_priority_},
                  field_trial::FindFullName("WebRTC-Audio-Allocation"));

  // TODO(mflodman): Keep testing this and set proper values.
  // Note: This is an early experiment currently only supported by Opus.
  if (send_side_bwe_with_overhead_) {
    constexpr int kMaxPacketSizeMs = WEBRTC_OPUS_SUPPORT_120MS_PTIME ? 120 : 60;
    min_overhead_bps_ = kOverheadPerPacket * 8 * 1000 / kMaxPacketSizeMs;
  }
  // priority_bitrate_raw will override priority_bitrate.
  if (priority_bitrate_raw_ && !priority_bitrate_->IsZero()) {
    RTC_LOG(LS_WARNING) << "'priority_bitrate' and '_raw' are mutually "
                           "exclusive but both were configured.";
  }
}

AudioAllocationSettings::~AudioAllocationSettings() {}

bool AudioAllocationSettings::ForceNoAudioFeedback() const {
  return force_no_audio_feedback_;
}

bool AudioAllocationSettings::IgnoreSeqNumIdChange() const {
  return !audio_send_side_bwe_;
}

bool AudioAllocationSettings::ConfigureRateAllocationRange() const {
  return audio_send_side_bwe_;
}

bool AudioAllocationSettings::ShouldSendTransportSequenceNumber(
    int transport_seq_num_extension_header_id) const {
  if (force_no_audio_feedback_)
    return false;
  return audio_send_side_bwe_ && !allocate_audio_without_feedback_ &&
         transport_seq_num_extension_header_id != 0;
}

bool AudioAllocationSettings::RequestAlrProbing() const {
  return enable_audio_alr_probing_;
}

bool AudioAllocationSettings::IncludeAudioInAllocationOnStart(
    int min_bitrate_bps,
    int max_bitrate_bps,
    bool has_dscp,
    int transport_seq_num_extension_header_id) const {
  if (has_dscp || min_bitrate_bps == -1 || max_bitrate_bps == -1)
    return false;
  if (transport_seq_num_extension_header_id != 0 && !force_no_audio_feedback_)
    return true;
  if (allocate_audio_without_feedback_)
    return true;
  if (audio_send_side_bwe_)
    return false;
  return true;
}

bool AudioAllocationSettings::IncludeAudioInAllocationOnReconfigure(
    int min_bitrate_bps,
    int max_bitrate_bps,
    bool has_dscp,
    int transport_seq_num_extension_header_id) const {
  // TODO(srte): Make this match include_audio_in_allocation_on_start.
  if (has_dscp || min_bitrate_bps == -1 || max_bitrate_bps == -1)
    return false;
  if (transport_seq_num_extension_header_id != 0)
    return true;
  if (audio_send_side_bwe_)
    return false;
  return true;
}

bool AudioAllocationSettings::IncludeOverheadInAudioAllocation() const {
  return send_side_bwe_with_overhead_;
}

absl::optional<DataRate> AudioAllocationSettings::MinBitrate() const {
  return min_bitrate_.GetOptional();
}
absl::optional<DataRate> AudioAllocationSettings::MaxBitrate() const {
  return max_bitrate_.GetOptional();
}
DataRate AudioAllocationSettings::DefaultPriorityBitrate() const {
  DataRate max_overhead = DataRate::Zero();
  if (priority_bitrate_raw_) {
    return *priority_bitrate_raw_;
  }
  if (send_side_bwe_with_overhead_) {
    const TimeDelta kMinPacketDuration = TimeDelta::ms(20);
    max_overhead = DataSize::bytes(kOverheadPerPacket) / kMinPacketDuration;
  }
  return priority_bitrate_.Get() + max_overhead;
}
absl::optional<double> AudioAllocationSettings::BitratePriority() const {
  return bitrate_priority_.GetOptional();
}

}  // namespace webrtc
