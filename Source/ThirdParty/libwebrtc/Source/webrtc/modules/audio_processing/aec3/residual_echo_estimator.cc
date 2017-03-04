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

#include <math.h>
#include <vector>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace {

constexpr float kSaturationLeakageFactor = 10.f;
constexpr size_t kSaturationLeakageBlocks = 10;
constexpr size_t kEchoPathChangeConvergenceBlocks = 3 * 250;

// Estimates the residual echo power when there is no detection correlation
// between the render and capture signals.
void InfiniteErlPowerEstimate(
    size_t active_render_blocks,
    size_t blocks_since_last_saturation,
    const std::array<float, kFftLengthBy2Plus1>& S2_fallback,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  if (active_render_blocks > 20 * 250) {
    // After an amount of active render samples for which an echo should have
    // been detected in the capture signal if the ERL was not infinite, set the
    // residual echo to 0.
    R2->fill(0.f);
  } else {
    // Before certainty has been reached about the presence of echo, use the
    // fallback echo power estimate as the residual echo estimate. Add a leakage
    // factor when there is saturation.
    std::copy(S2_fallback.begin(), S2_fallback.end(), R2->begin());
    if (blocks_since_last_saturation < kSaturationLeakageBlocks) {
      std::for_each(R2->begin(), R2->end(),
                    [](float& a) { a *= kSaturationLeakageFactor; });
    }
  }
}

// Estimates the echo power in an half-duplex manner.
void HalfDuplexPowerEstimate(bool active_render,
                             const std::array<float, kFftLengthBy2Plus1>& Y2,
                             std::array<float, kFftLengthBy2Plus1>* R2) {
  // Set the residual echo power to the power of the capture signal.
  if (active_render) {
    std::copy(Y2.begin(), Y2.end(), R2->begin());
  } else {
    R2->fill(0.f);
  }
}

// Estimates the residual echo power based on gains.
void GainBasedPowerEstimate(
    size_t external_delay,
    const FftBuffer& X_buffer,
    size_t blocks_since_last_saturation,
    size_t active_render_blocks,
    const std::array<bool, kFftLengthBy2Plus1>& bands_with_reliable_filter,
    const std::array<float, kFftLengthBy2Plus1>& echo_path_gain,
    const std::array<float, kFftLengthBy2Plus1>& S2_fallback,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  const auto& X2 = X_buffer.Spectrum(external_delay);

  // Base the residual echo power on gain of the linear echo path estimate if
  // that is reliable, otherwise use the fallback echo path estimate. Add a
  // leakage factor when there is saturation.
  if (active_render_blocks > kEchoPathChangeConvergenceBlocks) {
    for (size_t k = 0; k < R2->size(); ++k) {
      (*R2)[k] = bands_with_reliable_filter[k] ? echo_path_gain[k] * X2[k]
                                               : S2_fallback[k];
    }
  } else {
    for (size_t k = 0; k < R2->size(); ++k) {
      (*R2)[k] = S2_fallback[k];
    }
  }

  if (blocks_since_last_saturation < kSaturationLeakageBlocks) {
    std::for_each(R2->begin(), R2->end(),
                  [](float& a) { a *= kSaturationLeakageFactor; });
  }
}

// Estimates the residual echo power based on the linear echo path.
void ErleBasedPowerEstimate(
    bool headset_detected,
    const FftBuffer& X_buffer,
    bool using_subtractor_output,
    size_t linear_filter_based_delay,
    size_t blocks_since_last_saturation,
    bool poorly_aligned_filter,
    const std::array<bool, kFftLengthBy2Plus1>& bands_with_reliable_filter,
    const std::array<float, kFftLengthBy2Plus1>& echo_path_gain,
    const std::array<float, kFftLengthBy2Plus1>& S2_fallback,
    const std::array<float, kFftLengthBy2Plus1>& S2_linear,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const std::array<float, kFftLengthBy2Plus1>& erle,
    const std::array<float, kFftLengthBy2Plus1>& erl,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  // Residual echo power after saturation.
  if (blocks_since_last_saturation < kSaturationLeakageBlocks) {
    for (size_t k = 0; k < R2->size(); ++k) {
      (*R2)[k] = kSaturationLeakageFactor *
                 (bands_with_reliable_filter[k] && using_subtractor_output
                      ? S2_linear[k]
                      : std::min(S2_fallback[k], Y2[k]));
    }
    return;
  }

  // Residual echo power when a headset is used.
  if (headset_detected) {
    const auto& X2 = X_buffer.Spectrum(linear_filter_based_delay);
    for (size_t k = 0; k < R2->size(); ++k) {
      RTC_DCHECK_LT(0.f, erle[k]);
      (*R2)[k] = bands_with_reliable_filter[k] && using_subtractor_output
                     ? S2_linear[k] / erle[k]
                     : std::min(S2_fallback[k], Y2[k]);
      (*R2)[k] = std::min((*R2)[k], X2[k] * erl[k]);
    }
    return;
  }

  // Residual echo power when the adaptive filter is poorly aligned.
  if (poorly_aligned_filter) {
    for (size_t k = 0; k < R2->size(); ++k) {
      (*R2)[k] = bands_with_reliable_filter[k] && using_subtractor_output
                     ? S2_linear[k]
                     : std::min(S2_fallback[k], Y2[k]);
    }
    return;
  }

  // Residual echo power when there is no recent saturation, no headset detected
  // and when the adaptive filter is well aligned.
  for (size_t k = 0; k < R2->size(); ++k) {
    RTC_DCHECK_LT(0.f, erle[k]);
    const auto& X2 = X_buffer.Spectrum(linear_filter_based_delay);
    (*R2)[k] = bands_with_reliable_filter[k] && using_subtractor_output
                   ? S2_linear[k] / erle[k]
                   : std::min(echo_path_gain[k] * X2[k], Y2[k]);
  }
}

}  // namespace

