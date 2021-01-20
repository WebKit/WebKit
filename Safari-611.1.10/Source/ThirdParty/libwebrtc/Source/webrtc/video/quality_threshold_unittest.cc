/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/quality_threshold.h"

#include "test/gtest.h"

namespace webrtc {

TEST(QualityThresholdTest, BackAndForth) {
  const int kLowThreshold = 0;
  const int kHighThreshold = 1;
  const float kFraction = 0.75f;
  const int kMaxMeasurements = 10;

  QualityThreshold thresh(kLowThreshold, kHighThreshold, kFraction,
                          kMaxMeasurements);

  const int kNeededMeasurements =
      static_cast<int>(kFraction * kMaxMeasurements + 1);
  for (int i = 0; i < kNeededMeasurements; ++i) {
    EXPECT_FALSE(thresh.IsHigh());
    thresh.AddMeasurement(kLowThreshold);
  }
  ASSERT_TRUE(thresh.IsHigh());
  for (int i = 0; i < kNeededMeasurements; ++i) {
    EXPECT_FALSE(*thresh.IsHigh());
    thresh.AddMeasurement(kHighThreshold);
  }
  EXPECT_TRUE(*thresh.IsHigh());

  for (int i = 0; i < kNeededMeasurements; ++i) {
    EXPECT_TRUE(*thresh.IsHigh());
    thresh.AddMeasurement(kLowThreshold);
  }
  EXPECT_FALSE(*thresh.IsHigh());
}

TEST(QualityThresholdTest, Variance) {
  const int kLowThreshold = 0;
  const int kHighThreshold = 1;
  const float kFraction = 0.8f;
  const int kMaxMeasurements = 10;
  const double kMaxError = 0.01;

  // Previously randomly generated values...
  int values[] = {51, 79, 80, 56, 19, 20, 48, 57, 48, 25, 2, 25, 38, 37, 25};
  // ...with precomputed variances.
  double variances[] = {476.9, 687.6, 552, 336.4, 278.767, 265.167};

  QualityThreshold thresh(kLowThreshold, kHighThreshold, kFraction,
                          kMaxMeasurements);

  for (int i = 0; i < kMaxMeasurements; ++i) {
    EXPECT_FALSE(thresh.CalculateVariance());
    thresh.AddMeasurement(values[i]);
  }

  ASSERT_TRUE(thresh.CalculateVariance());
  EXPECT_NEAR(variances[0], *thresh.CalculateVariance(), kMaxError);
  for (unsigned int i = 1; i < sizeof(variances) / sizeof(double); ++i) {
    thresh.AddMeasurement(values[i + kMaxMeasurements - 1]);
    EXPECT_NEAR(variances[i], *thresh.CalculateVariance(), kMaxError);
  }

  for (int i = 0; i < kMaxMeasurements; ++i) {
    thresh.AddMeasurement(42);
  }
  EXPECT_NEAR(0, *thresh.CalculateVariance(), kMaxError);
}

TEST(QualityThresholdTest, BetweenThresholds) {
  const int kLowThreshold = 0;
  const int kHighThreshold = 2;
  const float kFraction = 0.6f;
  const int kMaxMeasurements = 10;

  const int kBetweenThresholds = (kLowThreshold + kHighThreshold) / 2;

  QualityThreshold thresh(kLowThreshold, kHighThreshold, kFraction,
                          kMaxMeasurements);

  for (int i = 0; i < 2 * kMaxMeasurements; ++i) {
    EXPECT_FALSE(thresh.IsHigh());
    thresh.AddMeasurement(kBetweenThresholds);
  }
  EXPECT_FALSE(thresh.IsHigh());
}

TEST(QualityThresholdTest, FractionHigh) {
  const int kLowThreshold = 0;
  const int kHighThreshold = 2;
  const float kFraction = 0.75f;
  const int kMaxMeasurements = 10;

  const int kBetweenThresholds = (kLowThreshold + kHighThreshold) / 2;
  const int kNeededMeasurements =
      static_cast<int>(kFraction * kMaxMeasurements + 1);

  QualityThreshold thresh(kLowThreshold, kHighThreshold, kFraction,
                          kMaxMeasurements);

  for (int i = 0; i < kMaxMeasurements; ++i) {
    EXPECT_FALSE(thresh.FractionHigh(1));
    thresh.AddMeasurement(kBetweenThresholds);
  }

  for (int i = 0; i < kNeededMeasurements; i++) {
    EXPECT_FALSE(thresh.FractionHigh(1));
    thresh.AddMeasurement(kHighThreshold);
  }
  EXPECT_FALSE(thresh.FractionHigh(2));
  ASSERT_TRUE(thresh.FractionHigh(1));
  EXPECT_NEAR(*thresh.FractionHigh(1), 1, 0.001);

  for (int i = 0; i < kNeededMeasurements; i++) {
    EXPECT_NEAR(*thresh.FractionHigh(1), 1, 0.001);
    thresh.AddMeasurement(kLowThreshold);
  }
  EXPECT_NEAR(
      *thresh.FractionHigh(1),
      static_cast<double>(kNeededMeasurements) / (kNeededMeasurements + 1),
      0.001);
}

}  // namespace webrtc
