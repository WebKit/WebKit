/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_mode_level_estimator.h"

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper)
    : saturation_protector_(apm_data_dumper),
      apm_data_dumper_(apm_data_dumper) {}

void AdaptiveModeLevelEstimator::UpdateEstimation(
    const VadWithLevel::LevelAndProbability& vad_data) {
  RTC_DCHECK_GT(vad_data.speech_rms_dbfs, -150.f);
  RTC_DCHECK_LT(vad_data.speech_rms_dbfs, 50.f);
  RTC_DCHECK_GT(vad_data.speech_peak_dbfs, -150.f);
  RTC_DCHECK_LT(vad_data.speech_peak_dbfs, 50.f);
  RTC_DCHECK_GE(vad_data.speech_probability, 0.f);
  RTC_DCHECK_LE(vad_data.speech_probability, 1.f);

  if (vad_data.speech_probability < kVadConfidenceThreshold) {
    DebugDumpEstimate();
    return;
  }

  const bool buffer_is_full = buffer_size_ms_ >= kFullBufferSizeMs;
  if (!buffer_is_full) {
    buffer_size_ms_ += kFrameDurationMs;
  }

  const float leak_factor = buffer_is_full ? kFullBufferLeakFactor : 1.f;

  estimate_numerator_ = estimate_numerator_ * leak_factor +
                        vad_data.speech_rms_dbfs * vad_data.speech_probability;
  estimate_denominator_ =
      estimate_denominator_ * leak_factor + vad_data.speech_probability;

  last_estimate_with_offset_dbfs_ = estimate_numerator_ / estimate_denominator_;

  saturation_protector_.UpdateMargin(vad_data, last_estimate_with_offset_dbfs_);
  DebugDumpEstimate();
}

float AdaptiveModeLevelEstimator::LatestLevelEstimate() const {
  return rtc::SafeClamp<float>(
      last_estimate_with_offset_dbfs_ + saturation_protector_.LastMargin(),
      -90.f, 30.f);
}

void AdaptiveModeLevelEstimator::Reset() {
  buffer_size_ms_ = 0;
  last_estimate_with_offset_dbfs_ = kInitialSpeechLevelEstimateDbfs;
  estimate_numerator_ = 0.f;
  estimate_denominator_ = 0.f;
  saturation_protector_.Reset();
}

void AdaptiveModeLevelEstimator::DebugDumpEstimate() {
  apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_with_offset_dbfs",
                            last_estimate_with_offset_dbfs_);
  apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_dbfs",
                            LatestLevelEstimate());
  saturation_protector_.DebugDumpEstimate();
}
}  // namespace webrtc
