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

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

namespace webrtc {
namespace aec3 {
#if defined(WEBRTC_ARCH_X86_FAMILY)

void ComputeGains_SSE2(
    const std::array<float, kFftLengthBy2Plus1>& nearend_power,
    const std::array<float, kFftLengthBy2Plus1>& residual_echo_power,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise_power,
    float strong_nearend_margin,
    std::array<float, kFftLengthBy2 - 1>* previous_gain_squared,
    std::array<float, kFftLengthBy2 - 1>* previous_masker,
    std::array<float, kFftLengthBy2Plus1>* gain);

#endif

void ComputeGains(
    const std::array<float, kFftLengthBy2Plus1>& nearend_power,
    const std::array<float, kFftLengthBy2Plus1>& residual_echo_power,
    const std::array<float, kFftLengthBy2Plus1>& comfort_noise_power,
    float strong_nearend_margin,
    std::array<float, kFftLengthBy2 - 1>* previous_gain_squared,
    std::array<float, kFftLengthBy2 - 1>* previous_masker,
    std::array<float, kFftLengthBy2Plus1>* gain);

}  // namespace aec3

class SuppressionGain {
 public:
  explicit SuppressionGain(Aec3Optimization optimization);
  void GetGain(const std::array<float, kFftLengthBy2Plus1>& nearend_power,
               const std::array<float, kFftLengthBy2Plus1>& residual_echo_power,
               const std::array<float, kFftLengthBy2Plus1>& comfort_noise_power,
               float strong_nearend_margin,
               std::array<float, kFftLengthBy2Plus1>* gain);

 private:
  const Aec3Optimization optimization_;
  std::array<float, kFftLengthBy2 - 1> previous_gain_squared_;
  std::array<float, kFftLengthBy2 - 1> previous_masker_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SuppressionGain);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_GAIN_H_
