/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC2_DIGITAL_GAIN_APPLIER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC2_DIGITAL_GAIN_APPLIER_H_

#include "webrtc/base/array_view.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"

namespace webrtc {

class DigitalGainApplier {
 public:
  DigitalGainApplier();

  // Applies the specified gain to an array of samples.
  void Process(float gain, rtc::ArrayView<float> samples);

 private:
  void LimitToAllowedRange(rtc::ArrayView<float> x);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC2_DIGITAL_GAIN_APPLIER_H_
