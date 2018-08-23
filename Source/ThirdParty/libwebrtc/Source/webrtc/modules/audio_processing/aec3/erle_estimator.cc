/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erle_estimator.h"

#include <algorithm>
#include <numeric>

#include "absl/types/optional.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {
constexpr int kPointsToAccumulate = 6;
constexpr float kEpsilon = 1e-3f;
}  // namespace

ErleEstimator::ErleEstimator(float min_erle,
                             float max_erle_lf,
                             float max_erle_hf)
    : min_erle_(min_erle),
      min_erle_log2_(FastApproxLog2f(min_erle_ + kEpsilon)),
      max_erle_lf_(max_erle_lf),
      max_erle_lf_log2(FastApproxLog2f(max_erle_lf_ + kEpsilon)),
      max_erle_hf_(max_erle_hf),
      erle_freq_inst_(kPointsToAccumulate),
      erle_time_inst_(kPointsToAccumulate) {
  erle_.fill(min_erle_);
  erle_onsets_.fill(min_erle_);
  hold_counters_.fill(0);
  coming_onset_.fill(true);
  erle_time_domain_log2_ = min_erle_log2_;
  hold_counter_time_domain_ = 0;
}

ErleEstimator::~ErleEstimator() = default;

ErleEstimator::ErleTimeInstantaneous::ErleTimeInstantaneous(
    int points_to_accumulate)
    : points_to_accumulate_(points_to_accumulate) {
  Reset();
}
ErleEstimator::ErleTimeInstantaneous::~ErleTimeInstantaneous() = default;

bool ErleEstimator::ErleTimeInstantaneous::Update(const float Y2_sum,
                                                  const float E2_sum) {
  bool ret = false;
  E2_acum_ += E2_sum;
  Y2_acum_ += Y2_sum;
  num_points_++;
  if (num_points_ == points_to_accumulate_) {
    if (E2_acum_ > 0.f) {
      ret = true;
      erle_log2_ = FastApproxLog2f(Y2_acum_ / E2_acum_ + kEpsilon);
    }
    num_points_ = 0;
    E2_acum_ = 0.f;
    Y2_acum_ = 0.f;
  }

  if (ret) {
    UpdateMaxMin();
    UpdateQualityEstimate();
  }
  return ret;
}

void ErleEstimator::ErleTimeInstantaneous::Reset() {
  ResetAccumulators();
  max_erle_log2_ = -10.f;  // -30 dB.
  min_erle_log2_ = 33.f;   // 100 dB.
  inst_quality_estimate_ = 0.f;
}

void ErleEstimator::ErleTimeInstantaneous::ResetAccumulators() {
  erle_log2_ = absl::nullopt;
  inst_quality_estimate_ = 0.f;
  num_points_ = 0;
  E2_acum_ = 0.f;
  Y2_acum_ = 0.f;
}

void ErleEstimator::ErleTimeInstantaneous::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) {
  data_dumper->DumpRaw("aec3_erle_time_inst_log2",
                       erle_log2_ ? *erle_log2_ : -10.f);
  data_dumper->DumpRaw(
      "aec3_erle_time_quality",
      GetInstQualityEstimate() ? GetInstQualityEstimate().value() : 0.f);
  data_dumper->DumpRaw("aec3_erle_time_max_log2", max_erle_log2_);
  data_dumper->DumpRaw("aec3_erle_time_min_log2", min_erle_log2_);
}

void ErleEstimator::ErleTimeInstantaneous::UpdateMaxMin() {
  RTC_DCHECK(erle_log2_);
  if (erle_log2_.value() > max_erle_log2_) {
    max_erle_log2_ = erle_log2_.value();
  } else {
    max_erle_log2_ -= 0.0004;  // Forget factor, approx 1dB every 3 sec.
  }

  if (erle_log2_.value() < min_erle_log2_) {
    min_erle_log2_ = erle_log2_.value();
  } else {
    min_erle_log2_ += 0.0004;  // Forget factor, approx 1dB every 3 sec.
  }
}

void ErleEstimator::ErleTimeInstantaneous::UpdateQualityEstimate() {
  const float alpha = 0.07f;
  float quality_estimate = 0.f;
  RTC_DCHECK(erle_log2_);
  if (max_erle_log2_ > min_erle_log2_) {
    quality_estimate = (erle_log2_.value() - min_erle_log2_) /
                       (max_erle_log2_ - min_erle_log2_);
  }
  if (quality_estimate > inst_quality_estimate_) {
    inst_quality_estimate_ = quality_estimate;
  } else {
    inst_quality_estimate_ +=
        alpha * (quality_estimate - inst_quality_estimate_);
  }
}

ErleEstimator::ErleFreqInstantaneous::ErleFreqInstantaneous(
    int points_to_accumulate)
    : points_to_accumulate_(points_to_accumulate) {
  Reset();
}

ErleEstimator::ErleFreqInstantaneous::~ErleFreqInstantaneous() = default;

