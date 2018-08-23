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

#include "rtc_base/gunit.h"
#include "test/field_trial.h"

namespace webrtc {
namespace {
void ExpectEqualSettings(QualityScalingExperiment::Settings a,
                         QualityScalingExperiment::Settings b) {
  EXPECT_EQ(a.vp8_low, b.vp8_low);
  EXPECT_EQ(a.vp8_high, b.vp8_high);
  EXPECT_EQ(a.vp9_low, b.vp9_low);
  EXPECT_EQ(a.vp9_high, b.vp9_high);
  EXPECT_EQ(a.h264_low, b.h264_low);
  EXPECT_EQ(a.h264_high, b.h264_high);
  EXPECT_EQ(a.generic_low, b.generic_low);
  EXPECT_EQ(a.generic_high, b.generic_high);
  EXPECT_EQ(a.alpha_high, b.alpha_high);
  EXPECT_EQ(a.alpha_low, b.alpha_low);
  EXPECT_EQ(a.drop, b.drop);
}

void ExpectEqualConfig(QualityScalingExperiment::Config a,
                       QualityScalingExperiment::Config b) {
  EXPECT_EQ(a.alpha_high, b.alpha_high);
  EXPECT_EQ(a.alpha_low, b.alpha_low);
  EXPECT_EQ(a.use_all_drop_reasons, b.use_all_drop_reasons);
}
}  // namespace

TEST(QualityScalingExperimentTest, DisabledWithoutFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials("");
  EXPECT_FALSE(QualityScalingExperiment::Enabled());
}

TEST(QualityScalingExperimentTest, EnabledWithFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled/");
  EXPECT_TRUE(QualityScalingExperiment::Enabled());
}

TEST(QualityScalingExperimentTest, ParseSettings) {
  const QualityScalingExperiment::Settings kExpected = {1, 2, 3,    4,     5, 6,
                                                        7, 8, 0.9f, 0.99f, 1};
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.9,0.99,1/");
  const auto settings = QualityScalingExperiment::ParseSettings();
  EXPECT_TRUE(settings);
  ExpectEqualSettings(kExpected, *settings);
}

TEST(QualityScalingExperimentTest, ParseSettingsFailsWithoutFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials("");
  EXPECT_FALSE(QualityScalingExperiment::ParseSettings());
}

TEST(QualityScalingExperimentTest, ParseSettingsFailsWithInvalidFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-invalid/");
  EXPECT_FALSE(QualityScalingExperiment::ParseSettings());
}

TEST(QualityScalingExperimentTest, GetConfig) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.9,0.99,0/");
  const auto config = QualityScalingExperiment::GetConfig();
  EXPECT_EQ(0.9f, config.alpha_high);
  EXPECT_EQ(0.99f, config.alpha_low);
  EXPECT_FALSE(config.use_all_drop_reasons);
}

TEST(QualityScalingExperimentTest, GetsDefaultConfigForInvalidFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-invalid/");
  const auto config = QualityScalingExperiment::GetConfig();
  ExpectEqualConfig(config, QualityScalingExperiment::Config());
}

TEST(QualityScalingExperimentTest, GetsDefaultAlphaForInvalidValue) {
  QualityScalingExperiment::Config expected_config;
  expected_config.use_all_drop_reasons = true;
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.99,0.9,1/");
  const auto config = QualityScalingExperiment::GetConfig();
  ExpectEqualConfig(config, expected_config);
}

TEST(QualityScalingExperimentTest, GetVp8Thresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP8);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(1, thresholds->low);
  EXPECT_EQ(2, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidVp8Value) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-0,0,3,4,5,6,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP8);
  EXPECT_FALSE(thresholds);
}

TEST(QualityScalingExperimentTest, GetVp9Thresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP9);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(3, thresholds->low);
  EXPECT_EQ(4, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidVp9Value) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,0,0,5,6,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP9);
  EXPECT_FALSE(thresholds);
}

TEST(QualityScalingExperimentTest, GetH264Thresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecH264);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(5, thresholds->low);
  EXPECT_EQ(6, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidH264Value) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,0,0,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecH264);
  EXPECT_FALSE(thresholds);
}

TEST(QualityScalingExperimentTest, GetGenericThresholds) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,0,0,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecGeneric);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(7, thresholds->low);
  EXPECT_EQ(8, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidGenericValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecGeneric);
  EXPECT_FALSE(thresholds);
}
}  // namespace webrtc
