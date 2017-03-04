/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/main_filter_update_gain.h"

#include <algorithm>
#include <functional>

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace {

constexpr float kHErrorInitial = 10000.f;

}  // namespace

int MainFilterUpdateGain::instance_count_ = 0;

MainFilterUpdateGain::MainFilterUpdateGain()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      poor_excitation_counter_(1000) {
  H_error_.fill(kHErrorInitial);
}

MainFilterUpdateGain::~MainFilterUpdateGain() {}

void MainFilterUpdateGain::HandleEchoPathChange() {
  H_error_.fill(kHErrorInitial);
}

void MainFilterUpdateGain::Compute(
    const FftBuffer& render_buffer,
    const RenderSignalAnalyzer& render_signal_analyzer,
    const SubtractorOutput& subtractor_output,
    const AdaptiveFirFilter& filter,
    bool saturated_capture_signal,
    FftData* gain_fft) {
  RTC_DCHECK(gain_fft);
  // Introducing shorter notation to improve readability.
  const FftBuffer& X_buffer = render_buffer;
  const FftData& E_main = subtractor_output.E_main;
  const auto& E2_main = subtractor_output.E2_main;
  const auto& E2_shadow = subtractor_output.E2_shadow;
  FftData* G = gain_fft;
  const size_t size_partitions = filter.SizePartitions();
  const auto& X2 = X_buffer.SpectralSum(size_partitions);
  const auto& erl = filter.Erl();

  ++call_counter_;

  if (render_signal_analyzer.PoorSignalExcitation()) {
    poor_excitation_counter_ = 0;
  }

  // Do not update the filter if the render is not sufficiently excited.
  if (++poor_excitation_counter_ < size_partitions ||
      saturated_capture_signal || call_counter_ <= size_partitions) {
    G->re.fill(0.f);
    G->im.fill(0.f);
  } else {
    // Corresponds of WGN of power -46 dBFS.
    constexpr float kX2Min = 44015068.0f;
    std::array<float, kFftLengthBy2Plus1> mu;
    // mu = H_error / (0.5* H_error* X2 + n * E2).
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      mu[k] =
          X2[k] > kX2Min
              ? H_error_[k] /
                    (0.5f * H_error_[k] * X2[k] + size_partitions * E2_main[k])
              : 0.f;
    }

    // Avoid updating the filter close to narrow bands in the render signals.
    render_signal_analyzer.MaskRegionsAroundNarrowBands(&mu);

    // H_error = H_error - 0.5 * mu * X2 * H_error.
    for (size_t k = 0; k < H_error_.size(); ++k) {
      H_error_[k] -= 0.5f * mu[k] * X2[k] * H_error_[k];
    }

    // G = mu * E.
    std::transform(mu.begin(), mu.end(), E_main.re.begin(), G->re.begin(),
                   std::multiplies<float>());
    std::transform(mu.begin(), mu.end(), E_main.im.begin(), G->im.begin(),
                   std::multiplies<float>());
  }

  // H_error = H_error + factor * erl.
  std::array<float, kFftLengthBy2Plus1> H_error_increase;
  constexpr float kErlScaleAccurate = 1.f / 30.0f;
  constexpr float kErlScaleInaccurate = 1.f / 10.0f;
  std::transform(E2_shadow.begin(), E2_shadow.end(), E2_main.begin(),
                 H_error_increase.begin(), [&](float a, float b) {
                   return a >= b ? kErlScaleAccurate : kErlScaleInaccurate;
                 });
  std::transform(erl.begin(), erl.end(), H_error_increase.begin(),
                 H_error_increase.begin(), std::multiplies<float>());
  std::transform(H_error_.begin(), H_error_.end(), H_error_increase.begin(),
                 H_error_.begin(),
                 [&](float a, float b) { return std::max(a + b, 0.1f); });

  data_dumper_->DumpRaw("aec3_main_gain_H_error", H_error_);
}

}  // namespace webrtc
