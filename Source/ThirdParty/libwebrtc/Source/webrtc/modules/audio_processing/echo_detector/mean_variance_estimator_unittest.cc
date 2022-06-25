
/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/echo_detector/mean_variance_estimator.h"

#include "test/gtest.h"

namespace webrtc {

TEST(MeanVarianceEstimatorTests, InsertTwoValues) {
  MeanVarianceEstimator test_estimator;
  // Insert two values.
  test_estimator.Update(3.f);
  test_estimator.Update(5.f);

  EXPECT_GT(test_estimator.mean(), 0.f);
  EXPECT_GT(test_estimator.std_deviation(), 0.f);
  // Test Clear method
  test_estimator.Clear();
  EXPECT_EQ(test_estimator.mean(), 0.f);
  EXPECT_EQ(test_estimator.std_deviation(), 0.f);
}

TEST(MeanVarianceEstimatorTests, InsertZeroes) {
  MeanVarianceEstimator test_estimator;
  // Insert the same value many times.
  for (size_t i = 0; i < 20000; i++) {
    test_estimator.Update(0.f);
  }
  EXPECT_EQ(test_estimator.mean(), 0.f);
  EXPECT_EQ(test_estimator.std_deviation(), 0.f);
}

TEST(MeanVarianceEstimatorTests, ConstantValueTest) {
  MeanVarianceEstimator test_estimator;
  for (size_t i = 0; i < 20000; i++) {
    test_estimator.Update(3.f);
  }
  // The mean should be close to three, and the standard deviation should be
  // close to zero.
  EXPECT_NEAR(3.0f, test_estimator.mean(), 0.01f);
  EXPECT_NEAR(0.0f, test_estimator.std_deviation(), 0.01f);
}

TEST(MeanVarianceEstimatorTests, AlternatingValueTest) {
  MeanVarianceEstimator test_estimator;
  for (size_t i = 0; i < 20000; i++) {
    test_estimator.Update(1.f);
    test_estimator.Update(-1.f);
  }
  // The mean should be close to zero, and the standard deviation should be
  // close to one.
  EXPECT_NEAR(0.0f, test_estimator.mean(), 0.01f);
  EXPECT_NEAR(1.0f, test_estimator.std_deviation(), 0.01f);
}

}  // namespace webrtc
