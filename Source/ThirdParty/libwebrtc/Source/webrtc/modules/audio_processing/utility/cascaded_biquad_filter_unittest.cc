/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/utility/cascaded_biquad_filter.h"

#include <vector>

#include "test/gtest.h"

namespace webrtc {

namespace {

// Coefficients for a second order Butterworth high-pass filter with cutoff
// frequency 100 Hz.
const CascadedBiQuadFilter::BiQuadCoefficients kHighPassFilterCoefficients = {
    {0.97261f, -1.94523f, 0.97261f},
    {-1.94448f, 0.94598f}};

const CascadedBiQuadFilter::BiQuadCoefficients kTransparentCoefficients = {
    {1.f, 0.f, 0.f},
    {0.f, 0.f}};

const CascadedBiQuadFilter::BiQuadCoefficients kBlockingCoefficients = {
    {0.f, 0.f, 0.f},
    {0.f, 0.f}};

std::vector<float> CreateInputWithIncreasingValues(size_t vector_length) {
  std::vector<float> v(vector_length);
  for (size_t k = 0; k < v.size(); ++k) {
    v[k] = k;
  }
  return v;
}

}  // namespace

// Verifies that the filter applies an effect which removes the input signal.
// The test also verifies that the in-place Process API call works as intended.
TEST(CascadedBiquadFilter, BlockingConfiguration) {
  std::vector<float> values = CreateInputWithIncreasingValues(1000);

  CascadedBiQuadFilter filter(kBlockingCoefficients, 1);
  filter.Process(values);

  EXPECT_EQ(std::vector<float>(1000, 0.f), values);
}

// Verifies that the filter is able to form a zero-mean output from a
// non-zeromean input signal when coefficients for a high-pass filter are
// applied. The test also verifies that the filter works with multiple biquads.
TEST(CascadedBiquadFilter, HighPassConfiguration) {
  std::vector<float> values(1000);
  for (size_t k = 0; k < values.size(); ++k) {
    values[k] = 1.f;
  }

  CascadedBiQuadFilter filter(kHighPassFilterCoefficients, 2);
  filter.Process(values);

  for (size_t k = values.size() / 2; k < values.size(); ++k) {
    EXPECT_NEAR(0.f, values[k], 1e-4);
  }
}

// Verifies that the reset functionality works as intended.
TEST(CascadedBiquadFilter, HighPassConfigurationResetFunctionality) {
  CascadedBiQuadFilter filter(kHighPassFilterCoefficients, 2);

  std::vector<float> values1(100, 1.f);
  filter.Process(values1);

  filter.Reset();

  std::vector<float> values2(100, 1.f);
  filter.Process(values2);

  for (size_t k = 0; k < values1.size(); ++k) {
    EXPECT_EQ(values1[k], values2[k]);
  }
}

// Verifies that the filter is able to produce a transparent effect with no
// impact on the data when the proper coefficients are applied. The test also
// verifies that the non-in-place Process API call works as intended.
TEST(CascadedBiquadFilter, TransparentConfiguration) {
  const std::vector<float> input = CreateInputWithIncreasingValues(1000);
  std::vector<float> output(input.size());

  CascadedBiQuadFilter filter(kTransparentCoefficients, 1);
  filter.Process(input, output);

  EXPECT_EQ(input, output);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the check of the lengths for the input and output works for the
// non-in-place call.
TEST(CascadedBiquadFilterDeathTest, InputSizeCheckVerification) {
  const std::vector<float> input = CreateInputWithIncreasingValues(10);
  std::vector<float> output(input.size() - 1);

  CascadedBiQuadFilter filter(kTransparentCoefficients, 1);
  EXPECT_DEATH(filter.Process(input, output), "");
}
#endif

// Verifies the conversion from zero, pole, gain to filter coefficients for
// lowpass filter.
TEST(CascadedBiquadFilter, BiQuadParamLowPass) {
  CascadedBiQuadFilter::BiQuadParam param(
      {-1.0f, 0.0f}, {0.23146901f, 0.39514232f}, 0.1866943331163784f);
  CascadedBiQuadFilter::BiQuad filter(param);
  const float epsilon = 1e-6f;
  EXPECT_NEAR(filter.coefficients.b[0], 0.18669433f, epsilon);
  EXPECT_NEAR(filter.coefficients.b[1], 0.37338867f, epsilon);
  EXPECT_NEAR(filter.coefficients.b[2], 0.18669433f, epsilon);
  EXPECT_NEAR(filter.coefficients.a[0], -0.46293803f, epsilon);
  EXPECT_NEAR(filter.coefficients.a[1], 0.20971536f, epsilon);
}

// Verifies the conversion from zero, pole, gain to filter coefficients for
// highpass filter.
TEST(CascadedBiquadFilter, BiQuadParamHighPass) {
  CascadedBiQuadFilter::BiQuadParam param(
      {1.0f, 0.0f}, {0.72712179f, 0.21296904f}, 0.75707637533388494f);
  CascadedBiQuadFilter::BiQuad filter(param);
  const float epsilon = 1e-6f;
  EXPECT_NEAR(filter.coefficients.b[0], 0.75707638f, epsilon);
  EXPECT_NEAR(filter.coefficients.b[1], -1.51415275f, epsilon);
  EXPECT_NEAR(filter.coefficients.b[2], 0.75707638f, epsilon);
  EXPECT_NEAR(filter.coefficients.a[0], -1.45424359f, epsilon);
  EXPECT_NEAR(filter.coefficients.a[1], 0.57406192f, epsilon);
}

// Verifies the conversion from zero, pole, gain to filter coefficients for
// bandpass filter.
TEST(CascadedBiquadFilter, BiQuadParamBandPass) {
  CascadedBiQuadFilter::BiQuadParam param(
      {1.0f, 0.0f}, {1.11022302e-16f, 0.71381051f}, 0.2452372752527856f, true);
  CascadedBiQuadFilter::BiQuad filter(param);
  const float epsilon = 1e-6f;
  EXPECT_NEAR(filter.coefficients.b[0], 0.24523728f, epsilon);
  EXPECT_NEAR(filter.coefficients.b[1], 0.f, epsilon);
  EXPECT_NEAR(filter.coefficients.b[2], -0.24523728f, epsilon);
  EXPECT_NEAR(filter.coefficients.a[0], -2.22044605e-16f, epsilon);
  EXPECT_NEAR(filter.coefficients.a[1], 5.09525449e-01f, epsilon);
}

}  // namespace webrtc
