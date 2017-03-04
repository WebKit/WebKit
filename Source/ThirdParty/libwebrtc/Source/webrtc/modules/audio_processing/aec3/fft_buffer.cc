/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

FftBuffer::FftBuffer(Aec3Optimization optimization,
                     size_t num_partitions,
                     const std::vector<size_t> num_ffts_for_spectral_sums)
    : optimization_(optimization),
      fft_buffer_(num_partitions),
      spectrum_buffer_(num_partitions, std::array<float, kFftLengthBy2Plus1>()),
      spectral_sums_(num_ffts_for_spectral_sums.size(),
                     std::array<float, kFftLengthBy2Plus1>()) {
  // Current implementation only allows a maximum of one spectral sum lengths.
  RTC_DCHECK_EQ(1, num_ffts_for_spectral_sums.size());
  spectral_sums_length_ = num_ffts_for_spectral_sums[0];
  RTC_DCHECK_GE(fft_buffer_.size(), spectral_sums_length_);

  for (auto& sum : spectral_sums_) {
    sum.fill(0.f);
  }

  for (auto& spectrum : spectrum_buffer_) {
    spectrum.fill(0.f);
  }

  for (auto& fft : fft_buffer_) {
    fft.Clear();
  }
}

FftBuffer::~FftBuffer() = default;

void FftBuffer::Insert(const FftData& fft) {
  // Insert the fft into the buffer.
  position_ = (position_ - 1 + fft_buffer_.size()) % fft_buffer_.size();
  fft_buffer_[position_].Assign(fft);

  // Compute and insert the spectrum for the FFT into the spectrum buffer.
  fft.Spectrum(optimization_, &spectrum_buffer_[position_]);

  // Pre-compute and cachec the spectral sums.
  std::copy(spectrum_buffer_[position_].begin(),
            spectrum_buffer_[position_].end(), spectral_sums_[0].begin());
  size_t position = (position_ + 1) % fft_buffer_.size();
  for (size_t j = 1; j < spectral_sums_length_; ++j) {
    const std::array<float, kFftLengthBy2Plus1>& spectrum =
        spectrum_buffer_[position];

    for (size_t k = 0; k < spectral_sums_[0].size(); ++k) {
      spectral_sums_[0][k] += spectrum[k];
    }

    position = position < (fft_buffer_.size() - 1) ? position + 1 : 0;
  }
}

}  // namespace webrtc
