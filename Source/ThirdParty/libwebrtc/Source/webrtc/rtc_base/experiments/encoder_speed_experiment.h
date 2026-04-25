// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef RTC_BASE_EXPERIMENTS_ENCODER_SPEED_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_ENCODER_SPEED_EXPERIMENT_H_

#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

class EncoderSpeedExperiment {
 public:
  explicit EncoderSpeedExperiment(const FieldTrialsView& field_trials);

  bool IsDynamicSpeedEnabled() const;
  VideoCodecComplexity GetComplexity(VideoCodecType codec_type,
                                     bool is_screenshare) const;

 private:
  struct ComplexitySettings {
    VideoCodecComplexity camera = VideoCodecComplexity::kComplexityNormal;
    VideoCodecComplexity screenshare = VideoCodecComplexity::kComplexityNormal;
  };

  void ParseCodecSettings(absl::string_view codec_name,
                          absl::string_view trial_string,
                          ComplexitySettings& settings);

  bool dynamic_speed_enabled_ = false;
  ComplexitySettings av1_complexity_;
  ComplexitySettings vp8_complexity_;
  ComplexitySettings vp9_complexity_;
  ComplexitySettings h264_complexity_;
  ComplexitySettings h265_complexity_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_ENCODER_SPEED_EXPERIMENT_H_
