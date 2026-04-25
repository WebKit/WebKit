// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "rtc_base/experiments/encoder_speed_experiment.h"

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

namespace {

constexpr absl::string_view kFieldTrialName = "WebRTC-EncoderSpeed";

// Helper function to parse complexity string.
std::optional<VideoCodecComplexity> ParseComplexity(absl::string_view s) {
  if (s.empty()) {
    return std::nullopt;
  } else if (s == "low") {
    return VideoCodecComplexity::kComplexityLow;
  } else if (s == "normal") {
    return VideoCodecComplexity::kComplexityNormal;
  } else if (s == "high") {
    return VideoCodecComplexity::kComplexityHigh;
  } else if (s == "higher") {
    return VideoCodecComplexity::kComplexityHigher;
  } else if (s == "max") {
    return VideoCodecComplexity::kComplexityMax;
  }

  return std::nullopt;  // Invalid value.
}

}  // namespace

void EncoderSpeedExperiment::ParseCodecSettings(absl::string_view codec_name,
                                                absl::string_view trial_string,
                                                ComplexitySettings& settings) {
  FieldTrialParameter<std::string> camera_complexity(
      std::string(codec_name) + "_camera", "");

  FieldTrialParameter<std::string> screenshare_complexity(
      std::string(codec_name) + "_screenshare", "");

  ParseFieldTrial({&camera_complexity, &screenshare_complexity}, trial_string);
  if (auto complexity = ParseComplexity(camera_complexity.Get())) {
    settings.camera = *complexity;
  }

  if (auto complexity = ParseComplexity(screenshare_complexity.Get())) {
    settings.screenshare = *complexity;
  }
}

EncoderSpeedExperiment::EncoderSpeedExperiment(
    const FieldTrialsView& field_trials) {
  std::string trial_string = field_trials.Lookup(kFieldTrialName);
  if (trial_string.empty()) {
    return;
  }

  FieldTrialParameter<bool> dynamic_speed_enabled("dynamic_speed", false);
  ParseFieldTrial({&dynamic_speed_enabled}, trial_string);
  dynamic_speed_enabled_ = dynamic_speed_enabled.Get();

  ParseCodecSettings("av1", trial_string, av1_complexity_);
  ParseCodecSettings("vp8", trial_string, vp8_complexity_);
  ParseCodecSettings("vp9", trial_string, vp9_complexity_);
  ParseCodecSettings("h264", trial_string, h264_complexity_);
  ParseCodecSettings("h265", trial_string, h265_complexity_);
}

bool EncoderSpeedExperiment::IsDynamicSpeedEnabled() const {
  return dynamic_speed_enabled_;
}

VideoCodecComplexity EncoderSpeedExperiment::GetComplexity(
    VideoCodecType codec_type,
    bool is_screenshare) const {
  const ComplexitySettings* settings = nullptr;
  switch (codec_type) {
    case kVideoCodecAV1:
      settings = &av1_complexity_;
      break;
    case kVideoCodecVP8:
      settings = &vp8_complexity_;
      break;
    case kVideoCodecVP9:
      settings = &vp9_complexity_;
      break;
    case kVideoCodecH264:
      settings = &h264_complexity_;
      break;
    case kVideoCodecH265:
      settings = &h265_complexity_;
      break;
    case kVideoCodecGeneric:
      // Not supported, just use the normal default value.
      return VideoCodecComplexity::kComplexityNormal;
  }

  return is_screenshare ? settings->screenshare : settings->camera;
}

}  // namespace webrtc
