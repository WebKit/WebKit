// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cmath>

#include "avif/internal.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Converts a double value to a fraction, and checks that the difference
// between numerator/denominator and v is below relative_tolerance.
void TestRoundTrip(double v, double relative_tolerance) {
  // Unsigned.
  if (v >= 0) {
    uint32_t numerator, denominator;
    ASSERT_TRUE(avifDoubleToUnsignedFraction(v, &numerator, &denominator)) << v;
    const double reconstructed = (double)numerator / denominator;
    const double tolerance = v * relative_tolerance;
    EXPECT_NEAR(reconstructed, v, tolerance)
        << "numerator " << (double)numerator << " denominator "
        << (double)denominator;
  }

  // Signed.
  if (v <= INT32_MAX) {
    for (double multiplier : {1.0, -1.0}) {
      double v2 = v * multiplier;
      int32_t numerator;
      uint32_t denominator;

      ASSERT_TRUE(avifDoubleToSignedFraction(v2, &numerator, &denominator))
          << v2;
      const double reconstructed = (double)numerator / denominator;
      const double tolerance = v * relative_tolerance;
      EXPECT_NEAR(reconstructed, v2, tolerance)
          << "numerator " << (double)numerator << " denominator "
          << (double)denominator;
    }
  }
}

constexpr double kLotsOfDecimals = 0.14159265358979323846;

TEST(ToFractionTest, RoundTrip) {
  // Whole numbers and simple fractions should match perfectly.
  constexpr double kPerfectTolerance = 0.0;
  TestRoundTrip(0.0, kPerfectTolerance);
  TestRoundTrip(1.0, kPerfectTolerance);
  TestRoundTrip(42.0, kPerfectTolerance);
  TestRoundTrip(102356.0, kPerfectTolerance);
  TestRoundTrip(102356456.0f, kPerfectTolerance);
  TestRoundTrip(UINT32_MAX / 2.0, kPerfectTolerance);
  TestRoundTrip((double)UINT32_MAX - 1.0, kPerfectTolerance);
  TestRoundTrip((double)UINT32_MAX, kPerfectTolerance);
  TestRoundTrip(0.123, kPerfectTolerance);
  TestRoundTrip(1.0 / 3.0, kPerfectTolerance);
  TestRoundTrip(1.0 / 4.0, kPerfectTolerance);
  TestRoundTrip(3.0 / 23.0, kPerfectTolerance);
  TestRoundTrip(1253456.456, kPerfectTolerance);
  TestRoundTrip(8598533.9, kPerfectTolerance);

  // Numbers with a lot of decimals or very large/small can show a small
  // error.
  constexpr double kSmallTolerance = 1e-9;
  TestRoundTrip(0.0123456, kSmallTolerance);
  TestRoundTrip(3 + kLotsOfDecimals, kSmallTolerance);
  TestRoundTrip(sqrt(2.0), kSmallTolerance);
  TestRoundTrip(exp(1.0), kSmallTolerance);
  TestRoundTrip(exp(10.0), kSmallTolerance);
  TestRoundTrip(exp(15.0), kSmallTolerance);
  // The golden ratio, the irrational number that is the "most difficult" to
  // approximate rationally according to Wikipedia.
  const double kGoldenRatio = (1.0 + std::sqrt(5.0)) / 2.0;
  TestRoundTrip(kGoldenRatio, kSmallTolerance);  // Golden ratio.
  TestRoundTrip(((double)UINT32_MAX) - 0.5, kSmallTolerance);
  // Note that values smaller than this might have a larger relative error
  // (e.g. 1.0e-10).
  TestRoundTrip(4.2e-10, kSmallTolerance);
}

// Tests the max difference between the fraction-ified value and the original
// value, for a subset of values between 0.0 and UINT32_MAX.
TEST(ToFractionTest, MaxDifference) {
  double max_error = 0;
  double max_error_v = 0;
  double max_relative_error = 0;
  double max_relative_error_v = 0;
  for (uint64_t i = 0; i < UINT32_MAX; i += 1000) {
    const double v = i + kLotsOfDecimals;
    uint32_t numerator, denominator;
    ASSERT_TRUE(avifDoubleToUnsignedFraction(v, &numerator, &denominator)) << v;
    const double reconstructed = (double)numerator / denominator;
    const double error = abs(reconstructed - v);
    const double relative_error = error / v;
    if (error > max_error) {
      max_error = error;
      max_error_v = v;
    }
    if (relative_error > max_relative_error) {
      max_relative_error = relative_error;
      max_relative_error_v = v;
    }
  }
  EXPECT_LE(max_error, 0.5f) << max_error_v;
  EXPECT_LT(max_relative_error, 1e-9) << max_relative_error_v;
}

// Tests the max difference between the fraction-ified value and the original
// value, for a subset of values between 0 and 1.0/UINT32_MAX.
TEST(ToFractionTest, MaxDifferenceSmall) {
  double max_error = 0;
  double max_error_v = 0;
  double max_relative_error = 0;
  double max_relative_error_v = 0;
  for (uint64_t i = 1; i < UINT32_MAX; i += 1000) {
    const double v = 1.0 / (i + kLotsOfDecimals);
    uint32_t numerator, denominator;
    ASSERT_TRUE(avifDoubleToUnsignedFraction(v, &numerator, &denominator)) << v;
    const double reconstructed = (double)numerator / denominator;
    const double error = abs(reconstructed - v);
    const double relative_error = error / v;
    if (error > max_error) {
      max_error = error;
      max_error_v = v;
    }
    if (relative_error > max_relative_error) {
      max_relative_error = relative_error;
      max_relative_error_v = v;
    }
  }
  EXPECT_LE(max_error, 1e-10) << max_error_v;
  EXPECT_LT(max_relative_error, 1e-5) << max_relative_error_v;
}

TEST(ToFractionTest, BadValues) {
  uint32_t numerator, denominator;
  // Negative value.
  EXPECT_FALSE(avifDoubleToUnsignedFraction(-0.1, &numerator, &denominator));
  // Too large.
  EXPECT_FALSE(avifDoubleToUnsignedFraction(((double)UINT32_MAX) + 1.0,
                                            &numerator, &denominator));
}

}  // namespace
}  // namespace avif
