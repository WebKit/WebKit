/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/decimator.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// b, a = signal.butter(2, 3400/8000.0, 'lowpass', analog=False) which are the
// same as b, a = signal.butter(2, 1700/4000.0, 'lowpass', analog=False).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients2 = {
    {0.22711796f, 0.45423593f, 0.22711796f},
    {-0.27666461f, 0.18513647f}};
constexpr int kNumFilters2 = 3;

// b, a = signal.butter(2, 1500/8000.0, 'lowpass', analog=False) which are the
// same as b, a = signal.butter(2, 75/4000.0, 'lowpass', analog=False).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients4 = {
    {0.0179f, 0.0357f, 0.0179f},
    {-1.5879f, 0.6594f}};
constexpr int kNumFilters4 = 3;

// b, a = signal.butter(2, 800/8000.0, 'lowpass', analog=False) which are the
// same as b, a = signal.butter(2, 400/4000.0, 'lowpass', analog=False).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients8 = {
    {0.02008337f, 0.04016673f, 0.02008337f},
    {-1.56101808f, 0.64135154f}};
constexpr int kNumFilters8 = 4;

}  // namespace

Decimator::Decimator(size_t down_sampling_factor)
    : down_sampling_factor_(down_sampling_factor),
      low_pass_filter_(
          down_sampling_factor_ == 4
              ? kLowPassFilterCoefficients4
              : (down_sampling_factor_ == 8 ? kLowPassFilterCoefficients8
                                            : kLowPassFilterCoefficients2),
          down_sampling_factor_ == 4
              ? kNumFilters4
              : (down_sampling_factor_ == 8 ? kNumFilters8 : kNumFilters2)) {
  RTC_DCHECK(down_sampling_factor_ == 2 || down_sampling_factor_ == 4 ||
             down_sampling_factor_ == 8);
}

void Decimator::Decimate(rtc::ArrayView<const float> in,
                         rtc::ArrayView<float> out) {
  RTC_DCHECK_EQ(kBlockSize, in.size());
  RTC_DCHECK_EQ(kBlockSize / down_sampling_factor_, out.size());
  std::array<float, kBlockSize> x;

  // Limit the frequency content of the signal to avoid aliasing.
  low_pass_filter_.Process(in, x);

  // Downsample the signal.
  for (size_t j = 0, k = 0; j < out.size(); ++j, k += down_sampling_factor_) {
    RTC_DCHECK_GT(kBlockSize, k);
    out[j] = x[k];
  }
}

}  // namespace webrtc
