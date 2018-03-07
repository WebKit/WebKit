/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_COMMON_TYPES_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_COMMON_TYPES_H_

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

// Ratio allocation between temporal streams:
// Values as required for the VP8 codec (accumulating).
static const float
    kVp8LayerRateAlloction[kMaxSimulcastStreams][kMaxTemporalStreams] = {
        {1.0f, 1.0f, 1.0f, 1.0f},  // 1 layer
        {0.6f, 1.0f, 1.0f, 1.0f},  // 2 layers {60%, 40%}
        {0.4f, 0.6f, 1.0f, 1.0f},  // 3 layers {40%, 20%, 40%}
        {0.25f, 0.4f, 0.6f, 1.0f}  // 4 layers {25%, 15%, 20%, 40%}
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_COMMON_TYPES_H_
