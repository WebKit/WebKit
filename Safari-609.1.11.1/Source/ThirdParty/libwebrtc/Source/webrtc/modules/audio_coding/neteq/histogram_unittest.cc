/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/histogram.h"

#include <cmath>

#include "test/gtest.h"

namespace webrtc {

TEST(HistogramTest, Initialization) {
  Histogram histogram(65, 32440);
  histogram.Reset();
  const auto& buckets = histogram.buckets();
  double sum = 0.0;
  for (size_t i = 0; i < buckets.size(); i++) {
    EXPECT_NEAR(ldexp(std::pow(0.5, static_cast<int>(i + 1)), 30), buckets[i],
                65537);
    // Tolerance 65537 in Q30 corresponds to a delta of approximately 0.00006.
    sum += buckets[i];
  }
  EXPECT_EQ(1 << 30, static_cast<int>(sum));  // Should be 1 in Q30.
}

TEST(HistogramTest, Add) {
  Histogram histogram(10, 32440);
  histogram.Reset();
  const std::vector<int> before = histogram.buckets();
  const int index = 5;
  histogram.Add(index);
  const std::vector<int> after = histogram.buckets();
  EXPECT_GT(after[index], before[index]);
  int sum = 0;
  for (int bucket : after) {
    sum += bucket;
  }
  EXPECT_EQ(1 << 30, sum);
}

TEST(HistogramTest, ForgetFactor) {
  Histogram histogram(10, 32440);
  histogram.Reset();
  const std::vector<int> before = histogram.buckets();
  const int index = 4;
  histogram.Add(index);
  const std::vector<int> after = histogram.buckets();
  for (int i = 0; i < histogram.NumBuckets(); ++i) {
    if (i != index) {
      EXPECT_LT(after[i], before[i]);
    }
  }
}

// Test if the histogram is scaled correctly if the bucket width is decreased.
TEST(HistogramTest, DownScale) {
  // Test a straightforward 60 to 20 change.
  std::vector<int> buckets = {12, 0, 0, 0, 0, 0};
  std::vector<int> expected_result = {4, 4, 4, 0, 0, 0};
  std::vector<int> stretched_buckets = Histogram::ScaleBuckets(buckets, 60, 20);
  EXPECT_EQ(stretched_buckets, expected_result);

  // Test an example where the last bin in the stretched histogram should
  // contain the sum of the elements that don't fit into the new histogram.
  buckets = {18, 15, 12, 9, 6, 3, 0};
  expected_result = {6, 6, 6, 5, 5, 5, 30};
  stretched_buckets = Histogram::ScaleBuckets(buckets, 60, 20);
  EXPECT_EQ(stretched_buckets, expected_result);

  // Test a 120 to 60 change.
  buckets = {18, 16, 14, 4, 0};
  expected_result = {9, 9, 8, 8, 18};
  stretched_buckets = Histogram::ScaleBuckets(buckets, 120, 60);
  EXPECT_EQ(stretched_buckets, expected_result);

  // Test a 120 to 20 change.
  buckets = {19, 12, 0, 0, 0, 0, 0, 0};
  expected_result = {3, 3, 3, 3, 3, 3, 2, 11};
  stretched_buckets = Histogram::ScaleBuckets(buckets, 120, 20);
  EXPECT_EQ(stretched_buckets, expected_result);

  // Test a 70 to 40 change.
  buckets = {13, 7, 5, 3, 1, 5, 12, 11, 3, 0, 0, 0};
  expected_result = {7, 5, 5, 3, 3, 2, 2, 1, 2, 2, 6, 22};
  stretched_buckets = Histogram::ScaleBuckets(buckets, 70, 40);
  EXPECT_EQ(stretched_buckets, expected_result);

  // Test a 30 to 20 change.
  buckets = {13, 7, 5, 3, 1, 5, 12, 11, 3, 0, 0, 0};
  expected_result = {8, 6, 6, 3, 2, 2, 1, 3, 3, 8, 7, 11};
  stretched_buckets = Histogram::ScaleBuckets(buckets, 30, 20);
  EXPECT_EQ(stretched_buckets, expected_result);
}

// Test if the histogram is scaled correctly if the bucket width is increased.
TEST(HistogramTest, UpScale) {
  // Test a 20 to 60 change.
  std::vector<int> buckets = {12, 11, 10, 3, 2, 1};
  std::vector<int> expected_result = {33, 6, 0, 0, 0, 0};
  std::vector<int> compressed_buckets =
      Histogram::ScaleBuckets(buckets, 20, 60);
  EXPECT_EQ(compressed_buckets, expected_result);

  // Test a 60 to 120 change.
  buckets = {18, 16, 14, 4, 1};
  expected_result = {34, 18, 1, 0, 0};
  compressed_buckets = Histogram::ScaleBuckets(buckets, 60, 120);
  EXPECT_EQ(compressed_buckets, expected_result);

  // Test a 20 to 120 change.
  buckets = {18, 12, 5, 4, 4, 3, 5, 1};
  expected_result = {46, 6, 0, 0, 0, 0, 0, 0};
  compressed_buckets = Histogram::ScaleBuckets(buckets, 20, 120);
  EXPECT_EQ(compressed_buckets, expected_result);

  // Test a 70 to 80 change.
  buckets = {13, 7, 5, 3, 1, 5, 12, 11, 3};
  expected_result = {11, 8, 6, 2, 5, 12, 13, 3, 0};
  compressed_buckets = Histogram::ScaleBuckets(buckets, 70, 80);
  EXPECT_EQ(compressed_buckets, expected_result);

  // Test a 50 to 110 change.
  buckets = {13, 7, 5, 3, 1, 5, 12, 11, 3};
  expected_result = {18, 8, 16, 16, 2, 0, 0, 0, 0};
  compressed_buckets = Histogram::ScaleBuckets(buckets, 50, 110);
  EXPECT_EQ(compressed_buckets, expected_result);
}

// Test if the histogram scaling function handles overflows correctly.
TEST(HistogramTest, OverflowTest) {
  // Test a upscale operation that can cause overflow.
  std::vector<int> buckets = {733544448, 0, 0, 0, 0, 0, 0,
                              340197376, 0, 0, 0, 0, 0, 0};
  std::vector<int> expected_result = {733544448, 340197376, 0, 0, 0, 0, 0,
                                      0,         0,         0, 0, 0, 0, 0};
  std::vector<int> scaled_buckets = Histogram::ScaleBuckets(buckets, 10, 60);
  EXPECT_EQ(scaled_buckets, expected_result);

  buckets = {655591163, 39962288, 360736736, 1930514, 4003853, 1782764,
             114119,    2072996,  0,         2149354, 0};
  expected_result = {1056290187, 7717131, 2187115, 2149354, 0, 0,
                     0,          0,       0,       0,       0};
  scaled_buckets = Histogram::ScaleBuckets(buckets, 20, 60);
  EXPECT_EQ(scaled_buckets, expected_result);

  // In this test case we will not be able to add everything to the final bin in
  // the scaled histogram. Check that the last bin doesn't overflow.
  buckets = {2000000000, 2000000000, 2000000000,
             2000000000, 2000000000, 2000000000};
  expected_result = {666666666, 666666666, 666666666,
                     666666667, 666666667, 2147483647};
  scaled_buckets = Histogram::ScaleBuckets(buckets, 60, 20);
  EXPECT_EQ(scaled_buckets, expected_result);

  // In this test case we will not be able to add enough to each of the bins,
  // so the values should be smeared out past the end of the normal range.
  buckets = {2000000000, 2000000000, 2000000000,
             2000000000, 2000000000, 2000000000};
  expected_result = {2147483647, 2147483647, 2147483647,
                     2147483647, 2147483647, 1262581765};
  scaled_buckets = Histogram::ScaleBuckets(buckets, 20, 60);
  EXPECT_EQ(scaled_buckets, expected_result);
}

TEST(HistogramTest, ReachSteadyStateForgetFactor) {
  static constexpr int kSteadyStateForgetFactor = (1 << 15) * 0.9993;
  Histogram histogram(100, kSteadyStateForgetFactor, 1.0);
  histogram.Reset();
  int n = (1 << 15) / ((1 << 15) - kSteadyStateForgetFactor);
  for (int i = 0; i < n; ++i) {
    histogram.Add(0);
  }
  EXPECT_EQ(histogram.forget_factor_for_testing(), kSteadyStateForgetFactor);
}

}  // namespace webrtc
