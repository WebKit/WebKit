/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/cascaded_biquad_filter.h"

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
TEST(CascadedBiquadFilter, InputSizeCheckVerification) {
  const std::vector<float> input = CreateInputWithIncreasingValues(10);
  std::vector<float> output(input.size() - 1);

  CascadedBiQuadFilter filter(kTransparentCoefficients, 1);
  EXPECT_DEATH(filter.Process(input, output), "");
}
#endif

}  // namespace webrtc
