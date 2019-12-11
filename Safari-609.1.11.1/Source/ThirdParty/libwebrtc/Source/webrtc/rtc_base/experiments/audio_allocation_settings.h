/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_

#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
namespace webrtc {
// This class encapsulates the logic that controls how allocation of audio
// bitrate is done. This is primarily based on field trials, but also on the
// values of audio parameters.
class AudioAllocationSettings {
 public:
  AudioAllocationSettings();
  ~AudioAllocationSettings();
  // Returns true if audio feedback should be force disabled.
  bool ForceNoAudioFeedback() const;
  // Returns true if changes in transport sequence number id should be ignored
  // as a trigger for reconfiguration.
  bool IgnoreSeqNumIdChange() const;
  // Returns true if the bitrate allocation range should be configured.
  bool ConfigureRateAllocationRange() const;
  // Returns true if sent audio packets should have transport wide sequence
  // numbers.
  // |transport_seq_num_extension_header_id| the extension header id for
  // transport sequence numbers. Set to 0 if not the extension is not
  // configured.
  bool ShouldSendTransportSequenceNumber(
      int transport_seq_num_extension_header_id) const;
  // Returns true if audio should request ALR probing from network controller.
  bool RequestAlrProbing() const;
  // Returns true if audio should be added to rate allocation when the audio
  // stream is started.
  // |min_bitrate_bps| the configured min bitrate, set to -1 if unset.
  // |max_bitrate_bps| the configured max bitrate, set to -1 if unset.
  // |has_dscp| true is dscp is enabled.
  // |transport_seq_num_extension_header_id| the extension header id for
  // transport sequence numbers. Set to 0 if not the extension is not
  // configured.
  bool IncludeAudioInAllocationOnStart(
      int min_bitrate_bps,
      int max_bitrate_bps,
      bool has_dscp,
      int transport_seq_num_extension_header_id) const;
  // Returns true if audio should be added to rate allocation when the audio
  // stream is reconfigured.
  // |min_bitrate_bps| the configured min bitrate, set to -1 if unset.
  // |max_bitrate_bps| the configured max bitrate, set to -1 if unset.
  // |has_dscp| true is dscp is enabled.
  // |transport_seq_num_extension_header_id| the extension header id for
  // transport sequence numbers. Set to 0 if not the extension is not
  // configured.
  bool IncludeAudioInAllocationOnReconfigure(
      int min_bitrate_bps,
      int max_bitrate_bps,
      bool has_dscp,
      int transport_seq_num_extension_header_id) const;
  // Returns true if we should include packet overhead in audio allocation.
  bool IncludeOverheadInAudioAllocation() const;

  // Returns the min bitrate for audio rate allocation.
  absl::optional<DataRate> MinBitrate() const;
  // Returns the max bitrate for audio rate allocation.
  absl::optional<DataRate> MaxBitrate() const;
  // Indicates the default priority bitrate for audio streams. The bitrate
  // allocator will prioritize audio until it reaches this bitrate and will
  // divide bitrate evently between audio and video above this bitrate.
  DataRate DefaultPriorityBitrate() const;

  // The bitrate priority is used to determine how much of the available bitrate
  // beyond the min or priority bitrate audio streams should receive.
  absl::optional<double> BitratePriority() const;

 private:
  const bool audio_send_side_bwe_;
  const bool allocate_audio_without_feedback_;
  const bool force_no_audio_feedback_;
  const bool enable_audio_alr_probing_;
  const bool send_side_bwe_with_overhead_;
  int min_overhead_bps_ = 0;
  // Field Trial configured bitrates to use as overrides over default/user
  // configured bitrate range when audio bitrate allocation is enabled.
  FieldTrialOptional<DataRate> min_bitrate_;
  FieldTrialOptional<DataRate> max_bitrate_;
  FieldTrialParameter<DataRate> priority_bitrate_;
  // By default the priority_bitrate is compensated for packet overhead.
  // Use this flag to configure a raw value instead.
  FieldTrialOptional<DataRate> priority_bitrate_raw_;
  FieldTrialOptional<double> bitrate_priority_;
};
}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_AUDIO_ALLOCATION_SETTINGS_H_
