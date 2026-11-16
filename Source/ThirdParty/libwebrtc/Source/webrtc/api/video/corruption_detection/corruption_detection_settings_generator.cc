/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/corruption_detection_settings_generator.h"

#include <algorithm>
#include <cmath>
#include <variant>

#include "api/video/corruption_detection/corruption_detection_filter_settings.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
void ValidateParameters(
    const CorruptionDetectionSettingsGenerator::ErrorThresholds&
        default_error_thresholds,
    const CorruptionDetectionSettingsGenerator::TransientParameters&
        transient_params) {
  int offset = transient_params.keyframe_threshold_offset;
  RTC_DCHECK_GE(offset, 0);
  RTC_DCHECK_LE(offset, 15);
  RTC_DCHECK_GE(default_error_thresholds.chroma, 0);
  RTC_DCHECK_LE(default_error_thresholds.chroma + offset, 15);
  RTC_DCHECK_GE(default_error_thresholds.luma, 0);
  RTC_DCHECK_LE(default_error_thresholds.luma + offset, 15);

  RTC_DCHECK_GE(transient_params.max_qp, 0);
  RTC_DCHECK_GE(transient_params.keyframe_stddev_offset, 0.0);
  RTC_DCHECK_GE(transient_params.keyframe_offset_duration_frames, 0);
  RTC_DCHECK_GE(transient_params.large_qp_change_threshold, 0);
  RTC_DCHECK_LE(transient_params.large_qp_change_threshold,
                transient_params.max_qp);
  RTC_DCHECK_GE(transient_params.std_dev_lower_bound, 0.0);
  RTC_DCHECK_LE(transient_params.std_dev_lower_bound, 40.0);
}
}  // namespace

CorruptionDetectionSettingsGenerator::CorruptionDetectionSettingsGenerator(
    const RationalFunctionParameters& function_params,
    const ErrorThresholds& default_error_thresholds,
    const TransientParameters& transient_params)
    : function_params_(function_params),
      error_thresholds_(default_error_thresholds),
      transient_params_(transient_params),
      frames_since_keyframe_(0) {
  ValidateParameters(default_error_thresholds, transient_params);
}

CorruptionDetectionSettingsGenerator::CorruptionDetectionSettingsGenerator(
    const ExponentialFunctionParameters& function_params,
    const ErrorThresholds& default_error_thresholds,
    const TransientParameters& transient_params)
    : function_params_(function_params),
      error_thresholds_(default_error_thresholds),
      transient_params_(transient_params),
      frames_since_keyframe_(0) {
  ValidateParameters(default_error_thresholds, transient_params);
}

CorruptionDetectionFilterSettings CorruptionDetectionSettingsGenerator::OnFrame(
    bool is_keyframe,
    int qp) {
  double std_dev = CalculateStdDev(qp);
  int y_err = error_thresholds_.luma;
  int uv_err = error_thresholds_.chroma;

  if (is_keyframe || (transient_params_.large_qp_change_threshold > 0 &&
                      std::abs(previous_qp_.value_or(qp) - qp) >=
                          transient_params_.large_qp_change_threshold)) {
    frames_since_keyframe_ = 0;
  }
  previous_qp_ = qp;

  if (frames_since_keyframe_ <=
      transient_params_.keyframe_offset_duration_frames) {
    // The progress, from the start at the keyframe at 0.0 to completely back to
    // normal at 1.0.
    double progress = transient_params_.keyframe_offset_duration_frames == 0
                          ? 1.0
                          : (static_cast<double>(frames_since_keyframe_) /
                             transient_params_.keyframe_offset_duration_frames);
    double adjusted_std_dev =
        std::min(std_dev + transient_params_.keyframe_stddev_offset, 40.0);
    double adjusted_y_err =
        std::min(y_err + transient_params_.keyframe_threshold_offset, 15);
    double adjusted_uv_err =
        std::min(uv_err + transient_params_.keyframe_threshold_offset, 15);

    std_dev = ((1.0 - progress) * adjusted_std_dev) + (progress * std_dev);
    y_err = static_cast<int>(((1.0 - progress) * adjusted_y_err) +
                             (progress * y_err) + 0.5);
    uv_err = static_cast<int>(((1.0 - progress) * adjusted_uv_err) +
                              (progress * uv_err) + 0.5);
  }

  ++frames_since_keyframe_;

  std_dev = std::max(std_dev, transient_params_.std_dev_lower_bound);
  std_dev = std::min(std_dev, 40.0);

  return CorruptionDetectionFilterSettings{.std_dev = std_dev,
                                           .luma_error_threshold = y_err,
                                           .chroma_error_threshold = uv_err};
}

double CorruptionDetectionSettingsGenerator::CalculateStdDev(int qp) const {
  if (std::holds_alternative<RationalFunctionParameters>(function_params_)) {
    const auto& params = std::get<RationalFunctionParameters>(function_params_);
    return (qp * params.numerator_factor) / (qp + params.denumerator_term) +
           params.offset;
  }
  RTC_DCHECK(
      std::holds_alternative<ExponentialFunctionParameters>(function_params_));

  const auto& params =
      std::get<ExponentialFunctionParameters>(function_params_);
  return params.scale *
         std::exp(params.exponent_factor * qp - params.exponent_offset);
}

}  // namespace webrtc
