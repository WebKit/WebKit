// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cmath>

#include "avif/internal.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

constexpr int kMaxTransferCharacteristic = 18;

// Thresholds in the transfer curve formulas.
// Add a little tolerance for test to pass on MinGW32.
constexpr float kTransferLog100Threshold = 0.01f + 1e-7f;
constexpr float kTransferLog100Sqrt10Threshold = 0.00316227766f;

TEST(TransferCharacteristicsTest, RoundTrip) {
  for (int tc_idx = 0; tc_idx <= kMaxTransferCharacteristic; ++tc_idx) {
    const avifTransferCharacteristics tc = (avifTransferCharacteristics)tc_idx;
    SCOPED_TRACE("transfer characteristics: " + std::to_string(tc));

    const avifTransferFunction to_linear =
        avifTransferCharacteristicsGetGammaToLinearFunction(tc);
    const avifTransferFunction to_gamma =
        avifTransferCharacteristicsGetLinearToGammaFunction(tc);

    constexpr int kSteps = 1000;
    float min_linear = std::numeric_limits<float>::max();
    float max_linear = 0.0f;
    for (int j = 0; j <= kSteps; ++j) {
      const float v = static_cast<float>(j) / kSteps;

      float epsilon = 0.0001f;
      // Non bijective part of some transfer functions.
      if (tc == AVIF_TRANSFER_CHARACTERISTICS_LOG100 &&
          v <= kTransferLog100Threshold) {
        epsilon = kTransferLog100Threshold / 2.0f;
      } else if (tc == AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10 &&
                 v <= kTransferLog100Sqrt10Threshold) {
        epsilon = kTransferLog100Sqrt10Threshold / 2.0f;
      }

      // Check round trips.
      ASSERT_NEAR(to_linear(to_gamma(v)), v, epsilon);
      ASSERT_NEAR(to_gamma(to_linear(v)), v, epsilon);

      const float linear = to_linear(v);
      if (linear > max_linear) max_linear = linear;
      if (linear < min_linear) min_linear = linear;
    }

    if (tc == AVIF_TRANSFER_CHARACTERISTICS_LOG100) {
      EXPECT_NEAR(min_linear, kTransferLog100Threshold / 2.0f, 1e-7f);
    } else if (tc == AVIF_TRANSFER_CHARACTERISTICS_LOG100_SQRT10) {
      EXPECT_EQ(min_linear, kTransferLog100Sqrt10Threshold / 2.0f);
    } else {
      EXPECT_EQ(min_linear, 0.0f);
    }

    if (tc == AVIF_TRANSFER_CHARACTERISTICS_PQ) {
      EXPECT_NEAR(max_linear, 10000.0f / 203.0f,
                  0.00001);  // PQ max extended SDR value.
    } else if (tc == AVIF_TRANSFER_CHARACTERISTICS_HLG) {
      EXPECT_NEAR(max_linear, 1000.0f / 203.0f,
                  0.00001);  // HLG max extended SDR value.
    } else if (tc == AVIF_TRANSFER_CHARACTERISTICS_SMPTE428) {
      // See formula in Table 3 of ITU-T H.273.
      EXPECT_NEAR(max_linear, 52.37f / 48.0f, 0.00001f);
    } else {
      EXPECT_EQ(max_linear, 1.0f);
    }
  }
}

// Check that the liner->gamma function has the right shape, i.e. it's mostly
// above the y=x diagonal.
// This detects bugs where the linear->gamma and
// gamma->linear implementations are swapped.
TEST(TransferCharacteristicsTest, ToGammaHasCorrectShape) {
  for (int tc_idx = 0; tc_idx <= kMaxTransferCharacteristic; ++tc_idx) {
    const avifTransferCharacteristics tc = (avifTransferCharacteristics)tc_idx;
    SCOPED_TRACE("transfer characteristics: " + std::to_string(tc));

    const avifTransferFunction to_gamma =
        avifTransferCharacteristicsGetLinearToGammaFunction(tc);

    constexpr int kSteps = 20;
    for (int j = 0; j <= kSteps; ++j) {
      const float linear = static_cast<float>(j) / kSteps;

      float extended_sdr_scaled = linear;
      if (tc == AVIF_TRANSFER_CHARACTERISTICS_PQ) {
        // Scale to the whole range.
        extended_sdr_scaled *= 10000.0f / 203.0f;
      } else if (tc == AVIF_TRANSFER_CHARACTERISTICS_HLG) {
        extended_sdr_scaled *= 1000.0f / 203.0f;
      }

      const float gamma = to_gamma(extended_sdr_scaled);

      if (tc == AVIF_TRANSFER_CHARACTERISTICS_SMPTE428 && linear > 0.9f) {
        continue;  // Smpte428 is a bit below the y=x diagonal at the high end.
      }

      // Check the point is above (or at) the y=x diagonal, with some tolerance.
      ASSERT_GE(gamma, linear - 1e-6f);
    }
  }
}

}  // namespace
}  // namespace avif
