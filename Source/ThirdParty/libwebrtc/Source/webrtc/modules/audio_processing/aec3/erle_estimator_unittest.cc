/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/erle_estimator.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

void VerifyErle(const std::array<float, kFftLengthBy2Plus1>& erle,
                float reference) {
  std::for_each(erle.begin(), erle.end(),
                [reference](float a) { EXPECT_NEAR(reference, a, 0.001); });
}

}  // namespace

// Verifies that the correct ERLE estimates are achieved.
TEST(ErleEstimator, Estimates) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> Y2;

  ErleEstimator estimator;

  // Verifies that the ERLE estimate is properley increased to higher values.
  X2.fill(500 * 1000.f * 1000.f);
  E2.fill(1000.f * 1000.f);
  Y2.fill(10 * E2[0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), 8.f);

  // Verifies that the ERLE is not immediately decreased when the ERLE in the
  // data decreases.
  Y2.fill(0.1f * E2[0]);
  for (size_t k = 0; k < 98; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), 8.f);

  // Verifies that the minimum ERLE is eventually achieved.
  for (size_t k = 0; k < 1000; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), 1.f);

  // Verifies that the ERLE estimate is is not updated for low-level render
  // signals.
  X2.fill(1000.f * 1000.f);
  Y2.fill(10 * E2[0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(X2, Y2, E2);
  }
  VerifyErle(estimator.Erle(), 1.f);
}
}  // namespace webrtc
