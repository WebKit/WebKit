/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_NOISE_SPECTRUM_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_NOISE_SPECTRUM_ESTIMATOR_H_

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

class NoiseSpectrumEstimator {
 public:
  explicit NoiseSpectrumEstimator(ApmDataDumper* data_dumper);
  void Initialize();
  void Update(rtc::ArrayView<const float> spectrum, bool first_update);

  rtc::ArrayView<const float> GetNoiseSpectrum() const {
    return rtc::ArrayView<const float>(noise_spectrum_);
  }

 private:
  ApmDataDumper* data_dumper_;
  float noise_spectrum_[65];

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(NoiseSpectrumEstimator);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_NOISE_SPECTRUM_ESTIMATOR_H_
