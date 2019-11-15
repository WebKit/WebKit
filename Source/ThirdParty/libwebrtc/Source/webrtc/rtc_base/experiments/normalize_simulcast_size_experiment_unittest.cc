/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/normalize_simulcast_size_experiment.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

TEST(NormalizeSimulcastSizeExperimentTest, GetExponent) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled-2/");
  EXPECT_EQ(2, NormalizeSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormalizeSimulcastSizeExperimentTest, GetExponentWithTwoParameters) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled-3-4/");
  EXPECT_EQ(3, NormalizeSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormalizeSimulcastSizeExperimentTest, GetExponentFailsIfNotEnabled) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Disabled/");
  EXPECT_FALSE(NormalizeSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormalizeSimulcastSizeExperimentTest,
     GetExponentFailsForInvalidFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled-invalid/");
  EXPECT_FALSE(NormalizeSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormalizeSimulcastSizeExperimentTest,
     GetExponentFailsForNegativeOutOfBoundValue) {
  // Supported range: [0, 5].
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled--1/");
  EXPECT_FALSE(NormalizeSimulcastSizeExperiment::GetBase2Exponent());
}

TEST(NormalizeSimulcastSizeExperimentTest,
     GetExponentFailsForPositiveOutOfBoundValue) {
  // Supported range: [0, 5].
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-NormalizeSimulcastResolution/Enabled-6/");
  EXPECT_FALSE(NormalizeSimulcastSizeExperiment::GetBase2Exponent());
}

}  // namespace webrtc
