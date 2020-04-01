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
#include <cmath>
#include <numeric>

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr float kPi = 3.141592f;

void ProduceSinusoid(int sample_rate_hz,
                     float sinusoidal_frequency_hz,
                     size_t* sample_counter,
                     std::vector<std::vector<std::vector<float>>>* x) {
  // Produce a sinusoid of the specified frequency.
  for (size_t k = *sample_counter, j = 0; k < (*sample_counter + kBlockSize);
       ++k, ++j) {
    for (size_t channel = 0; channel < (*x)[0].size(); ++channel) {
      (*x)[0][channel][j] =
          32767.f *
          std::sin(2.f * kPi * sinusoidal_frequency_hz * k / sample_rate_hz);
    }
  }
  *sample_counter = *sample_counter + kBlockSize;

  for (size_t band = 1; band < x->size(); ++band) {
    for (size_t channel = 0; channel < (*x)[band].size(); ++channel) {
      std::fill((*x)[band][channel].begin(), (*x)[band][channel].end(), 0.f);
    }
  }
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for null suppressor output.
TEST(SuppressionFilter, NullOutput) {
  std::vector<FftData> cn(1);
  std::vector<FftData> cn_high_bands(1);
  std::vector<FftData> E(1);
  std::array<float, kFftLengthBy2Plus1> gain;

  EXPECT_DEATH(SuppressionFilter(Aec3Optimization::kNone, 16000, 1)
                   .ApplyGain(cn, cn_high_bands, gain, 1.0f, E, nullptr),
               "");
}

// Verifies the check for allowed sample rate.
TEST(SuppressionFilter, ProperSampleRate) {
  EXPECT_DEATH(SuppressionFilter(Aec3Optimization::kNone, 16001, 1), "");
}

#endif

// Verifies that no comfort noise is added when the gain is 1.
TEST(SuppressionFilter, ComfortNoiseInUnityGain) {
  SuppressionFilter filter(Aec3Optimization::kNone, 48000, 1);
  std::vector<FftData> cn(1);
  std::vector<FftData> cn_high_bands(1);
  std::array<float, kFftLengthBy2Plus1> gain;
  std::array<float, kFftLengthBy2> e_old_;
  Aec3Fft fft;

  e_old_.fill(0.f);
  gain.fill(1.f);
  cn[0].re.fill(1.f);
  cn[0].im.fill(1.f);
  cn_high_bands[0].re.fill(1.f);
  cn_high_bands[0].im.fill(1.f);

  std::vector<std::vector<std::vector<float>>> e(
      3,
      std::vector<std::vector<float>>(1, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::vector<float>>> e_ref = e;

  std::vector<FftData> E(1);
  fft.PaddedFft(e[0][0], e_old_, Aec3Fft::Window::kSqrtHanning, &E[0]);
  std::copy(e[0][0].begin(), e[0][0].end(), e_old_.begin());

  filter.ApplyGain(cn, cn_high_bands, gain, 1.f, E, &e);

  for (size_t band = 0; band < e.size(); ++band) {
    for (size_t channel = 0; channel < e[band].size(); ++channel) {
      for (size_t sample = 0; sample < e[band][channel].size(); ++sample) {
        EXPECT_EQ(e_ref[band][channel][sample], e[band][channel][sample]);
      }
    }
  }
}

// Verifies that the suppressor is able to suppress a signal.
TEST(SuppressionFilter, SignalSuppression) {
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
  constexpr size_t kNumChannels = 1;

  SuppressionFilter filter(Aec3Optimization::kNone, kSampleRateHz, 1);
  std::vector<FftData> cn(1);
  std::vector<FftData> cn_high_bands(1);
  std::array<float, kFftLengthBy2> e_old_;
  Aec3Fft fft;
  std::array<float, kFftLengthBy2Plus1> gain;
  std::vector<std::vector<std::vector<float>>> e(
      kNumBands, std::vector<std::vector<float>>(
                     kNumChannels, std::vector<float>(kBlockSize, 0.f)));
  e_old_.fill(0.f);

  gain.fill(1.f);
  std::for_each(gain.begin() + 10, gain.end(), [](float& a) { a = 0.f; });

  cn[0].re.fill(0.f);
  cn[0].im.fill(0.f);
  cn_high_bands[0].re.fill(0.f);
  cn_high_bands[0].im.fill(0.f);

  size_t sample_counter = 0;

  float e0_input = 0.f;
  float e0_output = 0.f;
  for (size_t k = 0; k < 100; ++k) {
    ProduceSinusoid(16000, 16000 * 40 / kFftLengthBy2 / 2, &sample_counter, &e);
    e0_input = std::inner_product(e[0][0].begin(), e[0][0].end(),
                                  e[0][0].begin(), e0_input);

    std::vector<FftData> E(1);
    fft.PaddedFft(e[0][0], e_old_, Aec3Fft::Window::kSqrtHanning, &E[0]);
    std::copy(e[0][0].begin(), e[0][0].end(), e_old_.begin());

    filter.ApplyGain(cn, cn_high_bands, gain, 1.f, E, &e);
    e0_output = std::inner_product(e[0][0].begin(), e[0][0].end(),
                                   e[0][0].begin(), e0_output);
  }

  EXPECT_LT(e0_output, e0_input / 1000.f);
}

// Verifies that the suppressor is able to pass through a desired signal while
// applying suppressing for some frequencies.
TEST(SuppressionFilter, SignalTransparency) {
  constexpr size_t kNumChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  SuppressionFilter filter(Aec3Optimization::kNone, kSampleRateHz, 1);
  std::vector<FftData> cn(1);
  std::array<float, kFftLengthBy2> e_old_;
  Aec3Fft fft;
  std::vector<FftData> cn_high_bands(1);
  std::array<float, kFftLengthBy2Plus1> gain;
  std::vector<std::vector<std::vector<float>>> e(
      kNumBands, std::vector<std::vector<float>>(
                     kNumChannels, std::vector<float>(kBlockSize, 0.f)));
  e_old_.fill(0.f);
  gain.fill(1.f);
  std::for_each(gain.begin() + 30, gain.end(), [](float& a) { a = 0.f; });

  cn[0].re.fill(0.f);
  cn[0].im.fill(0.f);
  cn_high_bands[0].re.fill(0.f);
  cn_high_bands[0].im.fill(0.f);

  size_t sample_counter = 0;

  float e0_input = 0.f;
  float e0_output = 0.f;
  for (size_t k = 0; k < 100; ++k) {
    ProduceSinusoid(16000, 16000 * 10 / kFftLengthBy2 / 2, &sample_counter, &e);
    e0_input = std::inner_product(e[0][0].begin(), e[0][0].end(),
                                  e[0][0].begin(), e0_input);

    std::vector<FftData> E(1);
    fft.PaddedFft(e[0][0], e_old_, Aec3Fft::Window::kSqrtHanning, &E[0]);
    std::copy(e[0][0].begin(), e[0][0].end(), e_old_.begin());

    filter.ApplyGain(cn, cn_high_bands, gain, 1.f, E, &e);
    e0_output = std::inner_product(e[0][0].begin(), e[0][0].end(),
                                   e[0][0].begin(), e0_output);
  }

  EXPECT_LT(0.9f * e0_input, e0_output);
}

// Verifies that the suppressor delay.
TEST(SuppressionFilter, Delay) {
  constexpr size_t kNumChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  SuppressionFilter filter(Aec3Optimization::kNone, kSampleRateHz, 1);
  std::vector<FftData> cn(1);
  std::vector<FftData> cn_high_bands(1);
  std::array<float, kFftLengthBy2> e_old_;
  Aec3Fft fft;
  std::array<float, kFftLengthBy2Plus1> gain;
  std::vector<std::vector<std::vector<float>>> e(
      kNumBands, std::vector<std::vector<float>>(
                     kNumChannels, std::vector<float>(kBlockSize, 0.f)));

  gain.fill(1.f);

  cn[0].re.fill(0.f);
  cn[0].im.fill(0.f);
  cn_high_bands[0].re.fill(0.f);
  cn_high_bands[0].im.fill(0.f);

  for (size_t k = 0; k < 100; ++k) {
    for (size_t band = 0; band < kNumBands; ++band) {
      for (size_t channel = 0; channel < kNumChannels; ++channel) {
        for (size_t sample = 0; sample < kBlockSize; ++sample) {
          e[band][channel][sample] = k * kBlockSize + sample + channel;
        }
      }
    }

    std::vector<FftData> E(1);
    fft.PaddedFft(e[0][0], e_old_, Aec3Fft::Window::kSqrtHanning, &E[0]);
    std::copy(e[0][0].begin(), e[0][0].end(), e_old_.begin());

    filter.ApplyGain(cn, cn_high_bands, gain, 1.f, E, &e);
    if (k > 2) {
      for (size_t band = 0; band < kNumBands; ++band) {
        for (size_t channel = 0; channel < kNumChannels; ++channel) {
          for (size_t sample = 0; sample < kBlockSize; ++sample) {
            EXPECT_NEAR(k * kBlockSize + sample - kBlockSize + channel,
                        e[band][channel][sample], 0.01);
          }
        }
      }
    }
  }
}

}  // namespace webrtc
