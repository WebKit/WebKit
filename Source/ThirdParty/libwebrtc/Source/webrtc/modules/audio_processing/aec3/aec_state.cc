/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec_state.h"

#include <math.h>

#include <numeric>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Computes delay of the adaptive filter.
int EstimateFilterDelay(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response) {
  const auto& H2 = adaptive_filter_frequency_response;
  constexpr size_t kUpperBin = kFftLengthBy2 - 5;
  RTC_DCHECK_GE(kAdaptiveFilterLength, H2.size());
  std::array<int, kAdaptiveFilterLength> delays;
  delays.fill(0);
  for (size_t k = 1; k < kUpperBin; ++k) {
    // Find the maximum of H2[j].
    size_t peak = 0;
    for (size_t j = 0; j < H2.size(); ++j) {
      if (H2[j][k] > H2[peak][k]) {
        peak = j;
      }
    }
    ++delays[peak];
  }

  return std::distance(delays.begin(),
                       std::max_element(delays.begin(), delays.end()));
}

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      erle_estimator_(config.erle.min, config.erle.max_l, config.erle.max_h),
      config_(config),
      reverb_decay_(config_.ep_strength.default_len) {
  max_render_.fill(0.f);
}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  if (echo_path_variability.AudioPathChanged()) {
    blocks_since_last_saturation_ = kUnknownDelayRenderWindowSize + 1;
    usable_linear_estimate_ = false;
    echo_leakage_detected_ = false;
    capture_signal_saturation_ = false;
    echo_saturation_ = false;
    max_render_.fill(0.f);

    if (echo_path_variability.delay_change) {
      force_zero_gain_counter_ = 0;
      blocks_with_filter_adaptation_ = 0;
      blocks_with_strong_render_ = 0;
      initial_state_ = true;
      linear_echo_estimate_ = false;
      sufficient_filter_updates_ = false;
      render_received_ = false;
      force_zero_gain_ = true;
      capture_block_counter_ = 0;
    }
    if (echo_path_variability.gain_change) {
      capture_block_counter_ = kNumBlocksPerSecond;
    }
  }
}

