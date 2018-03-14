/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/main_filter_update_gain.h"

#include <algorithm>
#include <functional>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr float kHErrorInitial = 10000.f;
constexpr int kPoorExcitationCounterInitial = 1000;

}  // namespace

int MainFilterUpdateGain::instance_count_ = 0;

MainFilterUpdateGain::MainFilterUpdateGain(
    const EchoCanceller3Config::Filter::MainConfiguration& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      config_(config),
      poor_excitation_counter_(kPoorExcitationCounterInitial) {
  H_error_.fill(kHErrorInitial);
}

MainFilterUpdateGain::~MainFilterUpdateGain() {}

void MainFilterUpdateGain::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  // TODO(peah): Add even-specific behavior.
  H_error_.fill(kHErrorInitial);
  poor_excitation_counter_ = kPoorExcitationCounterInitial;
  call_counter_ = 0;
}

void MainFilterUpdateGain::Compute(
    const std::array<float, kFftLengthBy2Plus1>& render_power,
    const RenderSignalAnalyzer& render_signal_analyzer,
    const SubtractorOutput& subtractor_output,
    const AdaptiveFirFilter& filter,
    bool saturated_capture_signal,
    FftData* gain_fft) {
  RTC_DCHECK(gain_fft);
  // Introducing shorter notation to improve readability.
  const FftData& E_main = subtractor_output.E_main;
  const auto& E2_main = subtractor_output.E2_main;
  const auto& E2_shadow = subtractor_output.E2_shadow;
  FftData* G = gain_fft;
  const size_t size_partitions = filter.SizePartitions();
  auto X2 = render_power;
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
    // Corresponds to WGN of power -39 dBFS.
    std::array<float, kFftLengthBy2Plus1> mu;
    // mu = H_error / (0.5* H_error* X2 + n * E2).
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      mu[k] = X2[k] > config_.noise_gate
                  ? H_error_[k] / (0.5f * H_error_[k] * X2[k] +
                                   size_partitions * E2_main[k])
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
  std::transform(E2_shadow.begin(), E2_shadow.end(), E2_main.begin(),
                 H_error_increase.begin(), [&](float a, float b) {
                   return a >= b ? config_.leakage_converged
                                 : config_.leakage_diverged;
                 });
  std::transform(erl.begin(), erl.end(), H_error_increase.begin(),
                 H_error_increase.begin(), std::multiplies<float>());
  std::transform(H_error_.begin(), H_error_.end(), H_error_increase.begin(),
                 H_error_.begin(), [&](float a, float b) {
                   return std::max(a + b, config_.error_floor);
                 });

  data_dumper_->DumpRaw("aec3_main_gain_H_error", H_error_);
}

}  // namespace webrtc
