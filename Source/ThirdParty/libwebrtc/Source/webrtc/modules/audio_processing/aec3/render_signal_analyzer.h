/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_SIGNAL_ANALYZER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_SIGNAL_ANALYZER_H_

#include <array>
#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

namespace webrtc {

// Provides functionality for analyzing the properties of the render signal.
class RenderSignalAnalyzer {
 public:
  RenderSignalAnalyzer();
  ~RenderSignalAnalyzer();

  // Updates the render signal analysis with the most recent render signal.
  void Update(const FftBuffer& X_buffer,
              const rtc::Optional<size_t>& delay_partitions);

  // Returns true if the render signal is poorly exciting.
  bool PoorSignalExcitation() const {
    RTC_DCHECK_LT(2, narrow_band_counters_.size());
    return std::any_of(narrow_band_counters_.begin(),
                       narrow_band_counters_.end(),
                       [](size_t a) { return a > 10; });
  }

  // Zeros the array around regions with narrow bands signal characteristics.
  void MaskRegionsAroundNarrowBands(
      std::array<float, kFftLengthBy2Plus1>* v) const;

 private:
  std::array<size_t, kFftLengthBy2 - 1> narrow_band_counters_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RenderSignalAnalyzer);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_SIGNAL_ANALYZER_H_
