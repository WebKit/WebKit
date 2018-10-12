/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/decimator.h"

#include <math.h>
#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

std::string ProduceDebugText(int sample_rate_hz) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.Release();
}

constexpr size_t kDownSamplingFactors[] = {2, 4, 8};
constexpr float kPi = 3.141592f;
constexpr size_t kNumStartupBlocks = 50;
constexpr size_t kNumBlocks = 1000;

void ProduceDecimatedSinusoidalOutputPower(int sample_rate_hz,
                                           size_t down_sampling_factor,
                                           float sinusoidal_frequency_hz,
                                           float* input_power,
                                           float* output_power) {
  float input[kBlockSize * kNumBlocks];
  const size_t sub_block_size = kBlockSize / down_sampling_factor;

  // Produce a sinusoid of the specified frequency.
  for (size_t k = 0; k < kBlockSize * kNumBlocks; ++k) {
    input[k] =
        32767.f * sin(2.f * kPi * sinusoidal_frequency_hz * k / sample_rate_hz);
  }

  Decimator decimator(down_sampling_factor);
  std::vector<float> output(sub_block_size * kNumBlocks);

  for (size_t k = 0; k < kNumBlocks; ++k) {
    std::vector<float> sub_block(sub_block_size);

    decimator.Decimate(
        rtc::ArrayView<const float>(&input[k * kBlockSize], kBlockSize),
        sub_block);

    std::copy(sub_block.begin(), sub_block.end(),
              output.begin() + k * sub_block_size);
  }

  ASSERT_GT(kNumBlocks, kNumStartupBlocks);
  rtc::ArrayView<const float> input_to_evaluate(
      &input[kNumStartupBlocks * kBlockSize],
      (kNumBlocks - kNumStartupBlocks) * kBlockSize);
  rtc::ArrayView<const float> output_to_evaluate(
      &output[kNumStartupBlocks * sub_block_size],
      (kNumBlocks - kNumStartupBlocks) * sub_block_size);
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
TEST(Decimator, NoLeakageFromUpperFrequencies) {
  float input_power;
  float output_power;
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (auto down_sampling_factor : kDownSamplingFactors) {
      ProduceDebugText(rate);
      ProduceDecimatedSinusoidalOutputPower(rate, down_sampling_factor,
                                            3.f / 8.f * rate, &input_power,
                                            &output_power);
      EXPECT_GT(0.0001f * input_power, output_power);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies the check for the input size.
TEST(Decimator, WrongInputSize) {
  Decimator decimator(4);
  std::vector<float> x(std::vector<float>(kBlockSize - 1, 0.f));
  std::array<float, kBlockSize / 4> x_downsampled;
  EXPECT_DEATH(decimator.Decimate(x, x_downsampled), "");
}

// Verifies the check for non-null output parameter.
TEST(Decimator, NullOutput) {
  Decimator decimator(4);
  std::vector<float> x(std::vector<float>(kBlockSize, 0.f));
  EXPECT_DEATH(decimator.Decimate(x, nullptr), "");
}

// Verifies the check for the output size.
TEST(Decimator, WrongOutputSize) {
  Decimator decimator(4);
  std::vector<float> x(std::vector<float>(kBlockSize, 0.f));
  std::array<float, kBlockSize / 4 - 1> x_downsampled;
  EXPECT_DEATH(decimator.Decimate(x, x_downsampled), "");
}

// Verifies the check for the correct downsampling factor.
TEST(Decimator, CorrectDownSamplingFactor) {
  EXPECT_DEATH(Decimator(3), "");
}

#endif

}  // namespace webrtc
