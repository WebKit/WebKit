/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_DOWNSAMPLED_RENDER_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_DOWNSAMPLED_RENDER_BUFFER_H_

#include <array>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

// Holds the circular buffer of the downsampled render data.
struct DownsampledRenderBuffer {
  DownsampledRenderBuffer();
  ~DownsampledRenderBuffer();
  std::array<float, kDownsampledRenderBufferSize> buffer = {};
  int position = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_DOWNSAMPLED_RENDER_BUFFER_H_
