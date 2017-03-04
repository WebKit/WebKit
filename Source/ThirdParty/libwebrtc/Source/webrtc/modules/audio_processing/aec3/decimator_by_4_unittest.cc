/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/decimator_by_4.h"

#include <math.h>
#include <algorithm>
#include <array>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

constexpr float kPi = 3.141592f;
constexpr size_t kNumStartupBlocks = 50;
constexpr size_t kNumBlocks = 1000;

void ProduceDecimatedSinusoidalOutputPower(int sample_rate_hz,
                                           float sinusoidal_frequency_hz,
                                           float* input_power,
                                           float* output_power) {
  float input[kBlockSize * kNumBlocks];

  // Produce a sinusoid of the specified frequency.
  for (size_t k = 0; k < kBlockSize * kNumBlocks; ++k) {
    input[k] =
        32767.f * sin(2.f * kPi * sinusoidal_frequency_hz * k / sample_rate_hz);
  }

  DecimatorBy4 decimator;
  std::array<float, kSubBlockSize * kNumBlocks> output;

  for (size_t k = 0; k < kNumBlocks; ++k) {
    std::array<float, kSubBlockSize> sub_block;

    decimator.Decimate(
        rtc::ArrayView<const float>(&input[k * kBlockSize], kBlockSize),
        &sub_block);

    std::copy(sub_block.begin(), sub_block.end(),
              output.begin() + k * kSubBlockSize);
  }

  ASSERT_GT(kNumBlocks, kNumStartupBlocks);
  rtc::ArrayView<const float> input_to_evaluate(
      &input[kNumStartupBlocks * kBlockSize],
      (kNumBlocks - kNumStartupBlocks) * kBlockSize);
  rtc::ArrayView<const float> output_to_evaluate(
      &output[kNumStartupBlocks * kSubBlockSize],
      (kNumBlocks - kNumStartupBlocks) * kSubBlockSize);
  *input_power =
      std::inner_product(input_to_evaluate.begin(), input_to_evaluate.end(),
                         input_to_evaluate.begin(), 0.f) /
      input_to_evaluate.size();
  *output_power =
      std::inner_product(output_to_evaluate.begin(), output_to_evaluate.end(),
                         output_to_evaluate.begin(), 0.f) /
      output_to_evaluate.size();
}

}  // namespace

// Verifies that there is little aliasing from upper frequencies in the
// downsampling.
TEST(DecimatorBy4, NoLeakageFromUpperFrequencies) {
  float input_power;
  float output_power;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    ProduceDecimatedSinusoidalOutputPower(rate, 3.f / 8.f * rate, &input_power,
                                          &output_power);
    EXPECT_GT(0.0001f * input_power, output_power);
  }
}

// Verifies that the impact of low-frequency content is small during the
// downsampling.
TEST(DecimatorBy4, NoImpactOnLowerFrequencies) {
  float input_power;
  float output_power;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    ProduceDebugText(rate);
    ProduceDecimatedSinusoidalOutputPower(rate, 200.f, &input_power,
                                          &output_power);
    EXPECT_LT(0.7f * input_power, output_power);
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies the check for the input size.
TEST(DecimatorBy4, WrongInputSize) {
  DecimatorBy4 decimator;
  std::vector<float> x(std::vector<float>(kBlockSize - 1, 0.f));
  std::array<float, kSubBlockSize> x_downsampled;
  EXPECT_DEATH(decimator.Decimate(x, &x_downsampled), "");
}

// Verifies the check for non-null output parameter.
TEST(DecimatorBy4, NullOutput) {
  DecimatorBy4 decimator;
  std::vector<float> x(std::vector<float>(kBlockSize, 0.f));
  EXPECT_DEATH(decimator.Decimate(x, nullptr), "");
}

#endif

}  // namespace webrtc
