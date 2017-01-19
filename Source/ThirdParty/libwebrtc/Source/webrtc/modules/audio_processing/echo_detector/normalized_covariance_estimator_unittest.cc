
/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/echo_detector/normalized_covariance_estimator.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(NormalizedCovarianceEstimatorTests, IdenticalSignalTest) {
  NormalizedCovarianceEstimator test_estimator;
  for (size_t i = 0; i < 10000; i++) {
    test_estimator.Update(1.f, 0.f, 1.f, 1.f, 0.f, 1.f);
    test_estimator.Update(-1.f, 0.f, 1.f, -1.f, 0.f, 1.f);
  }
  // A normalized covariance value close to 1 is expected.
  EXPECT_NEAR(1.f, test_estimator.normalized_cross_correlation(), 0.01f);
  test_estimator.Clear();
  EXPECT_EQ(0.f, test_estimator.normalized_cross_correlation());
}

TEST(NormalizedCovarianceEstimatorTests, OppositeSignalTest) {
  NormalizedCovarianceEstimator test_estimator;
  // Insert the same value many times.
  for (size_t i = 0; i < 10000; i++) {
    test_estimator.Update(1.f, 0.f, 1.f, -1.f, 0.f, 1.f);
    test_estimator.Update(-1.f, 0.f, 1.f, 1.f, 0.f, 1.f);
  }
  // A normalized covariance value close to -1 is expected.
  EXPECT_NEAR(-1.f, test_estimator.normalized_cross_correlation(), 0.01f);
}

}  // namespace webrtc