void AecState::Update(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                          adaptive_filter_frequency_response,
                      const std::array<float, kAdaptiveFilterTimeDomainLength>&
                          adaptive_filter_impulse_response,
                      bool converged_filter,
                      const rtc::Optional<size_t>& external_delay_samples,
                      const RenderBuffer& render_buffer,
                      const std::array<float, kFftLengthBy2Plus1>& E2_main,
                      const std::array<float, kFftLengthBy2Plus1>& Y2,
                      rtc::ArrayView<const float> x,
                      const std::array<float, kBlockSize>& s,
                      bool echo_leakage_detected) {
  // Store input parameters.
  echo_leakage_detected_ = echo_leakage_detected;

  // Update counters.
  ++capture_block_counter_;

  // Force zero echo suppression gain after an echo path change to allow at
  // least some render data to be collected in order to avoid an initial echo
  // burst.
  force_zero_gain_ = (++force_zero_gain_counter_) < kNumBlocksPerSecond / 5;

  // Estimate delays.
  filter_delay_ = EstimateFilterDelay(adaptive_filter_frequency_response);
  external_delay_ =
      external_delay_samples
          ? rtc::Optional<size_t>(*external_delay_samples / kBlockSize)
          : rtc::nullopt;

  // Update the ERL and ERLE measures.
  if (converged_filter && capture_block_counter_ >= 2 * kNumBlocksPerSecond) {
    const auto& X2 = render_buffer.Spectrum(*filter_delay_);
    erle_estimator_.Update(X2, Y2, E2_main);
    erl_estimator_.Update(X2, Y2);
  }

  // Update the echo audibility evaluator.
  echo_audibility_.Update(x, s, converged_filter);


  if (config_.ep_strength.echo_can_saturate) {
    // Detect and flag echo saturation.
    RTC_DCHECK_LT(0, x.size());
    // Store the render values in a circular buffer.
    max_render_index_ = (max_render_index_ + 1) % max_render_.size();
    auto x_max_result = std::minmax_element(x.begin(), x.end());
    max_render_[max_render_index_] =
        std::max(fabs(*x_max_result.first), fabs(*x_max_result.second));

    bool saturated_echo = false;
    // Check for whether a saturated frame potentially could consist of
    // saturated echo.
    if (SaturatedCapture()) {
      if (converged_filter) {
        RTC_DCHECK(filter_delay_);
        const size_t index =
            (max_render_index_ + max_render_.size() - *filter_delay_) %
            max_render_.size();
        saturated_echo = max_render_[index] > 200.f;
      } else {
        saturated_echo =
            *std::max_element(max_render_.begin(), max_render_.end()) > 200.f;
      }
    }

    // Set flag for potential presence of saturated echo
    blocks_since_last_saturation_ =
        saturated_echo ? 0 : blocks_since_last_saturation_ + 1;
    if (converged_filter) {
      echo_saturation_ =
          blocks_since_last_saturation_ < kAdaptiveFilterLength + 1;
    } else {
      echo_saturation_ =
          blocks_since_last_saturation_ < kUnknownDelayRenderWindowSize + 1;
    }

    // Set flag for whether the echo path is generally strong enough to saturate
    // the echo.
    if (converged_filter) {
      // Base detection on predicted echo sample.
      auto s_max_result = std::minmax_element(s.begin(), s.end());
      const float s_max_abs =
          std::max(fabs(*s_max_result.first), fabs(*s_max_result.second));

      const bool saturated_echo_sample =
          s_max_abs >= 10000.f && SaturatedCapture();
      saturating_echo_path_counter_ = saturated_echo_sample
                                          ? 10 * kNumBlocksPerSecond
                                          : saturating_echo_path_counter_ - 1;
    } else {
      // Base detection on detected potentially echo.
      saturating_echo_path_counter_ = saturated_echo
                                          ? 10 * kNumBlocksPerSecond
                                          : saturating_echo_path_counter_ - 1;
    }
    saturating_echo_path_counter_ = std::max(0, saturating_echo_path_counter_);
    saturating_echo_path_ = saturating_echo_path_counter_ > 0;
  } else {
    echo_saturation_ = false;
    saturating_echo_path_ = false;
    saturating_echo_path_counter_ = 0;
  }

  // Compute render energies.
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  const bool active_render_block =
      x_energy > (config_.render_levels.active_render_limit *
                  config_.render_levels.active_render_limit) *
                     kFftLengthBy2;
  const bool strong_render_block = x_energy > 1000 * 1000 * kFftLengthBy2;

  if (active_render_block) {
    render_received_ = true;
  }

  // Update counters.
  blocks_with_filter_adaptation_ +=
      (active_render_block && (!SaturatedCapture()) ? 1 : 0);

  blocks_with_strong_render_ +=
      (strong_render_block && (!SaturatedCapture()) ? 1 : 0);

  // After an amount of active render samples for which an echo should have been
  // detected in the capture signal if the ERL was not infinite, flag that a
  // transparent mode should be entered.
  if (SaturatingEchoPath()) {
    transparent_mode_ = !converged_filter &&
                        (!render_received_ || blocks_with_strong_render_ >=
                                                  15 * kNumBlocksPerSecond);
  } else {
    transparent_mode_ = !converged_filter &&
                        (!render_received_ ||
                         blocks_with_strong_render_ >= 5 * kNumBlocksPerSecond);
  }

  // Update flag for whether the adaptation is in the initial state.
  if (SaturatingEchoPath()) {
    initial_state_ = capture_block_counter_ < 6 * kNumBlocksPerSecond;
  } else {
    initial_state_ = capture_block_counter_ < 3 * kNumBlocksPerSecond;
  }

  // Detect whether the linear filter is usable.
  if (SaturatingEchoPath()) {
    usable_linear_estimate_ =
        (!echo_saturation_) &&
        (converged_filter && SufficientFilterUpdates()) &&
        capture_block_counter_ >= 5 * kNumBlocksPerSecond && external_delay_;
  } else {
    usable_linear_estimate_ =
        (!echo_saturation_) &&
        (converged_filter || SufficientFilterUpdates()) &&
        capture_block_counter_ >= 2 * kNumBlocksPerSecond && external_delay_;
  }

  // Flag whether the linear echo estimate should be used.
  linear_echo_estimate_ = usable_linear_estimate_ && !TransparentMode();

  // Flag whether a sufficient number of filter updates has been done for the
  // filter to perform well.
  if (SaturatingEchoPath()) {
    sufficient_filter_updates_ =
        blocks_with_filter_adaptation_ >= 2 * kEchoPathChangeConvergenceBlocks;
  } else {
    sufficient_filter_updates_ =
        blocks_with_filter_adaptation_ >= kEchoPathChangeConvergenceBlocks;
  }

  // Update the room reverb estimate.
  UpdateReverb(adaptive_filter_impulse_response);
}

