/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/residual_echo_estimator.h"

#include <numeric>
#include <vector>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace {

// Estimates the echo generating signal power as gated maximal power over a time
// window.
void EchoGeneratingPower(const RenderBuffer& render_buffer,
                         size_t min_delay,
                         size_t max_delay,
                         std::array<float, kFftLengthBy2Plus1>* X2) {
  X2->fill(0.f);
  for (size_t k = min_delay; k <= max_delay; ++k) {
    std::transform(X2->begin(), X2->end(), render_buffer.Spectrum(k).begin(),
                   X2->begin(),
                   [](float a, float b) { return std::max(a, b); });
  }

  // Apply soft noise gate of -78 dBFS.
  constexpr float kNoiseGatePower = 27509.42f;
  std::for_each(X2->begin(), X2->end(), [kNoiseGatePower](float& a) {
    if (kNoiseGatePower > a) {
      a = std::max(0.f, a - 0.3f * (kNoiseGatePower - a));
    }
  });
}

constexpr int kNoiseFloorCounterMax = 50;
constexpr float kNoiseFloorMin = 10.f * 10.f * 128.f * 128.f;

// Updates estimate for the power of the stationary noise component in the
// render signal.
void RenderNoisePower(
    const RenderBuffer& render_buffer,
    std::array<float, kFftLengthBy2Plus1>* X2_noise_floor,
    std::array<int, kFftLengthBy2Plus1>* X2_noise_floor_counter) {
  RTC_DCHECK(X2_noise_floor);
  RTC_DCHECK(X2_noise_floor_counter);

  const auto render_power = render_buffer.Spectrum(0);
  RTC_DCHECK_EQ(X2_noise_floor->size(), render_power.size());
  RTC_DCHECK_EQ(X2_noise_floor_counter->size(), render_power.size());

  // Estimate the stationary noise power in a minimum statistics manner.
  for (size_t k = 0; k < render_power.size(); ++k) {
    // Decrease rapidly.
    if (render_power[k] < (*X2_noise_floor)[k]) {
      (*X2_noise_floor)[k] = render_power[k];
      (*X2_noise_floor_counter)[k] = 0;
    } else {
      // Increase in a delayed, leaky manner.
      if ((*X2_noise_floor_counter)[k] >= kNoiseFloorCounterMax) {
        (*X2_noise_floor)[k] =
            std::max((*X2_noise_floor)[k] * 1.1f, kNoiseFloorMin);
      } else {
        ++(*X2_noise_floor_counter)[k];
      }
    }
  }
}

// Assume a minimum echo path gain of -33 dB for headsets.
constexpr float kHeadsetEchoPathGain = 0.0005f;

}  // namespace

ResidualEchoEstimator::ResidualEchoEstimator() {
  Reset();
}

ResidualEchoEstimator::~ResidualEchoEstimator() = default;

void ResidualEchoEstimator::Estimate(
    bool using_subtractor_output,
    const AecState& aec_state,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& S2_linear,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  RTC_DCHECK(R2);

  const rtc::Optional<size_t> delay =
      aec_state.FilterDelay()
          ? aec_state.FilterDelay()
          : (aec_state.ExternalDelay() ? aec_state.ExternalDelay()
                                       : rtc::Optional<size_t>());

  // Estimate the power of the stationary noise in the render signal.
  RenderNoisePower(render_buffer, &X2_noise_floor_, &X2_noise_floor_counter_);

  // Estimate the residual echo power.
  const bool use_linear_echo_power =
      aec_state.UsableLinearEstimate() && using_subtractor_output;
  if (use_linear_echo_power && !aec_state.HeadsetDetected()) {
    RTC_DCHECK(aec_state.FilterDelay());
    const int filter_delay = *aec_state.FilterDelay();
    LinearEstimate(S2_linear, aec_state.Erle(), filter_delay, R2);
    AddEchoReverb(S2_linear, aec_state.SaturatedEcho(), filter_delay,
                  aec_state.ReverbDecayFactor(), R2);
  } else {
    // Estimate the echo generating signal power.
    std::array<float, kFftLengthBy2Plus1> X2;
    if (aec_state.ExternalDelay() || aec_state.FilterDelay()) {
      RTC_DCHECK(delay);
      const int delay_use = static_cast<int>(*delay);

      // Computes the spectral power over the blocks surrounding the delay.
      RTC_DCHECK_LT(delay_use, kResidualEchoPowerRenderWindowSize);
      EchoGeneratingPower(
          render_buffer, std::max(0, delay_use - 1),
          std::min(kResidualEchoPowerRenderWindowSize - 1, delay_use + 1), &X2);
    } else {
      // Computes the spectral power over the latest blocks.
      EchoGeneratingPower(render_buffer, 0,
                          kResidualEchoPowerRenderWindowSize - 1, &X2);
    }

    // Subtract the stationary noise power to avoid stationary noise causing
    // excessive echo suppression.
    std::transform(
        X2.begin(), X2.end(), X2_noise_floor_.begin(), X2.begin(),
        [](float a, float b) { return std::max(0.f, a - 10.f * b); });

    NonLinearEstimate(
        aec_state.HeadsetDetected() ? kHeadsetEchoPathGain : kFixedEchoPathGain,
        X2, Y2, R2);
    AddEchoReverb(*R2, aec_state.SaturatedEcho(),
                  std::min(static_cast<size_t>(kAdaptiveFilterLength),
                           delay.value_or(kAdaptiveFilterLength)),
                  aec_state.ReverbDecayFactor(), R2);
  }

  // If the echo is saturated, estimate the echo power as the maximum echo power
  // with a leakage factor.
  if (aec_state.SaturatedEcho()) {
    R2->fill((*std::max_element(R2->begin(), R2->end())) * 100.f);
  }

  std::copy(R2->begin(), R2->end(), R2_old_.begin());
}

