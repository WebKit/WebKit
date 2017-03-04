/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_

#include <stddef.h>
#include <vector>

namespace webrtc {

// Class for buffering the incoming render blocks such that these may be
// extracted with a specified delay.
class RenderDelayBuffer {
 public:
  static RenderDelayBuffer* Create(size_t size_blocks,
                                   size_t num_bands,
                                   size_t max_api_jitter_blocks);
  virtual ~RenderDelayBuffer() = default;

  // Swaps a block into the buffer (the content of block is destroyed) and
  // returns true if the insert is successful.
  virtual bool Insert(std::vector<std::vector<float>>* block) = 0;

  // Gets a reference to the next block (having the specified buffer delay) to
  // read in the buffer. This method can only be called if a block is
  // available which means that that must be checked prior to the call using
  // the method IsBlockAvailable().
  virtual const std::vector<std::vector<float>>& GetNext() = 0;

  // Sets the buffer delay. The delay set must be lower than the delay reported
  // by MaxDelay().
  virtual void SetDelay(size_t delay) = 0;

  // Gets the buffer delay.
  virtual size_t Delay() const = 0;

  // Returns the maximum allowed buffer delay increase.
  virtual size_t MaxDelay() const = 0;

  // Returns whether a block is available for reading.
  virtual bool IsBlockAvailable() const = 0;

  // Returns the maximum allowed api call jitter in blocks.
  virtual size_t MaxApiJitter() const = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
