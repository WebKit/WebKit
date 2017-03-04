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

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace {

constexpr float kMaxFilterEstimateStrength = 1000.f;

// Compute the delay of the adaptive filter as the partition with a distinct
// peak.
void AnalyzeFilter(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        filter_frequency_response,
    std::array<bool, kFftLengthBy2Plus1>* bands_with_reliable_filter,
    std::array<float, kFftLengthBy2Plus1>* filter_estimate_strength,
    rtc::Optional<size_t>* filter_delay) {
  const auto& H2 = filter_frequency_response;

  size_t reliable_delays_sum = 0;
  size_t num_reliable_delays = 0;

  constexpr size_t kUpperBin = kFftLengthBy2 - 5;
  for (size_t k = 1; k < kUpperBin; ++k) {
    int peak = 0;
    for (size_t j = 0; j < H2.size(); ++j) {
      if (H2[j][k] > H2[peak][k]) {
        peak = j;
      }
    }

    if (H2[peak][k] == 0.f) {
      (*filter_estimate_strength)[k] = 0.f;
    } else if (H2[H2.size() - 1][k] == 0.f) {
      (*filter_estimate_strength)[k] = kMaxFilterEstimateStrength;
    } else {
      (*filter_estimate_strength)[k] = std::min(
          kMaxFilterEstimateStrength, H2[peak][k] / H2[H2.size() - 1][k]);
    }

    constexpr float kMargin = 10.f;
    if (kMargin * H2[H2.size() - 1][k] < H2[peak][k]) {
      (*bands_with_reliable_filter)[k] = true;
      reliable_delays_sum += peak;
      ++num_reliable_delays;
    } else {
      (*bands_with_reliable_filter)[k] = false;
    }
  }
  (*bands_with_reliable_filter)[0] = (*bands_with_reliable_filter)[1];
  std::fill(bands_with_reliable_filter->begin() + kUpperBin,
            bands_with_reliable_filter->end(),
            (*bands_with_reliable_filter)[kUpperBin - 1]);
  (*filter_estimate_strength)[0] = (*filter_estimate_strength)[1];
  std::fill(filter_estimate_strength->begin() + kUpperBin,
            filter_estimate_strength->end(),
            (*filter_estimate_strength)[kUpperBin - 1]);

  *filter_delay =
      num_reliable_delays > 20
          ? rtc::Optional<size_t>(reliable_delays_sum / num_reliable_delays)
          : rtc::Optional<size_t>();
}

constexpr int kActiveRenderCounterInitial = 50;
constexpr int kActiveRenderCounterMax = 200;
constexpr int kEchoPathChangeCounterInitial = 50;
constexpr int kEchoPathChangeCounterMax = 3 * 250;

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      echo_path_change_counter_(kEchoPathChangeCounterInitial),
      active_render_counter_(kActiveRenderCounterInitial) {
  bands_with_reliable_filter_.fill(false);
  filter_estimate_strength_.fill(0.f);
}

AecState::~AecState() = default;

void AecState::Update(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                          filter_frequency_response,
                      const rtc::Optional<size_t>& external_delay_samples,
                      const FftBuffer& X_buffer,
                      const std::array<float, kFftLengthBy2Plus1>& E2_main,
                      const std::array<float, kFftLengthBy2Plus1>& E2_shadow,
                      const std::array<float, kFftLengthBy2Plus1>& Y2,
                      rtc::ArrayView<const float> x,
                      const EchoPathVariability& echo_path_variability,
                      bool echo_leakage_detected) {
  filter_length_ = filter_frequency_response.size();
  AnalyzeFilter(filter_frequency_response, &bands_with_reliable_filter_,
                &filter_estimate_strength_, &filter_delay_);
  // Compute the externally provided delay in partitions. The truncation is
  // intended here.
  external_delay_ =
      external_delay_samples
          ? rtc::Optional<size_t>(*external_delay_samples / kBlockSize)
          : rtc::Optional<size_t>();

  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);

  active_render_blocks_ =
      echo_path_variability.AudioPathChanged() ? 0 : active_render_blocks_ + 1;

  echo_path_change_counter_ = echo_path_variability.AudioPathChanged()
                                  ? kEchoPathChangeCounterMax
                                  : echo_path_change_counter_ - 1;
  active_render_counter_ = x_energy > 10000.f * kFftLengthBy2
                               ? kActiveRenderCounterMax
                               : active_render_counter_ - 1;

  usable_linear_estimate_ = filter_delay_ && echo_path_change_counter_ <= 0;

  echo_leakage_detected_ = echo_leakage_detected;

  model_based_aec_feasible_ = usable_linear_estimate_ || external_delay_;

  if (usable_linear_estimate_) {
    const auto& X2 = X_buffer.Spectrum(*filter_delay_);

    // TODO(peah): Expose these as stats.
    erle_estimator_.Update(X2, Y2, E2_main);
    erl_estimator_.Update(X2, Y2);

// TODO(peah): Add working functionality for headset detection. Until the
// functionality for that is working the headset detector is hardcoded to detect
// no headset.
#if 0
    const auto& erl = erl_estimator_.Erl();
    const int low_erl_band_count = std::count_if(
        erl.begin(), erl.end(), [](float a) { return a <= 0.1f; });

    const int noisy_band_count = std::count_if(
        filter_estimate_strength_.begin(), filter_estimate_strength_.end(),
        [](float a) { return a <= 10.f; });
    headset_detected_ = low_erl_band_count > 20 && noisy_band_count > 20;
#endif
    headset_detected_ = false;
  } else {
    headset_detected_ = false;
  }
}

}  // namespace webrtc
