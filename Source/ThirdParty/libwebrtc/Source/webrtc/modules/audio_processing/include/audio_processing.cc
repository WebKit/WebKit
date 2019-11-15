/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_processing.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

std::string NoiseSuppressionLevelToString(
    const AudioProcessing::Config::NoiseSuppression::Level& level) {
  switch (level) {
    case AudioProcessing::Config::NoiseSuppression::Level::kLow:
      return "Low";
    case AudioProcessing::Config::NoiseSuppression::Level::kModerate:
      return "Moderate";
    case AudioProcessing::Config::NoiseSuppression::Level::kHigh:
      return "High";
    case AudioProcessing::Config::NoiseSuppression::Level::kVeryHigh:
      return "VeryHigh";
  }
}

std::string GainController1ModeToString(
    const AudioProcessing::Config::GainController1::Mode& mode) {
  switch (mode) {
    case AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog:
      return "AdaptiveAnalog";
    case AudioProcessing::Config::GainController1::Mode::kAdaptiveDigital:
      return "AdaptiveDigital";
    case AudioProcessing::Config::GainController1::Mode::kFixedDigital:
      return "FixedDigital";
  }
}

std::string GainController2LevelEstimatorToString(
    const AudioProcessing::Config::GainController2::LevelEstimator& level) {
  switch (level) {
    case AudioProcessing::Config::GainController2::LevelEstimator::kRms:
      return "Rms";
    case AudioProcessing::Config::GainController2::LevelEstimator::kPeak:
      return "Peak";
  }
}

}  // namespace

void CustomProcessing::SetRuntimeSetting(
    AudioProcessing::RuntimeSetting setting) {}

std::string AudioProcessing::Config::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder builder(buf);
  builder << "AudioProcessing::Config{ "
          << "pre_amplifier: { enabled: " << pre_amplifier.enabled
          << ", fixed_gain_factor: " << pre_amplifier.fixed_gain_factor
          << " }, high_pass_filter: { enabled: " << high_pass_filter.enabled
          << " }, echo_canceller: { enabled: " << echo_canceller.enabled
          << ", mobile_mode: " << echo_canceller.mobile_mode
          << ", legacy_moderate_suppression_level: "
          << echo_canceller.legacy_moderate_suppression_level
          << ", use_legacy_aec: " << echo_canceller.use_legacy_aec
          << " }, noise_suppression: { enabled: " << noise_suppression.enabled
          << ", level: "
          << NoiseSuppressionLevelToString(noise_suppression.level)
          << " }, voice_detection: { enabled: " << voice_detection.enabled
          << " }, gain_controller1: { enabled: " << gain_controller1.enabled
          << ", mode: " << GainController1ModeToString(gain_controller1.mode)
          << ", target_level_dbfs: " << gain_controller1.target_level_dbfs
          << ", compression_gain_db: " << gain_controller1.compression_gain_db
          << ", enable_limiter: " << gain_controller1.enable_limiter
          << ", analog_level_minimum: " << gain_controller1.analog_level_minimum
          << ", analog_level_maximum: " << gain_controller1.analog_level_maximum
          << " }, gain_controller2: { enabled: " << gain_controller2.enabled
          << ", fixed_digital: { gain_db: "
          << gain_controller2.fixed_digital.gain_db
          << " }, adaptive_digital: { enabled: "
          << gain_controller2.adaptive_digital.enabled << ", level_estimator: "
          << GainController2LevelEstimatorToString(
                 gain_controller2.adaptive_digital.level_estimator)
          << ", use_saturation_protector: "
          << gain_controller2.adaptive_digital.use_saturation_protector
          << ", extra_saturation_margin_db: "
          << gain_controller2.adaptive_digital.extra_saturation_margin_db
          << " } }, residual_echo_detector: { enabled: "
          << residual_echo_detector.enabled
          << " }, level_estimation: { enabled: " << level_estimation.enabled
          << " } }";
  return builder.str();
}

}  // namespace webrtc
