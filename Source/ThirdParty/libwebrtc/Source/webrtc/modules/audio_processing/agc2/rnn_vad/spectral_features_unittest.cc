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
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr size_t kTestFeatureVectorSize = kNumBands + 3 * kNumLowerBands + 1;

// Writes non-zero sample values.
void WriteTestData(rtc::ArrayView<float> samples) {
  for (size_t i = 0; i < samples.size(); ++i) {
    samples[i] = i % 100;
  }
}

SpectralFeaturesView GetSpectralFeaturesView(
    std::array<float, kTestFeatureVectorSize>* feature_vector) {
  return {
      {feature_vector->data() + kNumLowerBands, kNumBands - kNumLowerBands},
      {feature_vector->data(), kNumLowerBands},
      {feature_vector->data() + kNumBands, kNumLowerBands},
      {feature_vector->data() + kNumBands + kNumLowerBands, kNumLowerBands},
      {feature_vector->data() + kNumBands + 2 * kNumLowerBands, kNumLowerBands},
      &(*feature_vector)[kNumBands + 3 * kNumLowerBands]};
}

constexpr float kInitialFeatureVal = -9999.f;

}  // namespace

TEST(RnnVadTest, SpectralFeaturesWithAndWithoutSilence) {
  // Initialize.
  SpectralFeaturesExtractor sfe;
  std::array<float, kFrameSize20ms24kHz> samples;
  rtc::ArrayView<float, kFrameSize20ms24kHz> samples_view(samples);
  bool is_silence;
  std::array<float, kTestFeatureVectorSize> feature_vector;
  auto feature_vector_view = GetSpectralFeaturesView(&feature_vector);

  // Write an initial value in the feature vector to detect changes.
  std::fill(feature_vector.begin(), feature_vector.end(), kInitialFeatureVal);

  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  // With silence.
  std::fill(samples.begin(), samples.end(), 0.f);
  is_silence = sfe.CheckSilenceComputeFeatures(samples_view, samples_view,
                                               feature_vector_view);
  // Silence is expected, the output won't be overwritten.
  EXPECT_TRUE(is_silence);
  EXPECT_TRUE(std::all_of(feature_vector.begin(), feature_vector.end(),
                          [](float x) { return x == kInitialFeatureVal; }));

  // With no silence.
  WriteTestData(samples);
  is_silence = sfe.CheckSilenceComputeFeatures(samples_view, samples_view,
                                               feature_vector_view);
  // Silence is not expected, the output will be overwritten.
  EXPECT_FALSE(is_silence);
  EXPECT_FALSE(std::all_of(feature_vector.begin(), feature_vector.end(),
                           [](float x) { return x == kInitialFeatureVal; }));
}

// When the input signal does not change, the spectral coefficients average does
// not change and the derivatives are zero. Similarly, the spectral variability
// score does not change either.
TEST(RnnVadTest, SpectralFeaturesConstantAverageZeroDerivative) {
  // Initialize.
  SpectralFeaturesExtractor sfe;
  std::array<float, kFrameSize20ms24kHz> samples;
  rtc::ArrayView<float, kFrameSize20ms24kHz> samples_view(samples);
  WriteTestData(samples);
  bool is_silence;

  // Fill the spectral features with test data.
  std::array<float, kTestFeatureVectorSize> feature_vector;
  auto feature_vector_view = GetSpectralFeaturesView(&feature_vector);
  for (size_t i = 0; i < kSpectralCoeffsHistorySize; ++i) {
    is_silence = sfe.CheckSilenceComputeFeatures(samples_view, samples_view,
                                                 feature_vector_view);
  }

  // Feed the test data one last time but using a different output vector.
  std::array<float, kTestFeatureVectorSize> feature_vector_last;
  auto feature_vector_last_view = GetSpectralFeaturesView(&feature_vector_last);
  is_silence = sfe.CheckSilenceComputeFeatures(samples_view, samples_view,
                                               feature_vector_last_view);

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
  // Spectral variability is unchanged.
  EXPECT_FLOAT_EQ(feature_vector[kNumBands + 3 * kNumLowerBands],
                  feature_vector_last[kNumBands + 3 * kNumLowerBands]);
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
