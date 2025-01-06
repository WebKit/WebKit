/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/corruption_classifier.h"

#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {
namespace {

using ::testing::DoubleNear;
#if GTEST_HAS_DEATH_TEST
using ::testing::_;
using ::testing::HasSubstr;
#endif  // GTEST_HAS_DEATH_TEST

constexpr int kLumaThreshold = 3;
constexpr int kChromaThreshold = 2;

constexpr double kMaxAbsoluteError = 1e-4;

// Arbitrary values for testing.
constexpr double kBaseOriginalLumaSampleValue1 = 1.0;
constexpr double kBaseOriginalLumaSampleValue2 = 2.5;
constexpr double kBaseOriginalChromaSampleValue1 = 0.5;

constexpr FilteredSample kFilteredOriginalSampleValues[] = {
    {.value = kBaseOriginalLumaSampleValue1, .plane = ImagePlane::kLuma},
    {.value = kBaseOriginalLumaSampleValue2, .plane = ImagePlane::kLuma},
    {.value = kBaseOriginalChromaSampleValue1, .plane = ImagePlane::kChroma}};

// The value 14.0 corresponds to the corruption probability being on the same
// side of 0.5 in the `ScalarConfig` and `LogisticFunctionConfig`.
constexpr float kScaleFactor = 14.0;

constexpr float kGrowthRate = 1.0;
constexpr float kMidpoint = 7.0;

// Helper function to create fake compressed sample values.
std::vector<FilteredSample> GetCompressedSampleValues(
    double increase_value_luma,
    double increase_value_chroma) {
  return std::vector<FilteredSample>{
      {.value = kBaseOriginalLumaSampleValue1 + increase_value_luma,
       .plane = ImagePlane::kLuma},
      {.value = kBaseOriginalLumaSampleValue2 + increase_value_luma,
       .plane = ImagePlane::kLuma},
      {.value = kBaseOriginalChromaSampleValue1 + increase_value_chroma,
       .plane = ImagePlane::kChroma}};
}

#if GTEST_HAS_DEATH_TEST
TEST(CorruptionClassifierTest, EmptySamplesShouldResultInDeath) {
  CorruptionClassifier corruption_classifier(kScaleFactor);
  EXPECT_DEATH(corruption_classifier.CalculateCorruptionProbability(
                   {}, {}, kLumaThreshold, kChromaThreshold),
               _);
}

TEST(CorruptionClassifierTest, DifferentAmountOfSamplesShouldResultInDeath) {
  CorruptionClassifier corruption_classifier(kScaleFactor);
  const std::vector<FilteredSample> filtered_compressed_samples = {
      {.value = 1.0, .plane = ImagePlane::kLuma}};

  EXPECT_DEATH(corruption_classifier.CalculateCorruptionProbability(
                   kFilteredOriginalSampleValues, filtered_compressed_samples,
                   kLumaThreshold, kChromaThreshold),
               HasSubstr("The original and compressed frame have different "
                         "amounts of filtered samples."));
}
#endif  // GTEST_HAS_DEATH_TEST

TEST(CorruptionClassifierTest,
     SameSampleValuesShouldResultInNoCorruptionScalarConfig) {
  float kIncreaseValue = 0.0;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kScaleFactor);

