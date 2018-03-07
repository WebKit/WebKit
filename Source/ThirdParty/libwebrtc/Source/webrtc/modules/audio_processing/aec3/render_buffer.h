/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RENDER_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RENDER_BUFFER_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Provides a buffer of the render data for the echo remover.
class RenderBuffer {
 public:
  // The constructor takes, besides from the other parameters, a vector
  // containing the number of FFTs that will be included in the spectral sums in
  // the call to SpectralSum.
  RenderBuffer(Aec3Optimization optimization,
               size_t num_bands,
               size_t size,
               const std::vector<size_t> num_ffts_for_spectral_sums);
  ~RenderBuffer();

  // Clears the buffer.
  void Clear();

  // Insert a block into the buffer.
  void Insert(const std::vector<std::vector<float>>& block);

  // Gets the last inserted block.
  const std::vector<std::vector<float>>& MostRecentBlock() const {
    return last_block_;
  }

  // Get the spectrum from one of the FFTs in the buffer
  const std::array<float, kFftLengthBy2Plus1>& Spectrum(
      size_t buffer_offset_ffts) const {
    return spectrum_buffer_[(position_ + buffer_offset_ffts) %
                            fft_buffer_.size()];
  }

  // Returns the sum of the spectrums for a certain number of FFTs.
  const std::array<float, kFftLengthBy2Plus1>& SpectralSum(
      size_t num_ffts) const {
    RTC_DCHECK_EQ(spectral_sums_length_, num_ffts);
    return spectral_sums_[0];
  }

  // Returns the circular buffer.
  rtc::ArrayView<const FftData> Buffer() const { return fft_buffer_; }

  // Returns the current position in the circular buffer
  size_t Position() const { return position_; }

 private:
  const Aec3Optimization optimization_;
  std::vector<FftData> fft_buffer_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> spectrum_buffer_;
  size_t spectral_sums_length_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> spectral_sums_;
  size_t position_ = 0;
  std::vector<std::vector<float>> last_block_;
  const Aec3Fft fft_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderBuffer);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_RENDER_BUFFER_H_
