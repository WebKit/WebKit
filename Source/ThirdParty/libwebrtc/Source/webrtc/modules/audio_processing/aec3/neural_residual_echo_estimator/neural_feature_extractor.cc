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
#include "modules/audio_processing/aec3/aec3_common.h"
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

const std::array<FeatureExtractor::ModelInputEnum, 2> kRequiredModelInputs = {
    FeatureExtractor::ModelInputEnum::kLinearAecOutput,
    FeatureExtractor::ModelInputEnum::kAecRef};

std::vector<float> GetSqrtHanningWindow(int frame_size, float scale) {
  std::vector<float> window(frame_size);
  WindowGenerator::Hanning(frame_size, window.data());
  std::transform(window.begin(), window.end(), window.begin(),
                 [scale](float x) { return scale * std::sqrt(x); });
  return window;
}

std::array<float, kBlockSize> AverageAllChannels(
    ArrayView<const ArrayView<const float, kBlockSize>> all_channels) {
  std::array<float, kBlockSize> summed_block;
  summed_block.fill(0.0f);
  const float scale = kScale * 1.0f / all_channels.size();
  for (auto& channel : all_channels) {
    for (size_t i = 0; i < kBlockSize; ++i) {
      summed_block[i] += scale * channel[i];
    }
  }
  return summed_block;
}

bool RequiredInput(FeatureExtractor::ModelInputEnum input_type) {
  for (const auto model_input_enum : kRequiredModelInputs) {
    if (model_input_enum == input_type) {
      return true;
    }
  }
  return false;
}

}  // namespace

TimeDomainFeatureExtractor::TimeDomainFeatureExtractor(int step_size)
    : step_size_(step_size),
      input_buffer_(static_cast<size_t>(ModelInputEnum::kNumInputs)) {}

TimeDomainFeatureExtractor::~TimeDomainFeatureExtractor() = default;

void TimeDomainFeatureExtractor::Reset() {
  for (auto& buffer : input_buffer_) {
    buffer.clear();
  }
}

bool TimeDomainFeatureExtractor::ReadyForInference() const {
  for (const auto model_input_enum : kRequiredModelInputs) {
    const std::vector<float>& input_buffer =
        input_buffer_[static_cast<size_t>(model_input_enum)];
    if (input_buffer.size() < step_size_) {
      return false;
    }
  }
  return true;
}

void TimeDomainFeatureExtractor::UpdateBuffers(
    ArrayView<const ArrayView<const float, kBlockSize>> all_channels,
    ModelInputEnum input_type) {
  if (!RequiredInput(input_type)) {
    return;
  }
  std::vector<float>& input_buffer =
      input_buffer_[static_cast<size_t>(input_type)];
  std::array<float, kBlockSize> summed_block = AverageAllChannels(all_channels);
  input_buffer.insert(input_buffer.end(), summed_block.cbegin(),
                      summed_block.cend());
}

void TimeDomainFeatureExtractor::PrepareModelInput(ArrayView<float> model_input,
                                                   ModelInputEnum input_type) {
  if (!RequiredInput(input_type)) {
    return;
  }
  std::vector<float>& input_buffer =
      input_buffer_[static_cast<size_t>(input_type)];
  RTC_CHECK_EQ(input_buffer.size(), step_size_);
  std::copy(model_input.cbegin() + step_size_, model_input.cend(),
            model_input.begin());
  std::copy(input_buffer.cbegin(), input_buffer.cend(),
            model_input.end() - step_size_);
  input_buffer.clear();
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
          static_cast<size_t>(FeatureExtractor::ModelInputEnum::kNumInputs)),
      input_buffer_(
          static_cast<size_t>(FeatureExtractor::ModelInputEnum::kNumInputs)) {
  std::memset(spectrum_, 0, sizeof(float) * frame_size_);
  for (const auto model_input_enum : kRequiredModelInputs) {
    pffft_states_[static_cast<size_t>(model_input_enum)].emplace_back(
        std::make_unique<PffftState>(frame_size_));
  }
}

void FrequencyDomainFeatureExtractor::Reset() {
  std::memset(spectrum_, 0, sizeof(float) * frame_size_);
  for (auto& buffers : input_buffer_) {
    for (auto& buffer : buffers) {
      buffer.clear();
    }
  }
  for (auto& states : pffft_states_) {
    for (auto& state : states) {
      if (state) {
        std::memset(state->data(), 0, sizeof(float) * frame_size_);
      }
    }
  }
}

