// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "rtc_base/experiments/psnr_experiment.h"

#include "api/field_trials.h"
#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(PsnrExperimentTest, DisabledByDefault) {
  FieldTrials field_trials("");
  PsnrExperiment config(field_trials);
  EXPECT_FALSE(config.IsEnabled());
}

TEST(PsnrExperimentTest, Enabled) {
  FieldTrials field_trials("WebRTC-Video-CalculatePsnr/Enabled/");
  PsnrExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.SamplingInterval(), TimeDelta::Millis(1000));
}

TEST(PsnrExperimentTest, EnabledWithCustomSampling) {
  FieldTrials field_trials(
      "WebRTC-Video-CalculatePsnr/Enabled,sampling_interval:2500ms/");
  PsnrExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.SamplingInterval(), TimeDelta::Millis(2500));
}

TEST(PsnrExperimentTest, EnabledWithInvalidSampling) {
  FieldTrials field_trials(
      "WebRTC-Video-CalculatePsnr/Enabled,sampling_interval:0ms/");
  PsnrExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.SamplingInterval(), TimeDelta::Millis(1000));
}

TEST(PsnrExperimentTest, DisabledWithParams) {
  FieldTrials field_trials(
      "WebRTC-Video-CalculatePsnr/Disabled,sampling_interval:500ms/");
  PsnrExperiment config(field_trials);
  EXPECT_FALSE(config.IsEnabled());
}

TEST(PsnrExperimentTest, EnabledWithNegativeSampling) {
  FieldTrials field_trials(
      "WebRTC-Video-CalculatePsnr/Enabled,sampling_interval:-100ms/");
  PsnrExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.SamplingInterval(), TimeDelta::Millis(1000));
}

TEST(PsnrExperimentTest, ExplicitlyDisabled) {
  FieldTrials field_trials("WebRTC-Video-CalculatePsnr/Disabled/");
  PsnrExperiment config(field_trials);
  EXPECT_FALSE(config.IsEnabled());
}

}  // namespace
}  // namespace webrtc
