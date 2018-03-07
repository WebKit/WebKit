/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_DOWNSAMPLED_RENDER_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_DOWNSAMPLED_RENDER_BUFFER_H_

#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

// Holds the circular buffer of the downsampled render data.
struct DownsampledRenderBuffer {
  explicit DownsampledRenderBuffer(size_t downsampled_buffer_size);
  ~DownsampledRenderBuffer();
  std::vector<float> buffer;
  int position = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_DOWNSAMPLED_RENDER_BUFFER_H_
