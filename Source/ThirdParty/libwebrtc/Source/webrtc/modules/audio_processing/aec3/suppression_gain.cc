/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/suppression_gain.h"

#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <math.h>
#include <algorithm>
#include <functional>
#include <numeric>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/aec3/vector_math.h"

namespace webrtc {
namespace {

// Adjust the gains according to the presence of known external filters.
void AdjustForExternalFilters(std::array<float, kFftLengthBy2Plus1>* gain) {
  // Limit the low frequency gains to avoid the impact of the high-pass filter
  // on the lower-frequency gain influencing the overall achieved gain.
  (*gain)[0] = (*gain)[1] = std::min((*gain)[1], (*gain)[2]);

  // Limit the high frequency gains to avoid the impact of the anti-aliasing
  // filter on the upper-frequency gains influencing the overall achieved
  // gain. TODO(peah): Update this when new anti-aliasing filters are
  // implemented.
  constexpr size_t kAntiAliasingImpactLimit = (64 * 2000) / 8000;
  const float min_upper_gain = (*gain)[kAntiAliasingImpactLimit];
  std::for_each(
      gain->begin() + kAntiAliasingImpactLimit, gain->end() - 1,
      [min_upper_gain](float& a) { a = std::min(a, min_upper_gain); });
  (*gain)[kFftLengthBy2] = (*gain)[kFftLengthBy2Minus1];
}

// Computes the gain to apply for the bands beyond the first band.
float UpperBandsGain(
    bool saturated_echo,
    const std::vector<std::vector<float>>& render,
    const std::array<float, kFftLengthBy2Plus1>& low_band_gain) {
  RTC_DCHECK_LT(0, render.size());
  if (render.size() == 1) {
    return 1.f;
  }

  constexpr size_t kLowBandGainLimit = kFftLengthBy2 / 2;
  const float gain_below_8_khz = *std::min_element(
      low_band_gain.begin() + kLowBandGainLimit, low_band_gain.end());

  // Always attenuate the upper bands when there is saturated echo.
  if (saturated_echo) {
    return std::min(0.001f, gain_below_8_khz);
  }

  // Compute the upper and lower band energies.
  const auto sum_of_squares = [](float a, float b) { return a + b * b; };
  const float low_band_energy =
      std::accumulate(render[0].begin(), render[0].end(), 0.f, sum_of_squares);
  float high_band_energy = 0.f;
  for (size_t k = 1; k < render.size(); ++k) {
    const float energy = std::accumulate(render[k].begin(), render[k].end(),
                                         0.f, sum_of_squares);
    high_band_energy = std::max(high_band_energy, energy);
  }

  // If there is more power in the lower frequencies than the upper frequencies,
  // or if the power in upper frequencies is low, do not bound the gain in the
  // upper bands.
  float anti_howling_gain;
  constexpr float kThreshold = kSubBlockSize * 10.f * 10.f;
  if (high_band_energy < std::max(low_band_energy, kThreshold)) {
    anti_howling_gain = 1.f;
  } else {
    // In all other cases, bound the gain for upper frequencies.
    RTC_DCHECK_LE(low_band_energy, high_band_energy);
    RTC_DCHECK_NE(0.f, high_band_energy);
    anti_howling_gain = 0.01f * sqrtf(low_band_energy / high_band_energy);
  }

  // Choose the gain as the minimum of the lower and upper gains.
  return std::min(gain_below_8_khz, anti_howling_gain);
}

// Limits the gain increase.
void UpdateMaxGainIncrease(
    size_t no_saturation_counter,
    bool low_noise_render,
    const std::array<float, kFftLengthBy2Plus1>& last_echo,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& last_gain,
    const std::array<float, kFftLengthBy2Plus1>& new_gain,
    std::array<float, kFftLengthBy2Plus1>* gain_increase) {
  float max_increasing;
  float max_decreasing;
  float rate_increasing;
  float rate_decreasing;
  float min_increasing;
  float min_decreasing;

  if (low_noise_render) {
    max_increasing = 8.f;
    max_decreasing = 8.f;
    rate_increasing = 2.f;
    rate_decreasing = 2.f;
    min_increasing = 4.f;
    min_decreasing = 4.f;
  } else if (no_saturation_counter > 10) {
    max_increasing = 4.f;
    max_decreasing = 4.f;
    rate_increasing = 2.f;
    rate_decreasing = 2.f;
    min_increasing = 1.2f;
    min_decreasing = 2.f;
  } else {
    max_increasing = 1.2f;
    max_decreasing = 1.2f;
    rate_increasing = 1.5f;
    rate_decreasing = 1.5f;
    min_increasing = 1.f;
    min_decreasing = 1.f;
  }

  for (size_t k = 0; k < new_gain.size(); ++k) {
    if (echo[k] > last_echo[k]) {
      (*gain_increase)[k] =
          new_gain[k] > last_gain[k]
              ? std::min(max_increasing, (*gain_increase)[k] * rate_increasing)
              : min_increasing;
    } else {
      (*gain_increase)[k] =
          new_gain[k] > last_gain[k]
              ? std::min(max_decreasing, (*gain_increase)[k] * rate_decreasing)
              : min_decreasing;
    }
  }
}

// Computes the gain to reduce the echo to a non audible level.
void GainToNoAudibleEcho(
    bool low_noise_render,
    bool saturated_echo,
    const std::array<float, kFftLengthBy2Plus1>& nearend,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& masker,
    const std::array<float, kFftLengthBy2Plus1>& min_gain,
    const std::array<float, kFftLengthBy2Plus1>& max_gain,
    const std::array<float, kFftLengthBy2Plus1>& one_by_echo,
    std::array<float, kFftLengthBy2Plus1>* gain) {
  constexpr float kEchoMaskingMargin = 1.f / 100.f;
  const float nearend_masking_margin =
      low_noise_render ? 2.f : (saturated_echo ? 0.001f : 0.01f);

  for (size_t k = 0; k < gain->size(); ++k) {
    RTC_DCHECK_LE(0.f, nearend_masking_margin * nearend[k]);
    if (echo[k] <= nearend_masking_margin * nearend[k]) {
      (*gain)[k] = 1.f;
    } else {
      (*gain)[k] = kEchoMaskingMargin * masker[k] * one_by_echo[k];
    }

    (*gain)[k] = std::min(std::max((*gain)[k], min_gain[k]), max_gain[k]);
  }
}

// Computes the signal output power that masks the echo signal.
void MaskingPower(const std::array<float, kFftLengthBy2Plus1>& nearend,
                  const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
                  const std::array<float, kFftLengthBy2Plus1>& last_masker,
                  const std::array<float, kFftLengthBy2Plus1>& gain,
                  std::array<float, kFftLengthBy2Plus1>* masker) {
  std::array<float, kFftLengthBy2Plus1> side_band_masker;
  for (size_t k = 0; k < gain.size(); ++k) {
    side_band_masker[k] = nearend[k] * gain[k] + comfort_noise[k];
    (*masker)[k] = comfort_noise[k] + 0.1f * last_masker[k];
  }
  for (size_t k = 1; k < gain.size() - 1; ++k) {
    (*masker)[k] += 0.1f * (side_band_masker[k - 1] + side_band_masker[k + 1]);
  }
}

}  // namespace

// TODO(peah): Add further optimizations, in particular for the divisions.
void SuppressionGain::LowerBandGain(
    bool low_noise_render,
    bool saturated_echo,
    const std::array<float, kFftLengthBy2Plus1>& nearend,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
    std::array<float, kFftLengthBy2Plus1>* gain) {
  // Count the number of blocks since saturation.
  no_saturation_counter_ = saturated_echo ? 0 : no_saturation_counter_ + 1;

  // Precompute 1/echo (note that when the echo is zero, the precomputed value
  // is never used).
  std::array<float, kFftLengthBy2Plus1> one_by_echo;
  std::transform(echo.begin(), echo.end(), one_by_echo.begin(),
                 [](float a) { return a > 0.f ? 1.f / a : 1.f; });

  // Compute the minimum gain as the attenuating gain to put the signal just
  // above the zero sample values.
  std::array<float, kFftLengthBy2Plus1> min_gain;
  const float min_echo_power = low_noise_render ? 192.f : 64.f;
  if (no_saturation_counter_ > 10) {
    for (size_t k = 0; k < nearend.size(); ++k) {
      const float denom = std::min(nearend[k], echo[k]);
      min_gain[k] = denom > 0.f ? min_echo_power / denom : 1.f;
      min_gain[k] = std::min(min_gain[k], 1.f);
    }
  } else {
    min_gain.fill(0.f);
  }

  // Compute the maximum gain by limiting the gain increase from the previous
  // gain.
  std::array<float, kFftLengthBy2Plus1> max_gain;
  for (size_t k = 0; k < gain->size(); ++k) {
    max_gain[k] =
        std::min(std::max(last_gain_[k] * gain_increase_[k], 0.001f), 1.f);
  }

  // Iteratively compute the gain required to attenuate the echo to a non
  // noticeable level.
  gain->fill(0.f);
  for (int k = 0; k < 2; ++k) {
    std::array<float, kFftLengthBy2Plus1> masker;
    MaskingPower(nearend, comfort_noise, last_masker_, *gain, &masker);
    GainToNoAudibleEcho(low_noise_render, saturated_echo, nearend, echo, masker,
                        min_gain, max_gain, one_by_echo, gain);
    AdjustForExternalFilters(gain);
  }

  // Update the allowed maximum gain increase.
  UpdateMaxGainIncrease(no_saturation_counter_, low_noise_render, last_echo_,
                        echo, last_gain_, *gain, &gain_increase_);

  // Store data required for the gain computation of the next block.
  std::copy(echo.begin(), echo.end(), last_echo_.begin());
  std::copy(gain->begin(), gain->end(), last_gain_.begin());
  MaskingPower(nearend, comfort_noise, last_masker_, *gain, &last_masker_);
  aec3::VectorMath(optimization_).Sqrt(*gain);
}

SuppressionGain::SuppressionGain(Aec3Optimization optimization)
    : optimization_(optimization) {
  last_gain_.fill(1.f);
  last_masker_.fill(0.f);
  gain_increase_.fill(1.f);
  last_echo_.fill(0.f);
}

void SuppressionGain::GetGain(
    const std::array<float, kFftLengthBy2Plus1>& nearend,
    const std::array<float, kFftLengthBy2Plus1>& echo,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
    bool saturated_echo,
    const std::vector<std::vector<float>>& render,
    bool force_zero_gain,
    float* high_bands_gain,
    std::array<float, kFftLengthBy2Plus1>* low_band_gain) {
  RTC_DCHECK(high_bands_gain);
  RTC_DCHECK(low_band_gain);

  if (force_zero_gain) {
    last_gain_.fill(0.f);
    std::copy(comfort_noise.begin(), comfort_noise.end(), last_masker_.begin());
    low_band_gain->fill(0.f);
    gain_increase_.fill(1.f);
    *high_bands_gain = 0.f;
    return;
  }

  bool low_noise_render = low_render_detector_.Detect(render);

  // Compute gain for the lower band.
  LowerBandGain(low_noise_render, saturated_echo, nearend, echo, comfort_noise,
                low_band_gain);

  // Compute the gain for the upper bands.
  *high_bands_gain = UpperBandsGain(saturated_echo, render, *low_band_gain);
}

// Detects when the render signal can be considered to have low power and
// consist of stationary noise.
bool SuppressionGain::LowNoiseRenderDetector::Detect(
    const std::vector<std::vector<float>>& render) {
  float x2_sum = 0.f;
  float x2_max = 0.f;
  for (auto x_k : render[0]) {
    const float x2 = x_k * x_k;
    x2_sum += x2;
    x2_max = std::max(x2_max, x2);
  }

  constexpr float kThreshold = 50.f * 50.f * 64.f;
  const bool low_noise_render =
      average_power_ < kThreshold && x2_max < 3 * average_power_;
  average_power_ = average_power_ * 0.9f + x2_sum * 0.1f;
  return low_noise_render;
}

}  // namespace webrtc
