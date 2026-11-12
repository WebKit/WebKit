/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_SPEECH_LEVEL_ESTIMATOR_EXPERIMENTAL_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AGC2_SPEECH_LEVEL_ESTIMATOR_EXPERIMENTAL_IMPL_H_

#include <type_traits>

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/agc2/speech_level_estimator.h"

namespace webrtc {
class ApmDataDumper;

// Active speech level estimator that differentiates background speakers and
// low-activity segments to avoid adapting to secondary speech or corrupting the
// tracked target speaker level.
//
// Once the estimator reaches confidence on the primary speaker's level,
// target speaker inactivity is detected in two ways:
// 1. Level drop (background speaker): If accumulated reliable speech is
//    quieter than the tracked level by at least
//    `background_speaker_offset_dbfs`, it is classified as a background speaker
//    and the tracked level is retained.
// 2. Low-activity segment: A primary speaker typically triggers the VAD
//    consistently, whereas a distant speaker or pauses between speech segments
//    produce sporadic bursts. If accumulating the required speech frames
//    exceeds `kMaxTimeToUpdateMs`, accumulation is reset and the segment is
//    flagged as low activity.
class SpeechLevelEstimatorExperimentalImpl : public SpeechLevelEstimator {
 public:
  static constexpr float kDefaultBackgroundSpeakerOffsetDbfs = 7.0f;
  static constexpr int kDefaultMaxTimeToUpdateMs = 5000;

  SpeechLevelEstimatorExperimentalImpl(
      ApmDataDumper* apm_data_dumper,
      const AudioProcessing::Config::GainController2::AdaptiveDigital& config,
      int adjacent_speech_frames_threshold,
      float background_speaker_offset_dbfs,
      int max_time_to_update_ms);
  SpeechLevelEstimatorExperimentalImpl(
      const SpeechLevelEstimatorExperimentalImpl&) = delete;
  SpeechLevelEstimatorExperimentalImpl& operator=(
      const SpeechLevelEstimatorExperimentalImpl&) = delete;

  // Updates the level estimation.
  void Update(float rms_dbfs, float speech_probability) override;
  // Returns the estimated speech plus noise level.
  float GetLevelDbfs() const override { return level_dbfs_; }
  // Returns true if the estimator is confident on its current estimate.
  bool IsConfident() const override { return is_confident_; }
  // Returns true if the target speaker is actively speaking.
  // Returns false if speech is classified as a background speaker or if
  // speech activity is too sporadic to reliably update the target level.
  bool IsTargetSpeakerActive() const override {
    return !is_background_speaker_ && !low_activity_segment_;
  }
  // Returns the threshold offset in dBFS for background speaker detection.
  float GetBackgroundSpeakerOffsetDbfs() const {
    return background_speaker_offset_dbfs_;
  }
  // Returns the maximum time in ms to update the level before resetting state.
  int GetMaxTimeToUpdateMs() const { return max_time_to_update_ms_; }

  void Reset() override;

 private:
  // Part of the level estimator state used for check-pointing and restore ops.
  struct LevelEstimatorState {
    int num_frames;
    float sum_of_levels_dbfs;
  };
  static_assert(std::is_trivially_copyable<LevelEstimatorState>::value, "");

  void UpdateIsConfident();

  void ResetLevelEstimatorState();

  void DumpDebugData() const;

  ApmDataDumper* const apm_data_dumper_;

  const float initial_speech_level_dbfs_;
  const int adjacent_speech_frames_threshold_;
  const float background_speaker_offset_dbfs_;
  const int max_time_to_update_ms_;
  const int max_frames_to_update_;
  LevelEstimatorState preliminary_state_;
  LevelEstimatorState reliable_state_;
  float level_dbfs_;
  bool is_confident_;
  bool is_background_speaker_;
  bool low_activity_segment_;
  int num_adjacent_speech_frames_;
  int num_frames_in_current_update_window_;
  float tracking_level_dbfs_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SPEECH_LEVEL_ESTIMATOR_EXPERIMENTAL_IMPL_H_
