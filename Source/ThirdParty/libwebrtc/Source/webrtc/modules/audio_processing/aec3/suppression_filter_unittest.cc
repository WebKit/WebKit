/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/suppression_filter.h"

#include <math.h>
#include <algorithm>
#include <numeric>

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr float kPi = 3.141592f;

void ProduceSinusoid(int sample_rate_hz,
                     float sinusoidal_frequency_hz,
                     size_t* sample_counter,
                     rtc::ArrayView<float> x) {
  // Produce a sinusoid of the specified frequency.
  for (size_t k = *sample_counter, j = 0; k < (*sample_counter + kBlockSize);
       ++k, ++j) {
    x[j] =
        32767.f * sin(2.f * kPi * sinusoidal_frequency_hz * k / sample_rate_hz);
  }
  *sample_counter = *sample_counter + kBlockSize;
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for null suppressor output.
TEST(SuppressionFilter, NullOutput) {
  FftData cn;
  FftData cn_high_bands;
  std::array<float, kFftLengthBy2Plus1> gain;

  EXPECT_DEATH(SuppressionFilter(16000).ApplyGain(cn, cn_high_bands, gain, 1.0f,
                                                  nullptr),
               "");
}

// Verifies the check for allowed sample rate.
TEST(SuppressionFilter, ProperSampleRate) {
  EXPECT_DEATH(SuppressionFilter(16001), "");
}

#endif

// Verifies that no comfort noise is added when the gain is 1.
TEST(SuppressionFilter, ComfortNoiseInUnityGain) {
  SuppressionFilter filter(48000);
  FftData cn;
  FftData cn_high_bands;
  std::array<float, kFftLengthBy2Plus1> gain;

  gain.fill(1.f);
  cn.re.fill(1.f);
  cn.im.fill(1.f);
  cn_high_bands.re.fill(1.f);
  cn_high_bands.im.fill(1.f);

  std::vector<std::vector<float>> e(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> e_ref = e;
  filter.ApplyGain(cn, cn_high_bands, gain, 1.f, &e);

  for (size_t k = 0; k < e.size(); ++k) {
    EXPECT_EQ(e_ref[k], e[k]);
  }
}

// Verifies that the suppressor is able to suppress a signal.
TEST(SuppressionFilter, SignalSuppression) {
  SuppressionFilter filter(48000);
  FftData cn;
  FftData cn_high_bands;
  std::array<float, kFftLengthBy2Plus1> gain;
  std::vector<std::vector<float>> e(3, std::vector<float>(kBlockSize, 0.f));

  gain.fill(1.f);
  std::for_each(gain.begin() + 10, gain.end(), [](float& a) { a = 0.f; });

  cn.re.fill(0.f);
  cn.im.fill(0.f);
  cn_high_bands.re.fill(0.f);
  cn_high_bands.im.fill(0.f);

  size_t sample_counter = 0;

  float e0_input = 0.f;
  float e0_output = 0.f;
  for (size_t k = 0; k < 100; ++k) {
    ProduceSinusoid(16000, 16000 * 40 / kFftLengthBy2 / 2, &sample_counter,
                    e[0]);
    e0_input =
        std::inner_product(e[0].begin(), e[0].end(), e[0].begin(), e0_input);
    filter.ApplyGain(cn, cn_high_bands, gain, 1.f, &e);
    e0_output =
        std::inner_product(e[0].begin(), e[0].end(), e[0].begin(), e0_output);
  }

  EXPECT_LT(e0_output, e0_input / 1000.f);
}

// Verifies that the suppressor is able to pass through a desired signal while
// applying suppressing for some frequencies.
TEST(SuppressionFilter, SignalTransparency) {
  SuppressionFilter filter(48000);
  FftData cn;
  FftData cn_high_bands;
  std::array<float, kFftLengthBy2Plus1> gain;
  std::vector<std::vector<float>> e(3, std::vector<float>(kBlockSize, 0.f));

  gain.fill(1.f);
  std::for_each(gain.begin() + 30, gain.end(), [](float& a) { a = 0.f; });

  cn.re.fill(0.f);
  cn.im.fill(0.f);
  cn_high_bands.re.fill(0.f);
  cn_high_bands.im.fill(0.f);

  size_t sample_counter = 0;

  float e0_input = 0.f;
  float e0_output = 0.f;
  for (size_t k = 0; k < 100; ++k) {
    ProduceSinusoid(16000, 16000 * 10 / kFftLengthBy2 / 2, &sample_counter,
                    e[0]);
    e0_input =
        std::inner_product(e[0].begin(), e[0].end(), e[0].begin(), e0_input);
    filter.ApplyGain(cn, cn_high_bands, gain, 1.f, &e);
    e0_output =
        std::inner_product(e[0].begin(), e[0].end(), e[0].begin(), e0_output);
  }

  EXPECT_LT(0.9f * e0_input, e0_output);
}

// Verifies that the suppressor delay.
TEST(SuppressionFilter, Delay) {
  SuppressionFilter filter(48000);
  FftData cn;
  FftData cn_high_bands;
  std::array<float, kFftLengthBy2Plus1> gain;
  std::vector<std::vector<float>> e(3, std::vector<float>(kBlockSize, 0.f));

  gain.fill(1.f);

  cn.re.fill(0.f);
  cn.im.fill(0.f);
  cn_high_bands.re.fill(0.f);
  cn_high_bands.im.fill(0.f);

  for (size_t k = 0; k < 100; ++k) {
    for (size_t j = 0; j < 3; ++j) {
      for (size_t i = 0; i < kBlockSize; ++i) {
        e[j][i] = k * kBlockSize + i;
      }
    }

    filter.ApplyGain(cn, cn_high_bands, gain, 1.f, &e);
    if (k > 2) {
      for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < kBlockSize; ++i) {
          EXPECT_NEAR(k * kBlockSize + i - kBlockSize, e[j][i], 0.01);
        }
      }
    }
  }
}

}  // namespace webrtc