void ResidualEchoEstimator::Reset() {
  X2_noise_floor_counter_.fill(kNoiseFloorCounterMax);
  X2_noise_floor_.fill(kNoiseFloorMin);
  R2_reverb_.fill(0.f);
  R2_old_.fill(0.f);
  R2_hold_counter_.fill(0.f);
  for (auto& S2_k : S2_old_) {
    S2_k.fill(0.f);
  }
}

void ResidualEchoEstimator::LinearEstimate(
    const std::array<float, kFftLengthBy2Plus1>& S2_linear,
    const std::array<float, kFftLengthBy2Plus1>& erle,
    size_t delay,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  std::fill(R2_hold_counter_.begin(), R2_hold_counter_.end(), 10.f);
  std::transform(erle.begin(), erle.end(), S2_linear.begin(), R2->begin(),
                 [](float a, float b) {
                   RTC_DCHECK_LT(0.f, a);
                   return b / a;
                 });
}

void ResidualEchoEstimator::NonLinearEstimate(
    float echo_path_gain,
    const std::array<float, kFftLengthBy2Plus1>& X2,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  // Compute preliminary residual echo.
  std::transform(X2.begin(), X2.end(), R2->begin(),
                 [echo_path_gain](float a) { return a * echo_path_gain; });

  for (size_t k = 0; k < R2->size(); ++k) {
    // Update hold counter.
    R2_hold_counter_[k] = R2_old_[k] < (*R2)[k] ? 0 : R2_hold_counter_[k] + 1;

    // Compute the residual echo by holding a maximum echo powers and an echo
    // fading corresponding to a room with an RT60 value of about 50 ms.
    (*R2)[k] = R2_hold_counter_[k] < 2
                   ? std::max((*R2)[k], R2_old_[k])
                   : std::min((*R2)[k] + R2_old_[k] * 0.1f, Y2[k]);
  }
}

void ResidualEchoEstimator::AddEchoReverb(
    const std::array<float, kFftLengthBy2Plus1>& S2,
    bool saturated_echo,
    size_t delay,
    float reverb_decay_factor,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  // Compute the decay factor for how much the echo has decayed before leaving
  // the region covered by the linear model.
  auto integer_power = [](float base, int exp) {
    float result = 1.f;
    for (int k = 0; k < exp; ++k) {
      result *= base;
    }
    return result;
  };
  RTC_DCHECK_LE(delay, S2_old_.size());
  const float reverb_decay_for_delay =
      integer_power(reverb_decay_factor, S2_old_.size() - delay);

  // Update the estimate of the reverberant residual echo power.
  S2_old_index_ = S2_old_index_ > 0 ? S2_old_index_ - 1 : S2_old_.size() - 1;
  const auto& S2_end = S2_old_[S2_old_index_];
  std::transform(
      S2_end.begin(), S2_end.end(), R2_reverb_.begin(), R2_reverb_.begin(),
      [reverb_decay_for_delay, reverb_decay_factor](float a, float b) {
        return (b + a * reverb_decay_for_delay) * reverb_decay_factor;
      });

  // Update the buffer of old echo powers.
  if (saturated_echo) {
    S2_old_[S2_old_index_].fill((*std::max_element(S2.begin(), S2.end())) *
                                100.f);
  } else {
    std::copy(S2.begin(), S2.end(), S2_old_[S2_old_index_].begin());
  }

  // Add the power of the echo reverb to the residual echo power.
  std::transform(R2->begin(), R2->end(), R2_reverb_.begin(), R2->begin(),
                 std::plus<float>());
}

}  // namespace webrtc