void AecState::UpdateReverb(
    const std::array<float, kAdaptiveFilterTimeDomainLength>&
        impulse_response) {
  if ((!(filter_delay_ && usable_linear_estimate_)) ||
      (*filter_delay_ > kAdaptiveFilterLength - 4)) {
    return;
  }

  // Form the data to match against by squaring the impulse response
  // coefficients.
  std::array<float, kAdaptiveFilterTimeDomainLength> matching_data;
  std::transform(impulse_response.begin(), impulse_response.end(),
                 matching_data.begin(), [](float a) { return a * a; });

  // Avoid matching against noise in the model by subtracting an estimate of the
  // model noise power.
  constexpr size_t kTailLength = 64;
  constexpr size_t tail_index = kAdaptiveFilterTimeDomainLength - kTailLength;
  const float tail_power = *std::max_element(matching_data.begin() + tail_index,
                                             matching_data.end());
  std::for_each(matching_data.begin(), matching_data.begin() + tail_index,
                [tail_power](float& a) { a = std::max(0.f, a - tail_power); });

  // Identify the peak index of the impulse response.
  const size_t peak_index = *std::max_element(
      matching_data.begin(), matching_data.begin() + tail_index);

  if (peak_index + 128 < tail_index) {
    size_t start_index = peak_index + 64;
    // Compute the matching residual error for the current candidate to match.
    float residual_sqr_sum = 0.f;
    float d_k = reverb_decay_to_test_;
    for (size_t k = start_index; k < tail_index; ++k) {
      if (matching_data[start_index + 1] == 0.f) {
        break;
      }

      float residual = matching_data[k] - matching_data[peak_index] * d_k;
      residual_sqr_sum += residual * residual;
      d_k *= reverb_decay_to_test_;
    }

    // If needed, update the best candidate for the reverb decay.
    if (reverb_decay_candidate_residual_ < 0.f ||
        residual_sqr_sum < reverb_decay_candidate_residual_) {
      reverb_decay_candidate_residual_ = residual_sqr_sum;
      reverb_decay_candidate_ = reverb_decay_to_test_;
    }
  }

  // Compute the next reverb candidate to evaluate such that all candidates will
  // be evaluated within one second.
  reverb_decay_to_test_ += (0.9965f - 0.9f) / (5 * kNumBlocksPerSecond);

  // If all reverb candidates have been evaluated, choose the best one as the
  // reverb decay.
  if (reverb_decay_to_test_ >= 0.9965f) {
    if (reverb_decay_candidate_residual_ < 0.f) {
      // Transform the decay to be in the unit of blocks.
      reverb_decay_ = powf(reverb_decay_candidate_, kFftLengthBy2);

      // Limit the estimated reverb_decay_ to the maximum one needed in practice
      // to minimize the impact of incorrect estimates.
      reverb_decay_ = std::min(config_.ep_strength.default_len, reverb_decay_);
    }
    reverb_decay_to_test_ = 0.9f;
    reverb_decay_candidate_residual_ = -1.f;
  }

  // For noisy impulse responses, assume a fixed tail length.
  if (tail_power > 0.0005f) {
    reverb_decay_ = config_.ep_strength.default_len;
  }
  data_dumper_->DumpRaw("aec3_reverb_decay", reverb_decay_);
  data_dumper_->DumpRaw("aec3_tail_power", tail_power);
}

void AecState::EchoAudibility::Update(rtc::ArrayView<const float> x,
                                      const std::array<float, kBlockSize>& s,
                                      bool converged_filter) {
  auto result_x = std::minmax_element(x.begin(), x.end());
  auto result_s = std::minmax_element(s.begin(), s.end());
  const float x_abs =
      std::max(fabsf(*result_x.first), fabsf(*result_x.second));
  const float s_abs =
      std::max(fabsf(*result_s.first), fabsf(*result_s.second));

  if (converged_filter) {
    if (x_abs < 20.f) {
      ++low_farend_counter_;
    } else {
      low_farend_counter_ = 0;
    }
  } else {
    if (x_abs < 100.f) {
      ++low_farend_counter_;
    } else {
      low_farend_counter_ = 0;
    }
  }

  // The echo is deemed as not audible if the echo estimate is on the level of
  // the quantization noise in the FFTs and the nearend level is sufficiently
  // strong to mask that by ensuring that the playout and AGC gains do not boost
  // any residual echo that is below the quantization noise level. Furthermore,
  // cases where the render signal is very close to zero are also identified as
  // not producing audible echo.
  inaudible_echo_ = (max_nearend_ > 500 && s_abs < 30.f) ||
                    (!converged_filter && x_abs < 500);
  inaudible_echo_ = inaudible_echo_ || low_farend_counter_ > 20;
}

void AecState::EchoAudibility::UpdateWithOutput(rtc::ArrayView<const float> e) {
  const float e_max = *std::max_element(e.begin(), e.end());
  const float e_min = *std::min_element(e.begin(), e.end());
  const float e_abs = std::max(fabsf(e_max), fabsf(e_min));

  if (max_nearend_ < e_abs) {
    max_nearend_ = e_abs;
    max_nearend_counter_ = 0;
  } else {
    if (++max_nearend_counter_ > 5 * kNumBlocksPerSecond) {
      max_nearend_ *= 0.995f;
    }
  }
}

}  // namespace webrtc