absl::optional<float>
ErleEstimator::ErleFreqInstantaneous::Update(float Y2, float E2, size_t band) {
  absl::optional<float> ret = absl::nullopt;
  RTC_DCHECK_LT(band, kFftLengthBy2Plus1);
  Y2_acum_[band] += Y2;
  E2_acum_[band] += E2;
  if (++num_points_[band] == points_to_accumulate_) {
    if (E2_acum_[band]) {
      ret = Y2_acum_[band] / E2_acum_[band];
    }
    num_points_[band] = 0;
    Y2_acum_[band] = 0.f;
    E2_acum_[band] = 0.f;
  }

  return ret;
}

void ErleEstimator::ErleFreqInstantaneous::Reset() {
  Y2_acum_.fill(0.f);
  E2_acum_.fill(0.f);
  num_points_.fill(0);
}

void ErleEstimator::Update(rtc::ArrayView<const float> render_spectrum,
                           rtc::ArrayView<const float> capture_spectrum,
                           rtc::ArrayView<const float> subtractor_spectrum,
                           bool converged_filter) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, render_spectrum.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, capture_spectrum.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, subtractor_spectrum.size());
  const auto& X2 = render_spectrum;
  const auto& Y2 = capture_spectrum;
  const auto& E2 = subtractor_spectrum;

  // Corresponds of WGN of power -46 dBFS.
  constexpr float kX2Min = 44015068.0f;

  constexpr int kErleHold = 100;
  constexpr int kBlocksForOnsetDetection = kErleHold + 150;

  auto erle_band_update = [](float erle_band, float new_erle, float alpha_inc,
                             float alpha_dec, float min_erle, float max_erle) {
    float alpha = new_erle > erle_band ? alpha_inc : alpha_dec;
    float erle_band_out = erle_band;
    erle_band_out = erle_band + alpha * (new_erle - erle_band);
    erle_band_out = rtc::SafeClamp(erle_band_out, min_erle, max_erle);
    return erle_band_out;
  };

  // Update the estimates in a clamped minimum statistics manner.
  auto erle_update = [&](size_t start, size_t stop, float max_erle) {
    for (size_t k = start; k < stop; ++k) {
      if (X2[k] > kX2Min) {
        absl::optional<float> new_erle =
            erle_freq_inst_.Update(Y2[k], E2[k], k);
        if (new_erle) {
          if (coming_onset_[k]) {
            coming_onset_[k] = false;
            erle_onsets_[k] =
                erle_band_update(erle_onsets_[k], new_erle.value(), 0.15f, 0.3f,
                                 min_erle_, max_erle);
          }
          hold_counters_[k] = kBlocksForOnsetDetection;
          erle_[k] = erle_band_update(erle_[k], new_erle.value(), 0.05f, 0.1f,
                                      min_erle_, max_erle);
        }
      }
    }
  };

  if (converged_filter) {
    // Note that the use of the converged_filter flag already imposed
    // a minimum of the erle that can be estimated as that flag would
    // be false if the filter is performing poorly.
    constexpr size_t kFftLengthBy4 = kFftLengthBy2 / 2;
    erle_update(1, kFftLengthBy4, max_erle_lf_);
    erle_update(kFftLengthBy4, kFftLengthBy2, max_erle_hf_);
  }

  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    hold_counters_[k]--;
    if (hold_counters_[k] <= (kBlocksForOnsetDetection - kErleHold)) {
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

  erle_[0] = erle_[1];
  erle_[kFftLengthBy2] = erle_[kFftLengthBy2 - 1];

  if (converged_filter) {
    // Compute ERLE over all frequency bins.
    const float X2_sum = std::accumulate(X2.begin(), X2.end(), 0.0f);
    if (X2_sum > kX2Min * X2.size()) {
      const float Y2_sum = std::accumulate(Y2.begin(), Y2.end(), 0.0f);
      const float E2_sum = std::accumulate(E2.begin(), E2.end(), 0.0f);
      if (erle_time_inst_.Update(Y2_sum, E2_sum)) {
        hold_counter_time_domain_ = kErleHold;
        erle_time_domain_log2_ +=
            0.1f * ((erle_time_inst_.GetInstErle_log2().value()) -
                    erle_time_domain_log2_);
        erle_time_domain_log2_ = rtc::SafeClamp(
            erle_time_domain_log2_, min_erle_log2_, max_erle_lf_log2);
      }
    }
  }
  --hold_counter_time_domain_;
  if (hold_counter_time_domain_ <= 0) {
    erle_time_domain_log2_ =
        std::max(min_erle_log2_, erle_time_domain_log2_ - 0.044f);
  }
  if (hold_counter_time_domain_ == 0) {
    erle_time_inst_.ResetAccumulators();
  }
}

void ErleEstimator::Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) {
  data_dumper->DumpRaw("aec3_erle", Erle());
  data_dumper->DumpRaw("aec3_erle_onset", ErleOnsets());
  data_dumper->DumpRaw("aec3_erle_time_domain_log2", ErleTimeDomainLog2());
  erle_time_inst_.Dump(data_dumper);
}

}  // namespace webrtc
