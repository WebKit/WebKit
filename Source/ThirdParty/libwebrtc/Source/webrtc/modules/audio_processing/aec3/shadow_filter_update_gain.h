/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SHADOW_FILTER_UPDATE_GAIN_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SHADOW_FILTER_UPDATE_GAIN_H_

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/render_signal_analyzer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Provides functionality for computing the fixed gain for the shadow filter.
class ShadowFilterUpdateGain {
 public:
  // Takes action in the case of a known echo path change.
  void HandleEchoPathChange();

  // Computes the gain.
  void Compute(const RenderBuffer& render_buffer,
               const RenderSignalAnalyzer& render_signal_analyzer,
               const FftData& E_shadow,
               size_t size_partitions,
               bool saturated_capture_signal,
               FftData* G);

 private:
  // TODO(peah): Check whether this counter should instead be initialized to a
  // large value.
  size_t poor_signal_excitation_counter_ = 0;
  size_t call_counter_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SHADOW_FILTER_UPDATE_GAIN_H_
