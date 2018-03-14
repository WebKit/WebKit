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

#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

ErleEstimator::ErleEstimator(float min_erle,
                             float max_erle_lf,
                             float max_erle_hf)
    : min_erle_(min_erle),
      max_erle_lf_(max_erle_lf),
      max_erle_hf_(max_erle_hf) {
  erle_.fill(min_erle_);
  hold_counters_.fill(0);
  erle_time_domain_ = min_erle_;
  hold_counter_time_domain_ = 0;
}

ErleEstimator::~ErleEstimator() = default;

void ErleEstimator::Update(rtc::ArrayView<const float> render_spectrum,
                           rtc::ArrayView<const float> capture_spectrum,
                           rtc::ArrayView<const float> subtractor_spectrum) {
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, render_spectrum.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, capture_spectrum.size());
  RTC_DCHECK_EQ(kFftLengthBy2Plus1, subtractor_spectrum.size());
  const auto& X2 = render_spectrum;
  const auto& Y2 = capture_spectrum;
  const auto& E2 = subtractor_spectrum;

  // Corresponds of WGN of power -46 dBFS.
  constexpr float kX2Min = 44015068.0f;

  // Update the estimates in a clamped minimum statistics manner.
  auto erle_update = [&](size_t start, size_t stop, float max_erle) {
    for (size_t k = start; k < stop; ++k) {
      if (X2[k] > kX2Min && E2[k] > 0.f) {
        const float new_erle = Y2[k] / E2[k];
        if (new_erle > erle_[k]) {
          hold_counters_[k - 1] = 100;
          erle_[k] += 0.1f * (new_erle - erle_[k]);
          erle_[k] = rtc::SafeClamp(erle_[k], min_erle_, max_erle);
        }
      }
    }
  };
  erle_update(1, kFftLengthBy2 / 2, max_erle_lf_);
  erle_update(kFftLengthBy2 / 2, kFftLengthBy2, max_erle_hf_);

  std::for_each(hold_counters_.begin(), hold_counters_.end(),
                [](int& a) { --a; });
  std::transform(hold_counters_.begin(), hold_counters_.end(),
                 erle_.begin() + 1, erle_.begin() + 1, [&](int a, float b) {
                   return a > 0 ? b : std::max(min_erle_, 0.97f * b);
                 });

  erle_[0] = erle_[1];
  erle_[kFftLengthBy2] = erle_[kFftLengthBy2 - 1];

  // Compute ERLE over all frequency bins.
  const float X2_sum = std::accumulate(X2.begin(), X2.end(), 0.0f);
  const float E2_sum = std::accumulate(E2.begin(), E2.end(), 0.0f);
  if (X2_sum > kX2Min * X2.size() && E2_sum > 0.f) {
    const float Y2_sum = std::accumulate(Y2.begin(), Y2.end(), 0.0f);
    const float new_erle = Y2_sum / E2_sum;
    if (new_erle > erle_time_domain_) {
      hold_counter_time_domain_ = 100;
      erle_time_domain_ += 0.1f * (new_erle - erle_time_domain_);
      erle_time_domain_ =
          rtc::SafeClamp(erle_time_domain_, min_erle_, max_erle_lf_);
    }
  }
  --hold_counter_time_domain_;
  erle_time_domain_ = (hold_counter_time_domain_ > 0)
                        ? erle_time_domain_
                        : std::max(min_erle_, 0.97f * erle_time_domain_);
}

}  // namespace webrtc
