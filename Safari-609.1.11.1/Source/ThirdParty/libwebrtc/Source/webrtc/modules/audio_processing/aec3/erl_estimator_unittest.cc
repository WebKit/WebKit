/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erl_estimator.h"

#include "test/gtest.h"

namespace webrtc {

namespace {

void VerifyErl(const std::array<float, kFftLengthBy2Plus1>& erl,
               float erl_time_domain,
               float reference) {
  std::for_each(erl.begin(), erl.end(),
                [reference](float a) { EXPECT_NEAR(reference, a, 0.001); });
  EXPECT_NEAR(reference, erl_time_domain, 0.001);
}

}  // namespace

// Verifies that the correct ERL estimates are achieved.
TEST(ErlEstimator, Estimates) {
  std::array<float, kFftLengthBy2Plus1> X2;
  std::array<float, kFftLengthBy2Plus1> Y2;

  ErlEstimator estimator(0);

  // Verifies that the ERL estimate is properly reduced to lower values.
  X2.fill(500 * 1000.f * 1000.f);
  Y2.fill(10 * X2[0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(true, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 10.f);

  // Verifies that the ERL is not immediately increased when the ERL in the data
  // increases.
  Y2.fill(10000 * X2[0]);
  for (size_t k = 0; k < 998; ++k) {
    estimator.Update(true, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 10.f);

  // Verifies that the rate of increase is 3 dB.
  estimator.Update(true, X2, Y2);
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 20.f);

  // Verifies that the maximum ERL is achieved when there are no low RLE
  // estimates.
  for (size_t k = 0; k < 1000; ++k) {
    estimator.Update(true, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 1000.f);

  // Verifies that the ERL estimate is is not updated for low-level signals
  X2.fill(1000.f * 1000.f);
  Y2.fill(10 * X2[0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(true, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 1000.f);
}

}  // namespace webrtc
