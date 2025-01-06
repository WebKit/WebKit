/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/stable_target_rate_experiment.h"

#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {

using test::ExplicitKeyValueConfig;

TEST(StableBweExperimentTest, Default) {
  ExplicitKeyValueConfig field_trials("");
  StableTargetRateExperiment config(field_trials);
  EXPECT_FALSE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.2);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.35);
}

TEST(StableBweExperimentTest, EnabledNoHysteresis) {
  ExplicitKeyValueConfig field_trials("WebRTC-StableTargetRate/enabled:true/");

  StableTargetRateExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.2);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.35);
}

TEST(StableBweExperimentTest, EnabledWithHysteresis) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-StableTargetRate/"
      "enabled:true,"
      "video_hysteresis_factor:1.1,"
      "screenshare_hysteresis_factor:1.2/");

  StableTargetRateExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.1);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.2);
}

TEST(StableBweExperimentTest, HysteresisOverrideVideoRateHystersis) {
  ExplicitKeyValueConfig field_trials(
      "WebRTC-StableTargetRate/"
      "enabled:true,"
      "video_hysteresis_factor:1.1,"
      "screenshare_hysteresis_factor:1.2/"
      "WebRTC-VideoRateControl/video_hysteresis:1.3,"
      "screenshare_hysteresis:1.4/");

  StableTargetRateExperiment config(field_trials);
  EXPECT_TRUE(config.IsEnabled());
  EXPECT_EQ(config.GetVideoHysteresisFactor(), 1.1);
  EXPECT_EQ(config.GetScreenshareHysteresisFactor(), 1.2);
}

}  // namespace webrtc
