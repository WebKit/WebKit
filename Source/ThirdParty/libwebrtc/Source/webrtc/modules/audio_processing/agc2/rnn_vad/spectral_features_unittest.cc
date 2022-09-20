/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features.h"

#include <algorithm>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_compare.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kTestFeatureVectorSize = kNumBands + 3 * kNumLowerBands + 1;

// Writes non-zero sample values.
void WriteTestData(rtc::ArrayView<float> samples) {
  for (int i = 0; rtc::SafeLt(i, samples.size()); ++i) {
    samples[i] = i % 100;
  }
}

rtc::ArrayView<float, kNumBands - kNumLowerBands> GetHigherBandsSpectrum(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return {feature_vector->data() + kNumLowerBands, kNumBands - kNumLowerBands};
}

rtc::ArrayView<float, kNumLowerBands> GetAverage(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return {feature_vector->data(), kNumLowerBands};
}

rtc::ArrayView<float, kNumLowerBands> GetFirstDerivative(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return {feature_vector->data() + kNumBands, kNumLowerBands};
}

rtc::ArrayView<float, kNumLowerBands> GetSecondDerivative(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return {feature_vector->data() + kNumBands + kNumLowerBands, kNumLowerBands};
}

rtc::ArrayView<float, kNumLowerBands> GetCepstralCrossCorrelation(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return {feature_vector->data() + kNumBands + 2 * kNumLowerBands,
          kNumLowerBands};
}

float* GetCepstralVariability(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return feature_vector->data() + kNumBands + 3 * kNumLowerBands;
}

constexpr float kInitialFeatureVal = -9999.f;

// Checks that silence is detected when the input signal is 0 and that the
// feature vector is written only if the input signal is not tagged as silence.
TEST(RnnVadTest, SpectralFeaturesWithAndWithoutSilence) {
  // Initialize.
  SpectralFeaturesExtractor sfe;
  std::array<float, kFrameSize20ms24kHz> samples;
  rtc::ArrayView<float, kFrameSize20ms24kHz> samples_view(samples);
  bool is_silence;
  std::array<float, kTestFeatureVectorSize> feature_vector;

  // Write an initial value in the feature vector to detect changes.
  std::fill(feature_vector.begin(), feature_vector.end(), kInitialFeatureVal);

  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  // With silence.
  std::fill(samples.begin(), samples.end(), 0.f);
  is_silence = sfe.CheckSilenceComputeFeatures(
      samples_view, samples_view, GetHigherBandsSpectrum(&feature_vector),
      GetAverage(&feature_vector), GetFirstDerivative(&feature_vector),
      GetSecondDerivative(&feature_vector),
      GetCepstralCrossCorrelation(&feature_vector),
      GetCepstralVariability(&feature_vector));
  // Silence is expected, the output won't be overwritten.
  EXPECT_TRUE(is_silence);
  EXPECT_TRUE(std::all_of(feature_vector.begin(), feature_vector.end(),
                          [](float x) { return x == kInitialFeatureVal; }));

  // With no silence.
  WriteTestData(samples);
  is_silence = sfe.CheckSilenceComputeFeatures(
      samples_view, samples_view, GetHigherBandsSpectrum(&feature_vector),
      GetAverage(&feature_vector), GetFirstDerivative(&feature_vector),
      GetSecondDerivative(&feature_vector),
      GetCepstralCrossCorrelation(&feature_vector),
      GetCepstralVariability(&feature_vector));
  // Silence is not expected, the output will be overwritten.
  EXPECT_FALSE(is_silence);
  EXPECT_FALSE(std::all_of(feature_vector.begin(), feature_vector.end(),
                           [](float x) { return x == kInitialFeatureVal; }));
}

// Feeds a constant input signal and checks that:
// - the cepstral coefficients average does not change;
// - the derivatives are zero;
// - the cepstral variability score does not change.
TEST(RnnVadTest, CepstralFeaturesConstantAverageZeroDerivative) {
  // Initialize.
  SpectralFeaturesExtractor sfe;
  std::array<float, kFrameSize20ms24kHz> samples;
  rtc::ArrayView<float, kFrameSize20ms24kHz> samples_view(samples);
  WriteTestData(samples);

  // Fill the spectral features with test data.
  std::array<float, kTestFeatureVectorSize> feature_vector;
  for (int i = 0; i < kCepstralCoeffsHistorySize; ++i) {
    sfe.CheckSilenceComputeFeatures(
        samples_view, samples_view, GetHigherBandsSpectrum(&feature_vector),
        GetAverage(&feature_vector), GetFirstDerivative(&feature_vector),
        GetSecondDerivative(&feature_vector),
        GetCepstralCrossCorrelation(&feature_vector),
        GetCepstralVariability(&feature_vector));
  }

  // Feed the test data one last time but using a different output vector.
  std::array<float, kTestFeatureVectorSize> feature_vector_last;
  sfe.CheckSilenceComputeFeatures(
      samples_view, samples_view, GetHigherBandsSpectrum(&feature_vector_last),
      GetAverage(&feature_vector_last),
      GetFirstDerivative(&feature_vector_last),
      GetSecondDerivative(&feature_vector_last),
      GetCepstralCrossCorrelation(&feature_vector_last),
      GetCepstralVariability(&feature_vector_last));

  // Average is unchanged.
  ExpectEqualFloatArray({feature_vector.data(), kNumLowerBands},
                        {feature_vector_last.data(), kNumLowerBands});
  // First and second derivatives are zero.
  constexpr std::array<float, kNumLowerBands> zeros{};
  ExpectEqualFloatArray(
      {feature_vector_last.data() + kNumBands, kNumLowerBands}, zeros);
  ExpectEqualFloatArray(
      {feature_vector_last.data() + kNumBands + kNumLowerBands, kNumLowerBands},
      zeros);
  // Variability is unchanged.
  EXPECT_FLOAT_EQ(feature_vector[kNumBands + 3 * kNumLowerBands],
                  feature_vector_last[kNumBands + 3 * kNumLowerBands]);
}

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
