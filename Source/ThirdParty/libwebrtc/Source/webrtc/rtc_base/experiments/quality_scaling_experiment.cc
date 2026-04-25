/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/quality_scaling_experiment.h"

#include <cstdio>
#include <optional>
#include <string>

#include "api/field_trials_view.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// This experiment controls QP thresholds for VP8, VP9, H264 and Generic codecs.
// Generic includes H265X but not standard H265.
constexpr char kFieldTrial[] = "WebRTC-Video-QualityScaling";
constexpr int kMinQp = 1;
constexpr int kMaxVp8Qp = 127;
constexpr int kMaxVp9Qp = 255;
constexpr int kMaxH264Qp = 51;
constexpr int kMaxGenericQp = 255;

#if !defined(WEBRTC_IOS)
// On non-iOS, this default string is used unless explicitly overriden.
// TODO(https://crbug.com/400338987): For use cases that does not explicitly
// turn the QP experiment on (e.g. Chrome), it does not make sense for this QP
// threshold to override the QP thresholds provided by the encoder
// implementation - we should trust that an encoder implementation that reports
// its own QP thresholds would know best, and only use these as a fallback for
// when the encoder does not specify any.
constexpr char kDefaultQualityScalingSetttings[] =
    "Enabled-29,95,149,205,24,37,26,36,0.9995,0.9999,1";
#endif

std::optional<VideoEncoder::QpThresholds> GetThresholds(int low,
                                                        int high,
                                                        int max) {
  if (low < kMinQp || high > max || high < low)
    return std::nullopt;

  RTC_LOG(LS_INFO) << "QP thresholds: low: " << low << ", high: " << high;
  return std::optional<VideoEncoder::QpThresholds>(
      VideoEncoder::QpThresholds(low, high));
}

// This experiment controls QP thresholds for standard H265 (not H265X).
// - Only for debugging/experimentation. Once QP thresholds have been determined
//   it is up to the encoder implementation to provide
//   VideoEncoder::EncoderInfo::scaling_settings.
//
// Example usage:
// --force-fieldtrials=WebRTC-H265-QualityScaling/low_qp:27,high_qp:35/
struct WebRTCH265QualityScaling {
  static constexpr char kFieldTrialName[] = "WebRTC-H265-QualityScaling";

  WebRTCH265QualityScaling(const FieldTrialsView& field_trials)
      : low_qp("low_qp"), high_qp("high_qp") {
    ParseFieldTrial({&low_qp, &high_qp}, field_trials.Lookup(kFieldTrialName));
  }

  bool IsEnabled() const { return low_qp && high_qp; }
  VideoEncoder::QpThresholds ToQpThresholds() const {
    RTC_DCHECK(IsEnabled());
    return VideoEncoder::QpThresholds(*low_qp, *high_qp);
  }

  FieldTrialOptional<int> low_qp;
  FieldTrialOptional<int> high_qp;
};
}  // namespace

bool QualityScalingExperiment::Enabled(const FieldTrialsView& field_trials) {
  WebRTCH265QualityScaling h265_quality_scaling(field_trials);
  return
#if defined(WEBRTC_IOS)
      field_trials.IsEnabled(kFieldTrial) ||
#else
      !field_trials.IsDisabled(kFieldTrial) ||
#endif
      h265_quality_scaling.IsEnabled();
}

std::optional<QualityScalingExperiment::Settings>
QualityScalingExperiment::ParseSettings(const FieldTrialsView& field_trials) {
  std::string group = field_trials.Lookup(kFieldTrial);
  // TODO(http://crbug.com/webrtc/12401): Completely remove the experiment code
  // after few releases.
#if !defined(WEBRTC_IOS)
  if (group.empty())
    group = kDefaultQualityScalingSetttings;
#endif
  Settings s;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%d",
             &s.vp8_low, &s.vp8_high, &s.vp9_low, &s.vp9_high, &s.h264_low,
             &s.h264_high, &s.generic_low, &s.generic_high, &s.alpha_high,
             &s.alpha_low, &s.drop) != 11) {
    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
    return std::nullopt;
  }
  return s;
}

std::optional<VideoEncoder::QpThresholds>
QualityScalingExperiment::GetQpThresholds(VideoCodecType codec_type,
                                          const FieldTrialsView& field_trials) {
  if (codec_type == kVideoCodecH265) {
    WebRTCH265QualityScaling h265_quality_scaling(field_trials);
    if (h265_quality_scaling.IsEnabled()) {
      return h265_quality_scaling.ToQpThresholds();
    }
  }
  const auto settings = ParseSettings(field_trials);
  if (!settings)
    return std::nullopt;

  switch (codec_type) {
    case kVideoCodecVP8:
      return GetThresholds(settings->vp8_low, settings->vp8_high, kMaxVp8Qp);
    case kVideoCodecVP9:
      return GetThresholds(settings->vp9_low, settings->vp9_high, kMaxVp9Qp);
    case kVideoCodecH264:
      return GetThresholds(settings->h264_low, settings->h264_high, kMaxH264Qp);
    case kVideoCodecGeneric:
      return GetThresholds(settings->generic_low, settings->generic_high,
                           kMaxGenericQp);
    default:
      return std::nullopt;
  }
}

QualityScalingExperiment::Config QualityScalingExperiment::GetConfig(
    const FieldTrialsView& field_trials) {
  const auto settings = ParseSettings(field_trials);
  if (!settings)
    return Config();

  Config config;
  config.use_all_drop_reasons = settings->drop > 0;

  if (settings->alpha_high < 0 || settings->alpha_low < settings->alpha_high) {
    RTC_LOG(LS_WARNING) << "Invalid alpha value provided, using default.";
    return config;
  }
  config.alpha_high = settings->alpha_high;
  config.alpha_low = settings->alpha_low;
  return config;
}

}  // namespace webrtc
