/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/speech_level_estimator.h"

#include <memory>
#include <string>

#include "api/audio/audio_processing.h"
#include "api/field_trials_view.h"
#include "modules/audio_processing/agc2/speech_level_estimator_experimental_impl.h"
#include "modules/audio_processing/agc2/speech_level_estimator_impl.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

struct SpeechLevelEstimatorExperimentalConfig {
  float background_speaker_offset_dbfs;
  int max_time_to_update_ms;
};

SpeechLevelEstimatorExperimentalConfig
FetchSpeechLevelEstimatorExperimentalConfig(
    const FieldTrialsView& field_trials) {
  constexpr float kDefaultBackgroundSpeakerOffsetDbfs =
      SpeechLevelEstimatorExperimentalImpl::kDefaultBackgroundSpeakerOffsetDbfs;
  constexpr int kDefaultMaxTimeToUpdateMs =
      SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs;
  float offset_db = kDefaultBackgroundSpeakerOffsetDbfs;
  int max_time_ms = kDefaultMaxTimeToUpdateMs;
  const std::string field_trial =
      field_trials.Lookup("WebRTC-Agc2SpeechLevelEstimatorExperimental");
  FieldTrialFlag enabled_param("Enabled");
  FieldTrialParameter<double> offset_param(
      /*key=*/"offset", /*default_value=*/kDefaultBackgroundSpeakerOffsetDbfs);
  FieldTrialParameter<int> max_time_param(
      /*key=*/"max_time_ms", /*default_value=*/kDefaultMaxTimeToUpdateMs);
  ParseFieldTrial({&enabled_param, &offset_param, &max_time_param},
                  field_trial);

  float offset_read = static_cast<float>(offset_param.Get());

  if (offset_read > 0.0f && offset_read < 90.0f) {
    offset_db = offset_read;
  } else if (offset_read != kDefaultBackgroundSpeakerOffsetDbfs) {
    RTC_LOG(LS_ERROR) << "AGC2: SpeechLevelEstimatorExperimental: wrong input, "
                         "background speaker offset = "
                      << offset_read << ". Using default: "
                      << kDefaultBackgroundSpeakerOffsetDbfs;
  }
  RTC_LOG(LS_INFO) << "AGC2: SpeechLevelEstimatorExperimental: "
                      "background_speaker_offset_db = "
                   << offset_db;

  int max_time_read = max_time_param.Get();
  if (max_time_read > 0) {
    max_time_ms = max_time_read;
  } else {
    RTC_LOG(LS_ERROR) << "AGC2: SpeechLevelEstimatorExperimental: wrong input, "
                         "max_time_ms = "
                      << max_time_read
                      << ". Using default: " << kDefaultMaxTimeToUpdateMs;
  }
  RTC_LOG(LS_INFO) << "AGC2: SpeechLevelEstimatorExperimental: "
                      "max_time_ms = "
                   << max_time_ms;

  return {.background_speaker_offset_dbfs = offset_db,
          .max_time_to_update_ms = max_time_ms};
}

}  // namespace

std::unique_ptr<SpeechLevelEstimator> SpeechLevelEstimator::Create(
    const FieldTrialsView& field_trials,
    ApmDataDumper* apm_data_dumper,
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config,
    int adjacent_speech_frames_threshold) {
  if (field_trials.IsEnabled("WebRTC-Agc2SpeechLevelEstimatorExperimental")) {
    RTC_LOG(LS_INFO) << "AGC2 using SpeechLevelEstimatorExperimental";
    const auto experimental_config =
        FetchSpeechLevelEstimatorExperimentalConfig(field_trials);
    return std::make_unique<SpeechLevelEstimatorExperimentalImpl>(
        apm_data_dumper, config, adjacent_speech_frames_threshold,
        experimental_config.background_speaker_offset_dbfs,
        experimental_config.max_time_to_update_ms);
  } else {
    RTC_LOG(LS_INFO) << "AGC2 using SpeechLevelEstimator";
    return std::make_unique<SpeechLevelEstimatorImpl>(
        apm_data_dumper, config, adjacent_speech_frames_threshold);
  }
}

}  // namespace webrtc
