/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_BIQUAD_FILTER_H_
#define MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_BIQUAD_FILTER_H_

#include <vector>

#include "api/array_view.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class BiQuadFilter {
 public:
  struct BiQuadCoefficients {
    float b[3];
    float a[2];
  };

  BiQuadFilter() = default;

  void Initialize(const BiQuadCoefficients& coefficients) {
    coefficients_ = coefficients;
  }

  // Produces a filtered output y of the input x. Both x and y need to
  // have the same length.
  void Process(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);

 private:
  struct BiQuadState {
    BiQuadState() {
      std::fill(b, b + arraysize(b), 0.f);
      std::fill(a, a + arraysize(a), 0.f);
    }

    float b[2];
    float a[2];
  };

  BiQuadState biquad_state_;
  BiQuadCoefficients coefficients_;

  RTC_DISALLOW_COPY_AND_ASSIGN(BiQuadFilter);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_BIQUAD_FILTER_H_
