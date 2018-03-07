/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_

#include <stddef.h>
#include <array>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/render_buffer.h"

namespace webrtc {

// Class for buffering the incoming render blocks such that these may be
// extracted with a specified delay.
class RenderDelayBuffer {
 public:
  static RenderDelayBuffer* Create(size_t num_bands,
                                   size_t down_sampling_factor,
                                   size_t downsampled_render_buffer_size,
                                   size_t render_delay_buffer_size);
  virtual ~RenderDelayBuffer() = default;

  // Resets the buffer data.
  virtual void Reset() = 0;

  // Inserts a block into the buffer and returns true if the insert is
  // successful.
  virtual bool Insert(const std::vector<std::vector<float>>& block) = 0;

  // Updates the buffers one step based on the specified buffer delay. Returns
  // true if there was no overrun, otherwise returns false.
  virtual bool UpdateBuffers() = 0;

  // Sets the buffer delay.
  virtual void SetDelay(size_t delay) = 0;

  // Gets the buffer delay.
  virtual size_t Delay() const = 0;

  // Returns the render buffer for the echo remover.
  virtual const RenderBuffer& GetRenderBuffer() const = 0;

  // Returns the downsampled render buffer.
  virtual const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