  // Expected: score = 0.
  // Note that the `score` above corresponds to the value returned by the
  // `GetScore` function. Then this value should be passed through the Scalar or
  // Logistic function giving the expected result inside DoubleNear. This
  // applies for all the following tests.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.0, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     SameSampleValuesShouldResultInNoCorruptionLogisticFunctionConfig) {
  float kIncreaseValue = 0.0;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kGrowthRate, kMidpoint);

  // Expected: score = 0. See above for explanation why we have `0.0009` below.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.0009, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenAllSampleDifferencesBelowThresholdScalarConfig) {
  // Following value should be < `kLumaThreshold` and `kChromaThreshold`.
  const double kIncreaseValue = 1;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kScaleFactor);

  // Expected: score = 0.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.0, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenAllSampleDifferencesBelowThresholdLogisticFunctionConfig) {
  // Following value should be < `kLumaThreshold` and `kChromaThreshold`.
  const double kIncreaseValue = 1;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kGrowthRate, kMidpoint);

  // Expected: score = 0.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.0009, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenSmallPartOfSamplesAboveThresholdScalarConfig) {
  const double kIncreaseValueLuma = 1;
  const double kIncreaseValueChroma = 2.5;  // Above `kChromaThreshold`.
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValueLuma, kIncreaseValueChroma);

  CorruptionClassifier corruption_classifier(kScaleFactor);

  // Expected: score = (0.5)^2 / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.0060, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenSmallPartOfSamplesAboveThresholdLogisticFunctionConfig) {
  const double kIncreaseValueLuma = 1;
  const double kIncreaseValueChroma = 2.5;  // Above `kChromaThreshold`.
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValueLuma, kIncreaseValueChroma);

  CorruptionClassifier corruption_classifier(kGrowthRate, kMidpoint);

  // Expected: score = (0.5)^2 / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.001, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenAllSamplesSlightlyAboveThresholdScalarConfig) {
  const double kIncreaseValueLuma = 4.2;    // Above `kLumaThreshold`.
  const double kIncreaseValueChroma = 2.5;  // Above `kChromaThreshold`.
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValueLuma, kIncreaseValueChroma);

  CorruptionClassifier corruption_classifier(kScaleFactor);

  // Expected: score = ((0.5)^2 + 2*(1.2)^2) / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.07452, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenAllSamplesSlightlyAboveThresholdLogisticFunctionConfig) {
  const double kIncreaseValueLuma = 4.2;    // Above `kLumaThreshold`.
  const double kIncreaseValueChroma = 2.5;  // Above `kChromaThreshold`.
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValueLuma, kIncreaseValueChroma);

  CorruptionClassifier corruption_classifier(kGrowthRate, kMidpoint);

  // Expected: score = ((0.5)^2 + 2*(1.2)^2) / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.0026, kMaxAbsoluteError));
}

// Observe that the following 2 tests in practice could be classified as
// corrupted, if so wanted. However, with the `kGrowthRate`, `kMidpoint` and
// `kScaleFactor` values chosen in these tests, the score is not high enough to
// be classified as corrupted.
TEST(CorruptionClassifierTest,
     NoCorruptionWhenAllSamplesSomewhatAboveThresholdScalarConfig) {
  const double kIncreaseValue = 5.0;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kScaleFactor);

  // Expected: score = ((3)^2 + 2*(2)^2) / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.4048, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     NoCorruptionWhenAllSamplesSomewhatAboveThresholdLogisticFunctionConfig) {
  // Somewhat above `kLumaThreshold` and `kChromaThreshold`.
  const double kIncreaseValue = 5.0;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kGrowthRate, kMidpoint);

  // Expected: score = ((3)^2 + 2*(2)^2) / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(0.2086, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     CorruptionWhenAllSamplesWellAboveThresholdScalarConfig) {
  // Well above `kLumaThreshold` and `kChromaThreshold`.
  const double kIncreaseValue = 7.0;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kScaleFactor);

  // Expected: score = ((5)^2 + 2*(4)^2) / 3. Expected 1 because of capping.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(1, kMaxAbsoluteError));
}

TEST(CorruptionClassifierTest,
     CorruptionWhenAllSamplesWellAboveThresholdLogisticFunctionConfig) {
  // Well above `kLumaThreshold` and `kChromaThreshold`.
  const double kIncreaseValue = 7.0;
  const std::vector<FilteredSample> kFilteredCompressedSampleValues =
      GetCompressedSampleValues(kIncreaseValue, kIncreaseValue);

  CorruptionClassifier corruption_classifier(kGrowthRate, kMidpoint);

  // Expected: score = ((5)^2 + 2*(4)^2) / 3.
  EXPECT_THAT(
      corruption_classifier.CalculateCorruptionProbability(
          kFilteredOriginalSampleValues, kFilteredCompressedSampleValues,
          kLumaThreshold, kChromaThreshold),
      DoubleNear(1, kMaxAbsoluteError));
}

}  // namespace
}  // namespace webrtc