ResidualEchoEstimator::ResidualEchoEstimator() {
  echo_path_gain_.fill(100.f);
}

ResidualEchoEstimator::~ResidualEchoEstimator() = default;

void ResidualEchoEstimator::Estimate(
    bool using_subtractor_output,
    const AecState& aec_state,
    const FftBuffer& X_buffer,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>& H2,
    const std::array<float, kFftLengthBy2Plus1>& E2_main,
    const std::array<float, kFftLengthBy2Plus1>& E2_shadow,
    const std::array<float, kFftLengthBy2Plus1>& S2_linear,
    const std::array<float, kFftLengthBy2Plus1>& S2_fallback,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    std::array<float, kFftLengthBy2Plus1>* R2) {
  RTC_DCHECK(R2);
  const rtc::Optional<size_t>& linear_filter_based_delay =
      aec_state.FilterDelay();

  // Update the echo path gain.
  if (linear_filter_based_delay) {
    std::copy(H2[*linear_filter_based_delay].begin(),
              H2[*linear_filter_based_delay].end(), echo_path_gain_.begin());
    constexpr float kEchoPathGainHeadroom = 10.f;
    std::for_each(
        echo_path_gain_.begin(), echo_path_gain_.end(),
        [kEchoPathGainHeadroom](float& a) { a *= kEchoPathGainHeadroom; });
  }

  // Counts the blocks since saturation.
  if (aec_state.SaturatedCapture()) {
    blocks_since_last_saturation_ = 0;
  } else {
    ++blocks_since_last_saturation_;
  }

  const auto& bands_with_reliable_filter = aec_state.BandsWithReliableFilter();

  if (aec_state.UsableLinearEstimate()) {
    // Residual echo power estimation when the adaptive filter is reliable.
    RTC_DCHECK(linear_filter_based_delay);
    ErleBasedPowerEstimate(
        aec_state.HeadsetDetected(), X_buffer, using_subtractor_output,
        *linear_filter_based_delay, blocks_since_last_saturation_,
        aec_state.PoorlyAlignedFilter(), bands_with_reliable_filter,
        echo_path_gain_, S2_fallback, S2_linear, Y2, aec_state.Erle(),
        aec_state.Erl(), R2);
  } else if (aec_state.ModelBasedAecFeasible()) {
    // Residual echo power when the adaptive filter is not reliable but still an
    // external echo path delay is provided (and hence can be estimated).
    RTC_DCHECK(aec_state.ExternalDelay());
    GainBasedPowerEstimate(
        *aec_state.ExternalDelay(), X_buffer, blocks_since_last_saturation_,
        aec_state.ActiveRenderBlocks(), bands_with_reliable_filter,
        echo_path_gain_, S2_fallback, R2);
  } else if (aec_state.EchoLeakageDetected()) {
    // Residual echo power when an external residual echo detection algorithm
    // has deemed the echo canceller to leak echoes.
    HalfDuplexPowerEstimate(aec_state.ActiveRender(), Y2, R2);
  } else {
    // Residual echo power when none of the other cases are fulfilled.
    InfiniteErlPowerEstimate(aec_state.ActiveRenderBlocks(),
                             blocks_since_last_saturation_, S2_fallback, R2);
  }
}

void ResidualEchoEstimator::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  if (echo_path_variability.AudioPathChanged()) {
    blocks_since_last_saturation_ = 0;
    echo_path_gain_.fill(100.f);
  }
}

}  // namespace webrtc
