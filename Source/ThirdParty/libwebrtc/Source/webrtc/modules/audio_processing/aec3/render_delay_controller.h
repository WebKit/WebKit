/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_CONTROLLER_H_

#include "webrtc/base/array_view.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// Class for aligning the render and capture signal using a RenderDelayBuffer.
class RenderDelayController {
 public:
  static RenderDelayController* Create(int sample_rate_hz);
  virtual ~RenderDelayController() = default;

  // Resets the delay controller.
  virtual void Reset() = 0;

  // Receives the externally used delay.
  virtual void SetDelay(size_t render_delay) = 0;

  // Aligns the render buffer content with the capture signal.
  virtual size_t GetDelay(const DownsampledRenderBuffer& render_buffer,
                          rtc::ArrayView<const float> capture) = 0;

  // Returns an approximate value for the headroom in the buffer alignment.
  virtual rtc::Optional<size_t> AlignmentHeadroomSamples() const = 0;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_CONTROLLER_H_
