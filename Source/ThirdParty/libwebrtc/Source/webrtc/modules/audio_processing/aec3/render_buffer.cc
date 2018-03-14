/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_buffer.h"

#include <algorithm>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

RenderBuffer::RenderBuffer(MatrixBuffer* block_buffer,
                           VectorBuffer* spectrum_buffer,
                           FftBuffer* fft_buffer)
    : block_buffer_(block_buffer),
      spectrum_buffer_(spectrum_buffer),
      fft_buffer_(fft_buffer) {
  RTC_DCHECK(block_buffer_);
  RTC_DCHECK(spectrum_buffer_);
  RTC_DCHECK(fft_buffer_);
  RTC_DCHECK_EQ(block_buffer_->buffer.size(), fft_buffer_->buffer.size());
  RTC_DCHECK_EQ(spectrum_buffer_->buffer.size(), fft_buffer_->buffer.size());
  RTC_DCHECK_EQ(spectrum_buffer_->read, fft_buffer_->read);
  RTC_DCHECK_EQ(spectrum_buffer_->write, fft_buffer_->write);
}

RenderBuffer::~RenderBuffer() = default;

void RenderBuffer::SpectralSum(
    size_t num_spectra,
    std::array<float, kFftLengthBy2Plus1>* X2) const {
  X2->fill(0.f);
  int position = spectrum_buffer_->read;
  for (size_t j = 0; j < num_spectra; ++j) {
    std::transform(X2->begin(), X2->end(),
                   spectrum_buffer_->buffer[position].begin(), X2->begin(),
                   std::plus<float>());
    position = spectrum_buffer_->IncIndex(position);
  }
}

}  // namespace webrtc
