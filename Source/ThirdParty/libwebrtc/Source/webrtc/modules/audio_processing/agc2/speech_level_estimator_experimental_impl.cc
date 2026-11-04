/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/speech_level_estimator_experimental_impl.h"

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

constexpr int kFramesPerUpdate = 100;

float ClampLevelEstimateDbfs(float level_estimate_dbfs) {
  return SafeClamp<float>(level_estimate_dbfs, -90.0f, 30.0f);
}

// Returns the initial speech level estimate needed to apply the initial gain.
float GetInitialSpeechLevelEstimateDbfs(
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config) {
  return ClampLevelEstimateDbfs(-kSaturationProtectorInitialHeadroomDb -
                                config.initial_gain_db - config.headroom_db);
}

}  // namespace

SpeechLevelEstimatorExperimentalImpl::SpeechLevelEstimatorExperimentalImpl(
    ApmDataDumper* apm_data_dumper,
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config,
    int adjacent_speech_frames_threshold,
    float background_speaker_offset_dbfs,
    int max_time_to_update_ms)
    : apm_data_dumper_(apm_data_dumper),
      initial_speech_level_dbfs_(GetInitialSpeechLevelEstimateDbfs(config)),
      adjacent_speech_frames_threshold_(adjacent_speech_frames_threshold),
      background_speaker_offset_dbfs_(background_speaker_offset_dbfs),
      max_time_to_update_ms_(max_time_to_update_ms),
      max_frames_to_update_(max_time_to_update_ms / kFrameDurationMs),
      level_dbfs_(initial_speech_level_dbfs_),
      is_confident_(false),
      is_background_speaker_(false),
      low_activity_segment_(false) {
  RTC_DCHECK(apm_data_dumper_);
  RTC_DCHECK_GE(adjacent_speech_frames_threshold_, 1);
  RTC_DCHECK_GT(background_speaker_offset_dbfs_, 0.0f);
  RTC_DCHECK_GT(max_time_to_update_ms_, 0);
  RTC_DCHECK_GT(max_frames_to_update_, kFramesPerUpdate);
  Reset();
}

void SpeechLevelEstimatorExperimentalImpl::Update(float rms_dbfs,
                                                  float speech_probability) {
  if (speech_probability < kVadConfidenceThreshold) {
    // Not a speech frame. Reset to the last reliable state.
    preliminary_state_ = reliable_state_;
    num_adjacent_speech_frames_ = 0;
  } else {
    // Speech frame observed.
    num_adjacent_speech_frames_++;

    // Update preliminary level estimate.
    preliminary_state_.num_frames++;
    preliminary_state_.sum_of_levels_dbfs += rms_dbfs;

    if (num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_) {
      // The ongoing sequence is long enough to update the reliable state.
      reliable_state_ = preliminary_state_;

      if (reliable_state_.num_frames >= kFramesPerUpdate) {
        low_activity_segment_ = false;
        // The reliable state has enough frames to update the speech level
        // estimation.
        const float reliable_level_dbfs = ClampLevelEstimateDbfs(
            reliable_state_.sum_of_levels_dbfs / reliable_state_.num_frames);
        if (is_confident_ &&
            reliable_level_dbfs <
                level_dbfs_ - background_speaker_offset_dbfs_) {
          // Level drop: detected when reliable speech is significantly quieter
          // than the established target speaker level.
          is_background_speaker_ = true;
        } else {
          is_background_speaker_ = false;
          level_dbfs_ = reliable_level_dbfs;
          is_confident_ = true;
        }
        ResetLevelEstimatorState();
      }
    }
  }

  if (is_confident_ && reliable_state_.num_frames > 0) {
    num_frames_in_current_update_window_++;
    // Low activity: a target speaker triggers the VAD frequently, whereas
    // sporadic bursts that time out before accumulating enough reliable frames
    // are treated as low-activity segments.
    if (num_frames_in_current_update_window_ >= max_frames_to_update_) {
      ResetLevelEstimatorState();
      num_adjacent_speech_frames_ = 0;
      low_activity_segment_ = true;
    }
  }

  DumpDebugData();
}

void SpeechLevelEstimatorExperimentalImpl::Reset() {
  ResetLevelEstimatorState();
  level_dbfs_ = initial_speech_level_dbfs_;
  num_adjacent_speech_frames_ = 0;
  tracking_level_dbfs_ = initial_speech_level_dbfs_;
  is_confident_ = false;
  is_background_speaker_ = false;
  low_activity_segment_ = false;
}

void SpeechLevelEstimatorExperimentalImpl::ResetLevelEstimatorState() {
  preliminary_state_.num_frames = 0;
  preliminary_state_.sum_of_levels_dbfs = 0.0f;
  reliable_state_.num_frames = 0;
  reliable_state_.sum_of_levels_dbfs = 0.0f;
  num_frames_in_current_update_window_ = 0;
}

void SpeechLevelEstimatorExperimentalImpl::DumpDebugData() const {
  if (!apm_data_dumper_)
    return;
  apm_data_dumper_->DumpRaw("agc2_speech_level_dbfs", level_dbfs_);
  apm_data_dumper_->DumpRaw("agc2_speech_level_is_confident", is_confident_);
  apm_data_dumper_->DumpRaw("agc2_speech_level_is_background_speaker",
                            is_background_speaker_);
  apm_data_dumper_->DumpRaw("agc2_speech_level_is_low_activity_segment",
                            low_activity_segment_);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_num_adjacent_speech_frames",
      num_adjacent_speech_frames_);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_preliminary_num_frames",
      preliminary_state_.num_frames);
  apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimator_reliable_num_frames",
                            reliable_state_.num_frames);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_num_frames_in_current_update_window",
      num_frames_in_current_update_window_);
}

}  // namespace webrtc
