/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/shadow_filter_update_gain.h"

#include <algorithm>
#include <functional>

#include "webrtc/base/checks.h"

namespace webrtc {

void ShadowFilterUpdateGain::Compute(
    const FftBuffer& X_buffer,
    const RenderSignalAnalyzer& render_signal_analyzer,
    const FftData& E_shadow,
    size_t size_partitions,
    bool saturated_capture_signal,
    FftData* G) {
  RTC_DCHECK(G);
  ++call_counter_;

  if (render_signal_analyzer.PoorSignalExcitation()) {
    poor_signal_excitation_counter_ = 0;
  }

  // Do not update the filter if the render is not sufficiently excited.
  if (++poor_signal_excitation_counter_ < size_partitions ||
      saturated_capture_signal || call_counter_ <= size_partitions) {
    G->re.fill(0.f);
    G->im.fill(0.f);
    return;
  }

  // Compute mu.
  constexpr float kX2Min = 44015068.0f;
  constexpr float kMuFixed = .5f;
  std::array<float, kFftLengthBy2Plus1> mu;
  const auto& X2 = X_buffer.SpectralSum(size_partitions);
  std::transform(X2.begin(), X2.end(), mu.begin(),
                 [&](float a) { return a > kX2Min ? kMuFixed / a : 0.f; });

  // Avoid updating the filter close to narrow bands in the render signals.
  render_signal_analyzer.MaskRegionsAroundNarrowBands(&mu);

  // G = mu * E * X2.
  std::transform(mu.begin(), mu.end(), E_shadow.re.begin(), G->re.begin(),
                 std::multiplies<float>());
  std::transform(mu.begin(), mu.end(), E_shadow.im.begin(), G->im.begin(),
                 std::multiplies<float>());
}

}  // namespace webrtc
