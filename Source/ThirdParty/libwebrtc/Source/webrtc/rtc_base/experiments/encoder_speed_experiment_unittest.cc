// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "rtc_base/experiments/encoder_speed_experiment.h"

#include "api/field_trials.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(EncoderSpeedExperimentTest, DisabledByDefault) {
  FieldTrials field_trials("");
  EncoderSpeedExperiment config(field_trials);
  EXPECT_FALSE(config.IsDynamicSpeedEnabled());
}

TEST(EncoderSpeedExperimentTest, DynamicSpeedEnabled) {
  FieldTrials field_trials("WebRTC-EncoderSpeed/dynamic_speed:true/");
  EncoderSpeedExperiment config(field_trials);
  EXPECT_TRUE(config.IsDynamicSpeedEnabled());
}

TEST(EncoderSpeedExperimentTest, DynamicSpeedExplicitlyDisabled) {
  FieldTrials field_trials("WebRTC-EncoderSpeed/dynamic_speed:false/");
  EncoderSpeedExperiment config(field_trials);
  EXPECT_FALSE(config.IsDynamicSpeedEnabled());
}

TEST(EncoderSpeedExperimentTest, DefaultComplexity) {
  FieldTrials field_trials("WebRTC-EncoderSpeed/dynamic_speed:true/");

  EncoderSpeedExperiment config(field_trials);

  EXPECT_EQ(config.GetComplexity(kVideoCodecVP8, /*is_screenshare=*/false),
            VideoCodecComplexity::kComplexityNormal);
  EXPECT_EQ(config.GetComplexity(kVideoCodecAV1, /*is_screenshare=*/true),
            VideoCodecComplexity::kComplexityNormal);
}

TEST(EncoderSpeedExperimentTest, PerCodecComplexity) {
  FieldTrials field_trials(
      "WebRTC-EncoderSpeed/"
      "dynamic_speed:true,av1_camera:high,av1_screenshare:low,vp8_camera:max/");

  EncoderSpeedExperiment config(field_trials);
  EXPECT_TRUE(config.IsDynamicSpeedEnabled());

  // AV1
  EXPECT_EQ(config.GetComplexity(kVideoCodecAV1, /*is_screenshare=*/false),
            VideoCodecComplexity::kComplexityHigh);
  EXPECT_EQ(config.GetComplexity(kVideoCodecAV1, /*is_screenshare=*/true),
            VideoCodecComplexity::kComplexityLow);

  // VP8
  EXPECT_EQ(config.GetComplexity(kVideoCodecVP8, /*is_screenshare=*/false),
            VideoCodecComplexity::kComplexityMax);
  EXPECT_EQ(config.GetComplexity(kVideoCodecVP8, /*is_screenshare=*/true),
            VideoCodecComplexity::kComplexityNormal);  // Default

  // VP9 (Not specified)
  EXPECT_EQ(config.GetComplexity(kVideoCodecVP9, /*is_screenshare=*/false),
            VideoCodecComplexity::kComplexityNormal);
  EXPECT_EQ(config.GetComplexity(kVideoCodecVP9, /*is_screenshare=*/true),
            VideoCodecComplexity::kComplexityNormal);
}

TEST(EncoderSpeedExperimentTest, PerCodecComplexityDynamicSpeedDisabled) {
  FieldTrials field_trials(
      "WebRTC-EncoderSpeed/"
      "dynamic_speed:false,av1_camera:high,av1_screenshare:low/");

  EncoderSpeedExperiment config(field_trials);
  EXPECT_FALSE(config.IsDynamicSpeedEnabled());

  // AV1
  EXPECT_EQ(config.GetComplexity(kVideoCodecAV1, /*is_screenshare=*/false),
            VideoCodecComplexity::kComplexityHigh);
  EXPECT_EQ(config.GetComplexity(kVideoCodecAV1, /*is_screenshare=*/true),
            VideoCodecComplexity::kComplexityLow);
}

TEST(EncoderSpeedExperimentTest, InvalidCodecComplexityValue) {
  FieldTrials field_trials(
      "WebRTC-EncoderSpeed/"
      "dynamic_speed:true,av1_camera:invalid,vp8_screenshare:max/");

  EncoderSpeedExperiment config(field_trials);
  EXPECT_TRUE(config.IsDynamicSpeedEnabled());
  EXPECT_EQ(config.GetComplexity(kVideoCodecAV1, /*is_screenshare=*/false),
            VideoCodecComplexity::kComplexityNormal);  // Invalid value
  EXPECT_EQ(config.GetComplexity(kVideoCodecVP8, /*is_screenshare=*/true),
            VideoCodecComplexity::kComplexityMax);
}

TEST(EncoderSpeedExperimentTest, InvalidDynamicSpeedValue) {
  FieldTrials field_trials("WebRTC-EncoderSpeed/dynamic_speed:invalid/");
  EncoderSpeedExperiment config(field_trials);
  EXPECT_FALSE(config.IsDynamicSpeedEnabled());  // Should default to false
}

}  // namespace

}  // namespace webrtc
