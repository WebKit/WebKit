/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_DECIMATOR_BY_4_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_DECIMATOR_BY_4_H_

#include <array>

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/cascaded_biquad_filter.h"

namespace webrtc {

// Provides functionality for decimating a signal by 4.
class DecimatorBy4 {
 public:
  DecimatorBy4();

  // Downsamples the signal.
  void Decimate(rtc::ArrayView<const float> in,
                std::array<float, kSubBlockSize>* out);

 private:
  CascadedBiQuadFilter low_pass_filter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DecimatorBy4);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_DECIMATOR_BY_4_H_
