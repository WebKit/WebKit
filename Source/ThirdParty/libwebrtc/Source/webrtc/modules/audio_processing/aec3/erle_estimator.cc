/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/erle_estimator.h"

#include <algorithm>

namespace webrtc {

namespace {

constexpr float kMinErle = 1.f;
constexpr float kMaxErle = 8.f;

}  // namespace

ErleEstimator::ErleEstimator() {
  erle_.fill(kMinErle);
  hold_counters_.fill(0);
}

ErleEstimator::~ErleEstimator() = default;

void ErleEstimator::Update(
    const std::array<float, kFftLengthBy2Plus1>& render_spectrum,
    const std::array<float, kFftLengthBy2Plus1>& capture_spectrum,
    const std::array<float, kFftLengthBy2Plus1>& subtractor_spectrum) {
  const auto& X2 = render_spectrum;
  const auto& Y2 = capture_spectrum;
  const auto& E2 = subtractor_spectrum;

  // Corresponds of WGN of power -46 dBFS.
  constexpr float kX2Min = 44015068.0f;

  // Update the estimates in a clamped minimum statistics manner.
  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    if (X2[k] > kX2Min && E2[k] > 0.f) {
      const float new_erle = Y2[k] / E2[k];
      if (new_erle > erle_[k]) {
        hold_counters_[k - 1] = 100;
        erle_[k] += 0.1f * (new_erle - erle_[k]);
        erle_[k] = std::max(kMinErle, std::min(erle_[k], kMaxErle));
      }
    }
  }

  std::for_each(hold_counters_.begin(), hold_counters_.end(),
                [](int& a) { --a; });
  std::transform(hold_counters_.begin(), hold_counters_.end(),
                 erle_.begin() + 1, erle_.begin() + 1, [](int a, float b) {
                   return a > 0 ? b : std::max(kMinErle, 0.97f * b);
                 });

  erle_[0] = erle_[1];
  erle_[kFftLengthBy2] = erle_[kFftLengthBy2 - 1];
}

}  // namespace webrtc
