/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/aec_state.h"

#include <math.h>
#include <numeric>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace {

constexpr size_t kEchoPathChangeConvergenceBlocks = 2 * kNumBlocksPerSecond;
constexpr size_t kSaturationLeakageBlocks = 20;

// Computes delay of the adaptive filter.
rtc::Optional<size_t> EstimateFilterDelay(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response) {
  const auto& H2 = adaptive_filter_frequency_response;

  size_t reliable_delays_sum = 0;
  size_t num_reliable_delays = 0;

  constexpr size_t kUpperBin = kFftLengthBy2 - 5;
  constexpr float kMinPeakMargin = 10.f;
  const size_t kTailPartition = H2.size() - 1;
  for (size_t k = 1; k < kUpperBin; ++k) {
    // Find the maximum of H2[j].
    int peak = 0;
    for (size_t j = 0; j < H2.size(); ++j) {
      if (H2[j][k] > H2[peak][k]) {
        peak = j;
      }
    }

    // Count the peak as a delay only if the peak is sufficiently larger than
    // the tail.
    if (kMinPeakMargin * H2[kTailPartition][k] < H2[peak][k]) {
      reliable_delays_sum += peak;
      ++num_reliable_delays;
    }
  }

  // Return no delay if not sufficient delays have been found.
  if (num_reliable_delays < 21) {
    return rtc::Optional<size_t>();
  }

  const size_t delay = reliable_delays_sum / num_reliable_delays;
  // Sanity check that the peak is not caused by a false strong DC-component in
  // the filter.
  for (size_t k = 1; k < kUpperBin; ++k) {
    if (H2[delay][k] > H2[delay][0]) {
      RTC_DCHECK_GT(H2.size(), delay);
      return rtc::Optional<size_t>(delay);
    }
  }
  return rtc::Optional<size_t>();
}

constexpr int kEchoPathChangeCounterInitial = kNumBlocksPerSecond / 5;
constexpr int kEchoPathChangeCounterMax = 2 * kNumBlocksPerSecond;

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      echo_path_change_counter_(kEchoPathChangeCounterInitial) {}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  if (echo_path_variability.AudioPathChanged()) {
    blocks_since_last_saturation_ = 0;
    usable_linear_estimate_ = false;
    echo_leakage_detected_ = false;
    capture_signal_saturation_ = false;
    echo_saturation_ = false;
    previous_max_sample_ = 0.f;

    if (echo_path_variability.delay_change) {
      force_zero_gain_counter_ = 0;
      blocks_with_filter_adaptation_ = 0;
      render_received_ = false;
      force_zero_gain_ = true;
      echo_path_change_counter_ = kEchoPathChangeCounterMax;
    }
    if (echo_path_variability.gain_change) {
      echo_path_change_counter_ = kEchoPathChangeCounterInitial;
    }
  }
}

void AecState::Update(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                          adaptive_filter_frequency_response,
                      const rtc::Optional<size_t>& external_delay_samples,
                      const RenderBuffer& render_buffer,
                      const std::array<float, kFftLengthBy2Plus1>& E2_main,
                      const std::array<float, kFftLengthBy2Plus1>& Y2,
                      rtc::ArrayView<const float> x,
                      bool echo_leakage_detected) {
  // Store input parameters.
  echo_leakage_detected_ = echo_leakage_detected;

  // Update counters.
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  const bool active_render_block = x_energy > 10000.f * kFftLengthBy2;
  if (active_render_block) {
    render_received_ = true;
  }
  blocks_with_filter_adaptation_ +=
      (active_render_block && (!SaturatedCapture()) ? 1 : 0);
  --echo_path_change_counter_;

  // Force zero echo suppression gain after an echo path change to allow at
  // least some render data to be collected in order to avoid an initial echo
  // burst.
  constexpr size_t kZeroGainBlocksAfterChange = kNumBlocksPerSecond / 5;
  force_zero_gain_ = (++force_zero_gain_counter_) < kZeroGainBlocksAfterChange;

  // Estimate delays.
  filter_delay_ = EstimateFilterDelay(adaptive_filter_frequency_response);
  external_delay_ =
      external_delay_samples
          ? rtc::Optional<size_t>(*external_delay_samples / kBlockSize)
          : rtc::Optional<size_t>();

  // Update the ERL and ERLE measures.
  if (filter_delay_ && echo_path_change_counter_ <= 0) {
    const auto& X2 = render_buffer.Spectrum(*filter_delay_);
    erle_estimator_.Update(X2, Y2, E2_main);
    erl_estimator_.Update(X2, Y2);
  }

  // Detect and flag echo saturation.
  // TODO(peah): Add the delay in this computation to ensure that the render and
  // capture signals are properly aligned.
  RTC_DCHECK_LT(0, x.size());
  const float max_sample = fabs(*std::max_element(
      x.begin(), x.end(), [](float a, float b) { return a * a < b * b; }));
  const bool saturated_echo =
      previous_max_sample_ * kFixedEchoPathGain > 1600 && SaturatedCapture();
  previous_max_sample_ = max_sample;

  // Counts the blocks since saturation.
  blocks_since_last_saturation_ =
      saturated_echo ? 0 : blocks_since_last_saturation_ + 1;
  echo_saturation_ = blocks_since_last_saturation_ < kSaturationLeakageBlocks;

  // Flag whether the linear filter estimate is usable.
  usable_linear_estimate_ =
      (!echo_saturation_) &&
      (!render_received_ ||
       blocks_with_filter_adaptation_ > kEchoPathChangeConvergenceBlocks) &&
      filter_delay_ && echo_path_change_counter_ <= 0;

  // After an amount of active render samples for which an echo should have been
  // detected in the capture signal if the ERL was not infinite, flag that a
  // headset is used.
  headset_detected_ =
      !external_delay_ && !filter_delay_ &&
      (!render_received_ ||
       blocks_with_filter_adaptation_ >= kEchoPathChangeConvergenceBlocks);
}

}  // namespace webrtc
