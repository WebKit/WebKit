/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/subtractor.h"

#include <algorithm>
#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {

void PredictionError(const Aec3Fft& fft,
                     const FftData& S,
                     rtc::ArrayView<const float> y,
                     std::array<float, kBlockSize>* e,
                     std::array<float, kBlockSize>* s,
                     bool* saturation) {
  std::array<float, kFftLength> tmp;
  fft.Ifft(S, &tmp);
  constexpr float kScale = 1.0f / kFftLengthBy2;
  std::transform(y.begin(), y.end(), tmp.begin() + kFftLengthBy2, e->begin(),
                 [&](float a, float b) { return a - b * kScale; });

  *saturation = false;

  if (s) {
    for (size_t k = 0; k < s->size(); ++k) {
      (*s)[k] = kScale * tmp[k + kFftLengthBy2];
    }
    auto result = std::minmax_element(s->begin(), s->end());
    *saturation = *result.first <= -32768 || *result.first >= 32767;
  }
  if (!(*saturation)) {
    auto result = std::minmax_element(e->begin(), e->end());
    *saturation = *result.first <= -32768 || *result.first >= 32767;
  }

  std::for_each(e->begin(), e->end(),
                [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
}

}  // namespace

Subtractor::Subtractor(const EchoCanceller3Config& config,
                       ApmDataDumper* data_dumper,
                       Aec3Optimization optimization)
    : fft_(),
      data_dumper_(data_dumper),
      optimization_(optimization),
      config_(config),
      main_filter_(config_.filter.main.length_blocks,
                   optimization,
                   data_dumper_),
      shadow_filter_(config_.filter.shadow.length_blocks,
                     optimization,
                     data_dumper_),
      G_main_(config_.filter.main_initial),
      G_shadow_(config_.filter.shadow_initial) {
  RTC_DCHECK(data_dumper_);
  // Currently, the rest of AEC3 requires the main and shadow filter lengths to
  // be identical.
  RTC_DCHECK_EQ(config_.filter.main.length_blocks,
                config_.filter.shadow.length_blocks);
  RTC_DCHECK_EQ(config_.filter.main_initial.length_blocks,
                config_.filter.shadow_initial.length_blocks);

  RTC_DCHECK_GE(config_.filter.main.length_blocks,
                config_.filter.main_initial.length_blocks);
  RTC_DCHECK_GE(config_.filter.shadow.length_blocks,
                config_.filter.shadow_initial.length_blocks);

  main_filter_.SetSizePartitions(config_.filter.main_initial.length_blocks);
  shadow_filter_.SetSizePartitions(config_.filter.shadow_initial.length_blocks);
}

Subtractor::~Subtractor() = default;

void Subtractor::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    main_filter_.HandleEchoPathChange();
    shadow_filter_.HandleEchoPathChange();
    G_main_.HandleEchoPathChange(echo_path_variability);
    G_shadow_.HandleEchoPathChange();
    G_main_.SetConfig(config_.filter.main_initial);
    G_shadow_.SetConfig(config_.filter.shadow_initial);
    main_filter_converged_ = false;
    shadow_filter_converged_ = false;
    main_filter_.SetSizePartitions(config_.filter.main_initial.length_blocks);
    shadow_filter_.SetSizePartitions(
        config_.filter.shadow_initial.length_blocks);
  };

  // TODO(peah): Add delay-change specific reset behavior.
  if ((echo_path_variability.delay_change ==
       EchoPathVariability::DelayAdjustment::kBufferFlush) ||
      (echo_path_variability.delay_change ==
       EchoPathVariability::DelayAdjustment::kDelayReset)) {
    full_reset();
  } else if (echo_path_variability.delay_change ==
             EchoPathVariability::DelayAdjustment::kNewDetectedDelay) {
    full_reset();
  } else if (echo_path_variability.delay_change ==
             EchoPathVariability::DelayAdjustment::kBufferReadjustment) {
    full_reset();
  }
}

