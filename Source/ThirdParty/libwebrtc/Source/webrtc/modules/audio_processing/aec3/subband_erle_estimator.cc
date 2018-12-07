/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/subband_erle_estimator.h"

#include <algorithm>
#include <functional>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

constexpr float kX2BandEnergyThreshold = 44015068.0f;
constexpr int kBlocksToHoldErle = 100;
constexpr int kBlocksForOnsetDetection = kBlocksToHoldErle + 150;
constexpr int kPointsToAccumulate = 6;

bool EnableAdaptErleOnLowRender() {
  return !field_trial::IsEnabled("WebRTC-Aec3AdaptErleOnLowRenderKillSwitch");
}

std::array<float, kFftLengthBy2Plus1> SetMaxErleBands(float max_erle_l,
                                                      float max_erle_h) {
  std::array<float, kFftLengthBy2Plus1> max_erle;
  std::fill(max_erle.begin(), max_erle.begin() + kFftLengthBy2 / 2, max_erle_l);
  std::fill(max_erle.begin() + kFftLengthBy2 / 2, max_erle.end(), max_erle_h);
  return max_erle;
}

}  // namespace

SubbandErleEstimator::SubbandErleEstimator(const EchoCanceller3Config& config)
    : min_erle_(config.erle.min),
      max_erle_(SetMaxErleBands(config.erle.max_l, config.erle.max_h)),
      adapt_on_low_render_(EnableAdaptErleOnLowRender()) {
  Reset();
}

SubbandErleEstimator::~SubbandErleEstimator() = default;

void SubbandErleEstimator::Reset() {
  erle_.fill(min_erle_);
  erle_onsets_.fill(min_erle_);
  coming_onset_.fill(true);
  hold_counters_.fill(0);
  ResetAccumulatedSpectra();
}

void SubbandErleEstimator::Update(rtc::ArrayView<const float> X2,
                                  rtc::ArrayView<const float> Y2,
                                  rtc::ArrayView<const float> E2,
                                  bool converged_filter,
                                  bool onset_detection) {
  if (converged_filter) {
    // Note that the use of the converged_filter flag already imposed
    // a minimum of the erle that can be estimated as that flag would
    // be false if the filter is performing poorly.
    UpdateAccumulatedSpectra(X2, Y2, E2);
    UpdateBands(onset_detection);
  }

  if (onset_detection) {
    DecreaseErlePerBandForLowRenderSignals();
  }

  erle_[0] = erle_[1];
  erle_[kFftLengthBy2] = erle_[kFftLengthBy2 - 1];
}

void SubbandErleEstimator::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  data_dumper->DumpRaw("aec3_erle_onset", ErleOnsets());
}

void SubbandErleEstimator::UpdateBands(bool onset_detection) {
  std::array<float, kFftLengthBy2> new_erle;
  std::array<bool, kFftLengthBy2> is_erle_updated;
  is_erle_updated.fill(false);

  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    if (accum_spectra_.num_points_[k] == kPointsToAccumulate &&
        accum_spectra_.E2_[k] > 0.f) {
      new_erle[k] = accum_spectra_.Y2_[k] / accum_spectra_.E2_[k];
      is_erle_updated[k] = true;
    }
  }

  if (onset_detection) {
    for (size_t k = 1; k < kFftLengthBy2; ++k) {
      if (is_erle_updated[k] && !accum_spectra_.low_render_energy_[k]) {
        if (coming_onset_[k]) {
          coming_onset_[k] = false;
          float alpha = new_erle[k] < erle_onsets_[k] ? 0.3f : 0.15f;
          erle_onsets_[k] = rtc::SafeClamp(
              erle_onsets_[k] + alpha * (new_erle[k] - erle_onsets_[k]),
              min_erle_, max_erle_[k]);
        }
        hold_counters_[k] = kBlocksForOnsetDetection;
      }
    }
  }

  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    if (is_erle_updated[k]) {
      float alpha = 0.05f;
      if (new_erle[k] < erle_[k]) {
        alpha = accum_spectra_.low_render_energy_[k] ? 0.f : 0.1f;
      }
      erle_[k] = rtc::SafeClamp(erle_[k] + alpha * (new_erle[k] - erle_[k]),
                                min_erle_, max_erle_[k]);
    }
  }
}

void SubbandErleEstimator::DecreaseErlePerBandForLowRenderSignals() {
  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    hold_counters_[k]--;
    if (hold_counters_[k] <= (kBlocksForOnsetDetection - kBlocksToHoldErle)) {
      if (erle_[k] > erle_onsets_[k]) {
        erle_[k] = std::max(erle_onsets_[k], 0.97f * erle_[k]);
        RTC_DCHECK_LE(min_erle_, erle_[k]);
      }
      if (hold_counters_[k] <= 0) {
        coming_onset_[k] = true;
        hold_counters_[k] = 0;
      }
    }
  }
}

void SubbandErleEstimator::ResetAccumulatedSpectra() {
  accum_spectra_.Y2_.fill(0.f);
  accum_spectra_.E2_.fill(0.f);
  accum_spectra_.num_points_.fill(0);
  accum_spectra_.low_render_energy_.fill(false);
}

void SubbandErleEstimator::UpdateAccumulatedSpectra(
    rtc::ArrayView<const float> X2,
    rtc::ArrayView<const float> Y2,
    rtc::ArrayView<const float> E2) {
  auto& st = accum_spectra_;
  if (adapt_on_low_render_) {
    if (st.num_points_[0] == kPointsToAccumulate) {
      st.num_points_[0] = 0;
      st.Y2_.fill(0.f);
      st.E2_.fill(0.f);
      st.low_render_energy_.fill(false);
    }
    std::transform(Y2.begin(), Y2.end(), st.Y2_.begin(), st.Y2_.begin(),
                   std::plus<float>());
    std::transform(E2.begin(), E2.end(), st.E2_.begin(), st.E2_.begin(),
                   std::plus<float>());

    for (size_t k = 0; k < X2.size(); ++k) {
      st.low_render_energy_[k] =
          st.low_render_energy_[k] || X2[k] < kX2BandEnergyThreshold;
    }
    st.num_points_[0]++;
    st.num_points_.fill(st.num_points_[0]);

  } else {
    // The update is always done using high render energy signals and
    // therefore the field accum_spectra_.low_render_energy_ does not need to
    // be modified.
    for (size_t k = 0; k < X2.size(); ++k) {
      if (X2[k] > kX2BandEnergyThreshold) {
        if (st.num_points_[k] == kPointsToAccumulate) {
          st.Y2_[k] = 0.f;
          st.E2_[k] = 0.f;
          st.num_points_[k] = 0;
        }
        st.Y2_[k] += Y2[k];
        st.E2_[k] += E2[k];
        st.num_points_[k]++;
      }
      RTC_DCHECK_EQ(st.low_render_energy_[k], false);
    }
  }
}

}  // namespace webrtc
