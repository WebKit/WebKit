/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/cpu_speed_experiment.h"

#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {

TEST(CpuSpeedExperimentTest, GetConfigsFailsIfNotEnabled) {
  EXPECT_FALSE(CpuSpeedExperiment::GetConfigs());
}

TEST(CpuSpeedExperimentTest, GetConfigsFailsForTooFewParameters) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,-1,2000,-10,3000/");
  EXPECT_FALSE(CpuSpeedExperiment::GetConfigs());
}

TEST(CpuSpeedExperimentTest, GetConfigs) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,-1,2000,-10,3000,-16/");

  const absl::optional<std::vector<CpuSpeedExperiment::Config>> kConfigs =
      CpuSpeedExperiment::GetConfigs();
  ASSERT_TRUE(kConfigs);
  EXPECT_THAT(*kConfigs,
              ::testing::ElementsAre(CpuSpeedExperiment::Config{1000, -1},
                                     CpuSpeedExperiment::Config{2000, -10},
                                     CpuSpeedExperiment::Config{3000, -16}));
}

TEST(CpuSpeedExperimentTest, GetValue) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,-5,2000,-10,3000,-12/");

  const absl::optional<std::vector<CpuSpeedExperiment::Config>> kConfigs =
      CpuSpeedExperiment::GetConfigs();
  ASSERT_TRUE(kConfigs);
  ASSERT_EQ(3u, (*kConfigs).size());
  EXPECT_EQ(-5, CpuSpeedExperiment::GetValue(1, *kConfigs));
  EXPECT_EQ(-5, CpuSpeedExperiment::GetValue(1000, *kConfigs));
  EXPECT_EQ(-10, CpuSpeedExperiment::GetValue(1000 + 1, *kConfigs));
  EXPECT_EQ(-10, CpuSpeedExperiment::GetValue(2000, *kConfigs));
  EXPECT_EQ(-12, CpuSpeedExperiment::GetValue(2000 + 1, *kConfigs));
  EXPECT_EQ(-12, CpuSpeedExperiment::GetValue(3000, *kConfigs));
  EXPECT_EQ(-16, CpuSpeedExperiment::GetValue(3000 + 1, *kConfigs));
}

TEST(CpuSpeedExperimentTest, GetConfigsFailsForTooSmallValue) {
  // Supported range: [-16, -1].
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,-1,2000,-10,3000,-17/");
  EXPECT_FALSE(CpuSpeedExperiment::GetConfigs());
}

TEST(CpuSpeedExperimentTest, GetConfigsFailsForTooLargeValue) {
  // Supported range: [-16, -1].
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,0,2000,-10,3000,-16/");
  EXPECT_FALSE(CpuSpeedExperiment::GetConfigs());
}

TEST(CpuSpeedExperimentTest, GetConfigsFailsIfPixelsDecreasing) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,-5,999,-10,3000,-16/");
  EXPECT_FALSE(CpuSpeedExperiment::GetConfigs());
}

TEST(CpuSpeedExperimentTest, GetConfigsFailsIfCpuSpeedIncreasing) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-VP8-CpuSpeed-Arm/Enabled-1000,-5,2000,-4,3000,-16/");
  EXPECT_FALSE(CpuSpeedExperiment::GetConfigs());
}

}  // namespace webrtc
