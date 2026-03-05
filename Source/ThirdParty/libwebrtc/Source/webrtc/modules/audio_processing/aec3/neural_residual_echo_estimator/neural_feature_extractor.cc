/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_feature_extractor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "common_audio/window_generator.h"
#include "rtc_base/checks.h"
#include "third_party/pffft/src/pffft.h"

namespace webrtc {

namespace {
// Trained moodel expects [-1,1]-scaled signals while AEC3 and APM scale
// floating point signals up by 32768 to match 16-bit fixed-point formats, so we
// convert to [-1,1] scale here.
constexpr float kScale = 1.0f / 32768;
// Exponent used to compress the power spectra.
constexpr float kSpectrumCompressionExponent = 0.15f;

const std::array<FeatureExtractor::ModelInputEnum, 2> kExpectedInputs = {
    FeatureExtractor::ModelInputEnum::kLinearAecOutput,
    FeatureExtractor::ModelInputEnum::kAecRef};

std::vector<float> GetSqrtHanningWindow(int frame_size, float scale) {
  std::vector<float> window(frame_size);
  WindowGenerator::Hanning(frame_size, window.data());
  std::transform(window.begin(), window.end(), window.begin(),
                 [scale](float x) { return scale * std::sqrt(x); });
  return window;
}
}  // namespace

void TimeDomainFeatureExtractor::PushFeaturesToModelInput(
    std::vector<float>& frame,
    ArrayView<float> model_input,
    FeatureExtractor::ModelInputEnum input_enum) {
  // Shift down overlap from previous frames.
  std::copy(model_input.begin() + frame.size(), model_input.end(),
            model_input.begin());
  std::transform(frame.begin(), frame.end(), model_input.end() - frame.size(),
                 [](float x) { return x * kScale; });
  frame.clear();
}

FrequencyDomainFeatureExtractor::FrequencyDomainFeatureExtractor(int step_size)
    : step_size_(step_size),
      frame_size_(2 * step_size_),
      sqrt_hanning_(GetSqrtHanningWindow(frame_size_, kScale)),
      spectrum_(static_cast<float*>(
          pffft_aligned_malloc(frame_size_ * sizeof(float)))),
      work_(static_cast<float*>(
          pffft_aligned_malloc(frame_size_ * sizeof(float)))),
      pffft_setup_(pffft_new_setup(frame_size_, PFFFT_REAL)),
      pffft_states_(
          static_cast<size_t>(FeatureExtractor::ModelInputEnum::kNumInputs)) {
  std::memset(spectrum_, 0, sizeof(float) * frame_size_);
  for (const auto model_input_enum : kExpectedInputs) {
    pffft_states_[static_cast<size_t>(model_input_enum)] =
        std::make_unique<PffftState>(frame_size_);
  }
}

FrequencyDomainFeatureExtractor::~FrequencyDomainFeatureExtractor() {
  pffft_destroy_setup(pffft_setup_);
  pffft_aligned_free(work_);
  pffft_aligned_free(spectrum_);
}

void FrequencyDomainFeatureExtractor::PushFeaturesToModelInput(
    std::vector<float>& frame,
    ArrayView<float> model_input,
    FeatureExtractor::ModelInputEnum input_enum) {
  std::unique_ptr<PffftState>& pffft_state =
      pffft_states_[static_cast<size_t>(input_enum)];
  if (pffft_state == nullptr) {
    frame.clear();
    return;
  }
  float* data = pffft_state->data();
  std::memcpy(data + step_size_, frame.data(), sizeof(float) * step_size_);
  for (int k = 0; k < frame_size_; ++k) {
    data[k] *= sqrt_hanning_[k];
  }
  pffft_transform_ordered(pffft_setup_, data, spectrum_, work_, PFFFT_FORWARD);
  RTC_CHECK_EQ(model_input.size(), step_size_ + 1);
  model_input[0] = spectrum_[0] * spectrum_[0];
  model_input[step_size_] = spectrum_[1] * spectrum_[1];
  for (int k = 1; k < step_size_; k++) {
    model_input[k] = spectrum_[2 * k] * spectrum_[2 * k] +
                     spectrum_[2 * k + 1] * spectrum_[2 * k + 1];
  }
  // Compress the power spectra.
  std::transform(
      model_input.begin(), model_input.end(), model_input.begin(),
      [](float a) { return std::pow(a, kSpectrumCompressionExponent); });
  // Saving the current frame as it is used when computing the next FFT.
  std::memcpy(data, frame.data(), sizeof(float) * step_size_);
  frame.clear();
}

}  // namespace webrtc
