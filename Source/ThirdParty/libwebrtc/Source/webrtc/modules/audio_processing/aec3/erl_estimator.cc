/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/erl_estimator.h"

#include <algorithm>

namespace webrtc {

namespace {

constexpr float kMinErl = 0.01f;
constexpr float kMaxErl = 1000.f;

}  // namespace

ErlEstimator::ErlEstimator() {
  erl_.fill(kMaxErl);
  hold_counters_.fill(0);
}

ErlEstimator::~ErlEstimator() = default;

void ErlEstimator::Update(
    const std::array<float, kFftLengthBy2Plus1>& render_spectrum,
    const std::array<float, kFftLengthBy2Plus1>& capture_spectrum) {
  const auto& X2 = render_spectrum;
  const auto& Y2 = capture_spectrum;

  // Corresponds to WGN of power -46 dBFS.
  constexpr float kX2Min = 44015068.0f;

  // Update the estimates in a maximum statistics manner.
  for (size_t k = 1; k < kFftLengthBy2; ++k) {
    if (X2[k] > kX2Min) {
      const float new_erl = Y2[k] / X2[k];
      if (new_erl < erl_[k]) {
        hold_counters_[k - 1] = 1000;
        erl_[k] += 0.1 * (new_erl - erl_[k]);
        erl_[k] = std::max(erl_[k], kMinErl);
      }
    }
  }

  std::for_each(hold_counters_.begin(), hold_counters_.end(),
                [](int& a) { --a; });
  std::transform(hold_counters_.begin(), hold_counters_.end(), erl_.begin() + 1,
                 erl_.begin() + 1, [](int a, float b) {
                   return a > 0 ? b : std::min(kMaxErl, 2.f * b);
                 });

  erl_[0] = erl_[1];
  erl_[kFftLengthBy2] = erl_[kFftLengthBy2 - 1];
}

}  // namespace webrtc
