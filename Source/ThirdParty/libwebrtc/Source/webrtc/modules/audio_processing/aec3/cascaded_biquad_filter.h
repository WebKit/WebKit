/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_CASCADED_BIQUAD_FILTER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_CASCADED_BIQUAD_FILTER_H_

#include <vector>

#include "api/array_view.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Applies a number of identical biquads in a cascaded manner. The filter
// implementation is direct form 1.
class CascadedBiQuadFilter {
 public:
  struct BiQuadState {
    BiQuadState() : x(), y() {}
    float x[2];
    float y[2];
  };

  struct BiQuadCoefficients {
    float b[3];
    float a[2];
  };

  CascadedBiQuadFilter(
      const CascadedBiQuadFilter::BiQuadCoefficients& coefficients,
      size_t num_biquads);
  ~CascadedBiQuadFilter();
  // Applies the biquads on the values in x in order to form the output in y.
  void Process(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);
  // Applies the biquads on the values in y in an in-place manner.
  void Process(rtc::ArrayView<float> y);

 private:
  void ApplyBiQuad(rtc::ArrayView<const float> x,
                   rtc::ArrayView<float> y,
                   CascadedBiQuadFilter::BiQuadState* biquad_state);

  std::vector<BiQuadState> biquad_states_;
  const BiQuadCoefficients coefficients_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CascadedBiQuadFilter);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_CASCADED_BIQUAD_FILTER_H_
