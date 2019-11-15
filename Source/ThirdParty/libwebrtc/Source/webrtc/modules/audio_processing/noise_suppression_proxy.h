/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_NOISE_SUPPRESSION_PROXY_H_
#define MODULES_AUDIO_PROCESSING_NOISE_SUPPRESSION_PROXY_H_

#include <vector>

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {
// This class ensures interoperability with the pointer-to-submodule interface
// AudioProcessing::noise_suppression() and AudioProcessing::ApplyConfig:
// Enable(..) and set_level(..) calls are applied via
// AudioProcessing::ApplyConfig, while all other function calls are forwarded
// directly to a wrapped NoiseSuppression instance.
class NoiseSuppressionProxy : public NoiseSuppression {
 public:
  NoiseSuppressionProxy(AudioProcessing* apm, NoiseSuppression* ns);
  ~NoiseSuppressionProxy() override;

  // NoiseSuppression implementation.
  int Enable(bool enable) override;
  bool is_enabled() const override;
  int set_level(Level level) override;
  Level level() const override;
  float speech_probability() const override;
  std::vector<float> NoiseEstimate() override;

 private:
  AudioProcessing* apm_;
  NoiseSuppression* ns_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(NoiseSuppressionProxy);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_NOISE_SUPPRESSION_PROXY_H_