FrequencyDomainFeatureExtractor::~FrequencyDomainFeatureExtractor() {
  pffft_destroy_setup(pffft_setup_);
  pffft_aligned_free(work_);
  pffft_aligned_free(spectrum_);
}

bool FrequencyDomainFeatureExtractor::ReadyForInference() const {
  for (const auto model_input_enum : kRequiredModelInputs) {
    const std::vector<std::vector<float>>& input_buffer =
        input_buffer_[static_cast<size_t>(model_input_enum)];
    if (input_buffer.empty() || input_buffer[0].size() < step_size_) {
      return false;
    }
  }
  return true;
}

void FrequencyDomainFeatureExtractor::ComputeAndAddPowerSpectra(
    ArrayView<const float> frame,
    std::unique_ptr<PffftState>& pffft_state,
    int number_channels,
    ArrayView<float> power_spectra) {
  const float kAverageScale = 1.0f / number_channels;
  if (pffft_state == nullptr) {
    pffft_state = std::make_unique<PffftState>(frame_size_);
  }
  RTC_CHECK(pffft_state);
  float* data = pffft_state->data();
  std::memcpy(data + step_size_, frame.data(), sizeof(float) * step_size_);
  for (int k = 0; k < frame_size_; ++k) {
    data[k] *= sqrt_hanning_[k];
  }
  pffft_transform_ordered(pffft_setup_, data, spectrum_, work_, PFFFT_FORWARD);
  RTC_CHECK_EQ(power_spectra.size(), step_size_ + 1);
  power_spectra[0] += kAverageScale * (spectrum_[0] * spectrum_[0]);
  power_spectra[step_size_] += kAverageScale * spectrum_[1] * spectrum_[1];
  for (size_t k = 1; k < step_size_; k++) {
    power_spectra[k] +=
        kAverageScale * (spectrum_[2 * k] * spectrum_[2 * k] +
                         spectrum_[2 * k + 1] * spectrum_[2 * k + 1]);
  }
  // Saving the current frame as it is used when computing the next FFT.
  std::memcpy(data, frame.data(), sizeof(float) * step_size_);
}

void FrequencyDomainFeatureExtractor::UpdateBuffers(
    ArrayView<const ArrayView<const float, kBlockSize>> all_channels,
    ModelInputEnum input_type) {
  if (!RequiredInput(input_type)) {
    return;
  }
  std::vector<std::vector<float>>& input_buffer =
      input_buffer_[static_cast<size_t>(input_type)];
  input_buffer.resize(all_channels.size());
  for (size_t ch = 0; ch < all_channels.size(); ++ch) {
    const ArrayView<const float, kBlockSize>& frame_in = all_channels[ch];
    std::vector<float>& input_buffer_ch = input_buffer[ch];
    input_buffer_ch.insert(input_buffer_ch.end(), frame_in.cbegin(),
                           frame_in.cend());
  }
}
void FrequencyDomainFeatureExtractor::PrepareModelInput(
    ArrayView<float> model_input,
    ModelInputEnum input_type) {
  if (!RequiredInput(input_type)) {
    return;
  }
  std::vector<std::vector<float>>& input_buffer =
      input_buffer_[static_cast<size_t>(input_type)];
  RTC_CHECK_EQ(input_buffer[0].size(), step_size_);
  std::memset(model_input.data(), 0, sizeof(float) * model_input.size());
  std::vector<std::unique_ptr<PffftState>>& pffft_states_channels =
      pffft_states_[static_cast<size_t>(input_type)];
  pffft_states_channels.resize(input_buffer.size());
  for (size_t ch = 0; ch < input_buffer.size(); ++ch) {
    ComputeAndAddPowerSpectra(input_buffer[ch], pffft_states_channels[ch],
                              static_cast<int>(input_buffer.size()),
                              model_input);
    input_buffer[ch].clear();
  }

  // Compress the power spectra.
  std::transform(
      model_input.begin(), model_input.end(), model_input.begin(),
      [](float a) { return std::pow(a, kSpectrumCompressionExponent); });
}
}  // namespace webrtc
