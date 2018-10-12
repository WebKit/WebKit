/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/rtt_mult_experiment.h"
#include "rtc_base/gunit.h"
#include "test/field_trial.h"

namespace webrtc {

TEST(RttMultExperimentTest, RttMultDisabledByDefault) {
  EXPECT_FALSE(RttMultExperiment::RttMultEnabled());
}

TEST(RttMultExperimentTest, RttMultEnabledByFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled-0.25/");
  EXPECT_TRUE(RttMultExperiment::RttMultEnabled());
}

TEST(RttMultExperimentTest, RttMultTestValue) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled-0.25/");
  EXPECT_EQ(0.25, RttMultExperiment::GetRttMultValue());
}

TEST(RttMultExperimentTest, RttMultTestMalformedEnabled) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enable-0.25/");
  EXPECT_FALSE(RttMultExperiment::RttMultEnabled());
}

TEST(RttMultExperimentTest, RttMultTestValueOutOfBoundsPositive) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled-1.5/");
  EXPECT_EQ(1.0, RttMultExperiment::GetRttMultValue());
}

TEST(RttMultExperimentTest, RttMultTestValueOutOfBoundsNegative) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled--0.5/");
  EXPECT_EQ(0.0, RttMultExperiment::GetRttMultValue());
}

TEST(RttMultExperimentTest, RttMultTestMalformedValue) {
  webrtc::test::ScopedFieldTrials field_trials("WebRTC-RttMult/Enabled-0.2a5/");
  EXPECT_NE(0.25, RttMultExperiment::GetRttMultValue());
}

}  // namespace webrtc
