/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/bandwidth_quality_scaler_settings.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(BandwidthQualityScalerSettingsTest, ValuesNotSetByDefault) {
  const auto settings = BandwidthQualityScalerSettings::ParseFromFieldTrials();
  EXPECT_FALSE(settings.BitrateStateUpdateInterval());
}

TEST(BandwidthQualityScalerSettingsTest, ParseBitrateStateUpdateInterval) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BandwidthQualityScalerSettings/"
      "bitrate_state_update_interval_s_:100/");
  EXPECT_EQ(100u, BandwidthQualityScalerSettings::ParseFromFieldTrials()
                      .BitrateStateUpdateInterval());
}

TEST(BandwidthQualityScalerSettingsTest, ParseAll) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BandwidthQualityScalerSettings/"
      "bitrate_state_update_interval_s_:100/");
  EXPECT_EQ(100u, BandwidthQualityScalerSettings::ParseFromFieldTrials()
                      .BitrateStateUpdateInterval());
}

TEST(BandwidthQualityScalerSettingsTest, DoesNotParseIncorrectValue) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-BandwidthQualityScalerSettings/"
      "bitrate_state_update_interval_s_:??/");
  const auto settings = BandwidthQualityScalerSettings::ParseFromFieldTrials();
  EXPECT_FALSE(settings.BitrateStateUpdateInterval());
}

}  // namespace
}  // namespace webrtc
