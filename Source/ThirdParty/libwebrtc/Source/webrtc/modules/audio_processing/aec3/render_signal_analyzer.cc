/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/render_signal_analyzer.h"

#include <algorithm>

#include "webrtc/base/checks.h"

namespace webrtc {

namespace {
constexpr size_t kCounterThreshold = 5;

}  // namespace

RenderSignalAnalyzer::RenderSignalAnalyzer() {
  narrow_band_counters_.fill(0);
}
RenderSignalAnalyzer::~RenderSignalAnalyzer() = default;

void RenderSignalAnalyzer::Update(
    const RenderBuffer& render_buffer,
    const rtc::Optional<size_t>& delay_partitions) {
  if (!delay_partitions) {
    narrow_band_counters_.fill(0);
    return;
  }

  const std::array<float, kFftLengthBy2Plus1>& X2 =
      render_buffer.Spectrum(*delay_partitions);

  // Detect narrow band signal regions.
  for (size_t k = 1; k < (X2.size() - 1); ++k) {
    narrow_band_counters_[k - 1] = X2[k] > 3 * std::max(X2[k - 1], X2[k + 1])
                                       ? narrow_band_counters_[k - 1] + 1
                                       : 0;
  }
}

void RenderSignalAnalyzer::MaskRegionsAroundNarrowBands(
    std::array<float, kFftLengthBy2Plus1>* v) const {
  RTC_DCHECK(v);

  // Set v to zero around narrow band signal regions.
  if (narrow_band_counters_[0] > kCounterThreshold) {
    (*v)[1] = (*v)[0] = 0.f;
  }
  for (size_t k = 2; k < kFftLengthBy2 - 1; ++k) {
    if (narrow_band_counters_[k - 1] > kCounterThreshold) {
      (*v)[k - 2] = (*v)[k - 1] = (*v)[k] = (*v)[k + 1] = (*v)[k + 2] = 0.f;
    }
  }
  if (narrow_band_counters_[kFftLengthBy2 - 2] > kCounterThreshold) {
    (*v)[kFftLengthBy2] = (*v)[kFftLengthBy2 - 1] = 0.f;
  }
}

}  // namespace webrtc
