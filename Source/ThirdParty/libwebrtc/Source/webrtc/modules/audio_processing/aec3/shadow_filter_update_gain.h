/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SHADOW_FILTER_UPDATE_GAIN_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SHADOW_FILTER_UPDATE_GAIN_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"
#include "webrtc/modules/audio_processing/aec3/render_signal_analyzer.h"

namespace webrtc {

// Provides functionality for computing the fixed gain for the shadow filter.
class ShadowFilterUpdateGain {
 public:
  // Computes the gain.
  void Compute(const FftBuffer& X_buffer,
               const RenderSignalAnalyzer& render_signal_analyzer,
               const FftData& E_shadow,
               size_t size_partitions,
               bool saturated_capture_signal,
               FftData* G);

 private:
  size_t poor_signal_excitation_counter_ = 0;
  size_t call_counter_ = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SHADOW_FILTER_UPDATE_GAIN_H_
