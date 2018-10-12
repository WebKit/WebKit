/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr int kLowFrequencyLimit = kFftLengthBy2 / 2;
constexpr float kMaxErleLf = 8.f;
constexpr float kMaxErleHf = 1.5f;
constexpr float kMinErle = 1.0f;
constexpr float kTrueErle = 10.f;
constexpr float kTrueErleOnsets = 1.0f;

void VerifyErleBands(rtc::ArrayView<const float> erle,
                     float reference_lf,
                     float reference_hf) {
  std::for_each(
      erle.begin(), erle.begin() + kLowFrequencyLimit,
      [reference_lf](float a) { EXPECT_NEAR(reference_lf, a, 0.001); });
  std::for_each(
      erle.begin() + kLowFrequencyLimit, erle.end(),
      [reference_hf](float a) { EXPECT_NEAR(reference_hf, a, 0.001); });
}

void VerifyErle(rtc::ArrayView<const float> erle,
                float erle_time_domain,
                float reference_lf,
                float reference_hf) {
  VerifyErleBands(erle, reference_lf, reference_hf);
  EXPECT_NEAR(reference_lf, erle_time_domain, 0.5);
}

void FormFarendFrame(std::array<float, kFftLengthBy2Plus1>* X2,
                     std::array<float, kFftLengthBy2Plus1>* E2,
                     std::array<float, kFftLengthBy2Plus1>* Y2,
                     float erle) {
  X2->fill(500 * 1000.f * 1000.f);
  E2->fill(1000.f * 1000.f);
  Y2->fill(erle * (*E2)[0]);
}

void FormNearendFrame(std::array<float, kFftLengthBy2Plus1>* X2,
                      std::array<float, kFftLengthBy2Plus1>* E2,
                      std::array<float, kFftLengthBy2Plus1>* Y2) {
  X2->fill(0.f);
  Y2->fill(500.f * 1000.f * 1000.f);
  E2->fill((*Y2)[0]);
}

}  // namespace

TEST(ErleEstimator, VerifyErleIncreaseAndHold) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> Y2;

  ErleEstimator estimator(0, kMinErle, kMaxErleLf, kMaxErleHf);

  // Verifies that the ERLE estimate is properly increased to higher values.
  FormFarendFrame(&X2, &E2, &Y2, kTrueErle);

  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(X2, Y2, E2, true, true);
  }
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             kMaxErleLf, kMaxErleHf);

  FormNearendFrame(&X2, &E2, &Y2);
  // Verifies that the ERLE is not immediately decreased during nearend
  // activity.
  for (size_t k = 0; k < 50; ++k) {
    estimator.Update(X2, Y2, E2, true, true);
  }
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             kMaxErleLf, kMaxErleHf);
}

TEST(ErleEstimator, VerifyErleTrackingOnOnsets) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> Y2;

  ErleEstimator estimator(0, kMinErle, kMaxErleLf, kMaxErleHf);

  for (size_t burst = 0; burst < 20; ++burst) {
    FormFarendFrame(&X2, &E2, &Y2, kTrueErleOnsets);
    for (size_t k = 0; k < 10; ++k) {
      estimator.Update(X2, Y2, E2, true, true);
    }
    FormFarendFrame(&X2, &E2, &Y2, kTrueErle);
    for (size_t k = 0; k < 200; ++k) {
      estimator.Update(X2, Y2, E2, true, true);
    }
    FormNearendFrame(&X2, &E2, &Y2);
    for (size_t k = 0; k < 300; ++k) {
      estimator.Update(X2, Y2, E2, true, true);
    }
  }
  VerifyErleBands(estimator.ErleOnsets(), kMinErle, kMinErle);
  FormNearendFrame(&X2, &E2, &Y2);
  for (size_t k = 0; k < 1000; k++) {
    estimator.Update(X2, Y2, E2, true, true);
  }
  // Verifies that during ne activity, Erle converges to the Erle for onsets.
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             kMinErle, kMinErle);
}

}  // namespace webrtc
