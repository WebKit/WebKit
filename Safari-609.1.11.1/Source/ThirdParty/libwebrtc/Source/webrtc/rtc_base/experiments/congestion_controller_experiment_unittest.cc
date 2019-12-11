/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/congestion_controller_experiment.h"
#include "rtc_base/gunit.h"
#include "test/field_trial.h"

namespace webrtc {
TEST(CongestionControllerExperimentTest, BbrDisabledByDefault) {
  webrtc::test::ScopedFieldTrials field_trials("");
  EXPECT_FALSE(CongestionControllerExperiment::BbrControllerEnabled());
}

TEST(CongestionControllerExperimentTest, BbrEnabledByFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-BweCongestionController/Enabled,BBR/");
  EXPECT_TRUE(CongestionControllerExperiment::BbrControllerEnabled());
}
}  // namespace webrtc
