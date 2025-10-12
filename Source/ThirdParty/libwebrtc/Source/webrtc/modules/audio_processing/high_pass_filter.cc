/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/high_pass_filter.h"

#include <array>
#include <cstddef>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/utility/cascaded_biquad_filter.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
// [B,A] = butter(2,100/8000,'high')
constexpr std::array<CascadedBiQuadFilter::BiQuadCoefficients, 1>
    kHighPassFilterCoefficients16kHz = {{
        {{0.972613898f, -1.945227797f, 0.972613898f},
         {-1.944477658f, 0.945977936f}},
    }};

// [B,A] = butter(2,100/16000,'high')
constexpr std::array<CascadedBiQuadFilter::BiQuadCoefficients, 1>
    kHighPassFilterCoefficients32kHz = {{
        {{0.986211925f, -1.972423849f, 0.986211925f},
         {-1.972233729f, 0.972613969f}},
    }};

// [B,A] = butter(2,100/24000,'high')
constexpr std::array<CascadedBiQuadFilter::BiQuadCoefficients, 1>
    kHighPassFilterCoefficients48kHz = {{
        {{0.990786698f, -1.981573396f, 0.990786698f},
         {-1.981488509f, 0.981658283f}},
    }};

ArrayView<const CascadedBiQuadFilter::BiQuadCoefficients> ChooseCoefficients(
    int sample_rate_hz) {
  switch (sample_rate_hz) {
    case 16000:
      return ArrayView<const CascadedBiQuadFilter::BiQuadCoefficients>(
          kHighPassFilterCoefficients16kHz);
    case 32000:
      return ArrayView<const CascadedBiQuadFilter::BiQuadCoefficients>(
          kHighPassFilterCoefficients32kHz);
    case 48000:
      return ArrayView<const CascadedBiQuadFilter::BiQuadCoefficients>(
          kHighPassFilterCoefficients48kHz);
    default:
      RTC_DCHECK_NOTREACHED();
  }
  RTC_DCHECK_NOTREACHED();
  return ArrayView<const CascadedBiQuadFilter::BiQuadCoefficients>(
      kHighPassFilterCoefficients16kHz);
}

}  // namespace

HighPassFilter::HighPassFilter(int sample_rate_hz, size_t num_channels)
    : sample_rate_hz_(sample_rate_hz) {
  filters_.resize(num_channels);
  auto coefficients = ChooseCoefficients(sample_rate_hz_);
  for (size_t k = 0; k < filters_.size(); ++k) {
    filters_[k].reset(new CascadedBiQuadFilter(coefficients));
  }
}

HighPassFilter::~HighPassFilter() = default;

void HighPassFilter::Process(AudioBuffer* audio, bool use_split_band_data) {
  RTC_DCHECK(audio);
  RTC_DCHECK_EQ(filters_.size(), audio->num_channels());
  if (use_split_band_data) {
    for (size_t k = 0; k < audio->num_channels(); ++k) {
      ArrayView<float> channel_data = ArrayView<float>(
          audio->split_bands(k)[0], audio->num_frames_per_band());
      filters_[k]->Process(channel_data);
    }
  } else {
    for (size_t k = 0; k < audio->num_channels(); ++k) {
      ArrayView<float> channel_data =
          ArrayView<float>(&audio->channels()[k][0], audio->num_frames());
      filters_[k]->Process(channel_data);
    }
  }
}

void HighPassFilter::Process(std::vector<std::vector<float>>* audio) {
  RTC_DCHECK_EQ(filters_.size(), audio->size());
  for (size_t k = 0; k < audio->size(); ++k) {
    filters_[k]->Process((*audio)[k]);
  }
}

void HighPassFilter::Reset() {
  for (size_t k = 0; k < filters_.size(); ++k) {
    filters_[k]->Reset();
  }
}

void HighPassFilter::Reset(size_t num_channels) {
  const size_t old_num_channels = filters_.size();
  filters_.resize(num_channels);
  if (filters_.size() < old_num_channels) {
    Reset();
  } else {
    for (size_t k = 0; k < old_num_channels; ++k) {
      filters_[k]->Reset();
    }
    const auto& coefficients = ChooseCoefficients(sample_rate_hz_);
    for (size_t k = old_num_channels; k < filters_.size(); ++k) {
      filters_[k].reset(new CascadedBiQuadFilter(coefficients));
    }
  }
}

}  // namespace webrtc
