/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/subtractor.h"

#include <algorithm>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

namespace {

void ComputeError(const Aec3Fft& fft,
                  const FftData& S,
                  rtc::ArrayView<const float> y,
                  std::array<float, kBlockSize>* e,
                  FftData* E) {
  std::array<float, kFftLength> s;
  fft.Ifft(S, &s);
  constexpr float kScale = 1.0f / kFftLengthBy2;
  std::transform(y.begin(), y.end(), s.begin() + kFftLengthBy2, e->begin(),
                 [&](float a, float b) { return a - b * kScale; });
  std::for_each(e->begin(), e->end(), [](float& a) {
    a = std::max(std::min(a, 32767.0f), -32768.0f);
  });
  fft.ZeroPaddedFft(*e, E);
}
}  // namespace

std::vector<size_t> Subtractor::NumBlocksInRenderSums() const {
  if (kMainFilterSizePartitions != kShadowFilterSizePartitions) {
    return {kMainFilterSizePartitions, kShadowFilterSizePartitions};
  } else {
    return {kMainFilterSizePartitions};
  }
}

Subtractor::Subtractor(ApmDataDumper* data_dumper,
                       Aec3Optimization optimization)
    : fft_(),
      data_dumper_(data_dumper),
      optimization_(optimization),
      main_filter_(kMainFilterSizePartitions, true, optimization, data_dumper_),
      shadow_filter_(kShadowFilterSizePartitions,
                     false,
                     optimization,
                     data_dumper_) {
  RTC_DCHECK(data_dumper_);
}

Subtractor::~Subtractor() {}

void Subtractor::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  if (echo_path_variability.delay_change) {
    main_filter_.HandleEchoPathChange();
    shadow_filter_.HandleEchoPathChange();
    G_main_.HandleEchoPathChange();
  }
}

void Subtractor::Process(const FftBuffer& render_buffer,
                         const rtc::ArrayView<const float> capture,
                         const RenderSignalAnalyzer& render_signal_analyzer,
                         bool saturation,
                         SubtractorOutput* output) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());
  rtc::ArrayView<const float> y = capture;
  const FftBuffer& X_buffer = render_buffer;
  FftData& E_main = output->E_main;
  FftData& E_shadow = output->E_shadow;
  std::array<float, kBlockSize>& e_main = output->e_main;
  std::array<float, kBlockSize>& e_shadow = output->e_shadow;

  FftData S;
  FftData& G = S;

  // Form and analyze the output of the main filter.
  main_filter_.Filter(X_buffer, &S);
  ComputeError(fft_, S, y, &e_main, &E_main);

  // Form and analyze the output of the shadow filter.
  shadow_filter_.Filter(X_buffer, &S);
  ComputeError(fft_, S, y, &e_shadow, &E_shadow);

  // Compute spectra for future use.
  E_main.Spectrum(optimization_, &output->E2_main);
  E_shadow.Spectrum(optimization_, &output->E2_shadow);

  // Update the main filter.
  G_main_.Compute(X_buffer, render_signal_analyzer, *output, main_filter_,
                  saturation, &G);
  main_filter_.Adapt(X_buffer, G);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_main", G.im);

  // Update the shadow filter.
  G_shadow_.Compute(X_buffer, render_signal_analyzer, E_shadow,
                    shadow_filter_.SizePartitions(), saturation, &G);
  shadow_filter_.Adapt(X_buffer, G);
  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.re);
  data_dumper_->DumpRaw("aec3_subtractor_G_shadow", G.im);

  main_filter_.DumpFilter("aec3_subtractor_H_main");
  shadow_filter_.DumpFilter("aec3_subtractor_H_shadow");
}

}  // namespace webrtc
