/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_

#include <array>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class SuppressionGain {
 public:
  explicit SuppressionGain(Aec3Optimization optimization);
  void GetGain(const std::array<float, kFftLengthBy2Plus1>& nearend,
               const std::array<float, kFftLengthBy2Plus1>& echo,
               const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
               bool saturated_echo,
               const std::vector<std::vector<float>>& render,
               bool force_zero_gain,
               float* high_bands_gain,
               std::array<float, kFftLengthBy2Plus1>* low_band_gain);

 private:
  void LowerBandGain(bool stationary_with_low_power,
                     bool saturated_echo,
                     const std::array<float, kFftLengthBy2Plus1>& nearend,
                     const std::array<float, kFftLengthBy2Plus1>& echo,
                     const std::array<float, kFftLengthBy2Plus1>& comfort_noise,
                     std::array<float, kFftLengthBy2Plus1>* gain);

  class LowNoiseRenderDetector {
   public:
    bool Detect(const std::vector<std::vector<float>>& render);

   private:
    float average_power_ = 32768.f * 32768.f;
  };

  const Aec3Optimization optimization_;
  std::array<float, kFftLengthBy2Plus1> last_gain_;
  std::array<float, kFftLengthBy2Plus1> last_masker_;
  std::array<float, kFftLengthBy2Plus1> gain_increase_;
  std::array<float, kFftLengthBy2Plus1> last_echo_;

  LowNoiseRenderDetector low_render_detector_;
  size_t no_saturation_counter_ = 0;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SuppressionGain);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_