void Subtractor::ExitInitialState() {
  G_main_.SetConfig(config_.filter.main);
  G_shadow_.SetConfig(config_.filter.shadow);
  main_filter_.SetSizePartitions(config_.filter.main.length_blocks);
  shadow_filter_.SetSizePartitions(config_.filter.shadow.length_blocks);
}

void Subtractor::Process(const RenderBuffer& render_buffer,
                         const rtc::ArrayView<const float> capture,
                         const RenderSignalAnalyzer& render_signal_analyzer,
                         const AecState& aec_state,
                         SubtractorOutput* output) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());
  rtc::ArrayView<const float> y = capture;
  FftData& E_main = output->E_main;
  FftData& E_main_nonwindowed = output->E_main_nonwindowed;
  FftData E_shadow;
  std::array<float, kBlockSize>& e_main = output->e_main;
  std::array<float, kBlockSize>& e_shadow = output->e_shadow;

  FftData S;
  FftData& G = S;

  // Form the output of the main filter.
  main_filter_.Filter(render_buffer, &S);
  bool main_saturation = false;
  PredictionError(fft_, S, y, &e_main, &output->s_main, &main_saturation);
  fft_.ZeroPaddedFft(e_main, Aec3Fft::Window::kHanning, &E_main);

  // Form the output of the shadow filter.
  shadow_filter_.Filter(render_buffer, &S);
  bool shadow_saturation = false;
  PredictionError(fft_, S, y, &e_shadow, nullptr, &shadow_saturation);
  fft_.ZeroPaddedFft(e_shadow, Aec3Fft::Window::kHanning, &E_shadow);

  if (!(main_filter_converged_ || shadow_filter_converged_)) {
    const auto sum_of_squares = [](float a, float b) { return a + b * b; };
    const float y2 = std::accumulate(y.begin(), y.end(), 0.f, sum_of_squares);

    if (!main_filter_converged_) {
      const float e2_main =
          std::accumulate(e_main.begin(), e_main.end(), 0.f, sum_of_squares);
      main_filter_converged_ = e2_main > 0.1 * y2;
    }

    if (!shadow_filter_converged_) {
      const float e2_shadow = std::accumulate(e_shadow.begin(), e_shadow.end(),
                                              0.f, sum_of_squares);
      shadow_filter_converged_ = e2_shadow > 0.1 * y2;
    }
  }

  // Compute spectra for future use.
  E_shadow.Spectrum(optimization_, output->E2_shadow);
  E_main.Spectrum(optimization_, output->E2_main);

  if (main_filter_converged_ || !shadow_filter_converged_) {
    fft_.ZeroPaddedFft(e_main, Aec3Fft::Window::kRectangular,
                       &E_main_nonwindowed);
    E_main_nonwindowed.Spectrum(optimization_, output->E2_main_nonwindowed);
  } else {
    fft_.ZeroPaddedFft(e_shadow, Aec3Fft::Window::kRectangular,
                       &E_main_nonwindowed);
    E_main_nonwindowed.Spectrum(optimization_, output->E2_main_nonwindowed);
  }

  // Update the main filter.
  std::array<float, kFftLengthBy2Plus1> X2;
  render_buffer.SpectralSum(main_filter_.SizePartitions(), &X2);
  G_main_.Compute(X2, render_signal_analyzer, *output, main_filter_,
                  aec_state.SaturatedCapture() || main_saturation, &G);
  main_filter_.Adapt(render_buffer, G);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.im);

  // Update the shadow filter.
  if (shadow_filter_.SizePartitions() != main_filter_.SizePartitions()) {
    render_buffer.SpectralSum(shadow_filter_.SizePartitions(), &X2);
  }
  G_shadow_.Compute(X2, render_signal_analyzer, E_shadow,
                    shadow_filter_.SizePartitions(),
                    aec_state.SaturatedCapture() || shadow_saturation, &G);
  shadow_filter_.Adapt(render_buffer, G);

  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.im);

  main_filter_.DumpFilter("aec3_subtractor_H_main");
  shadow_filter_.DumpFilter("aec3_subtractor_H_shadow");
}

}  // namespace webrtc
