/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_INTERNAL_DEFINES_H_
#define WEBRTC_MODULES_VIDEO_CODING_INTERNAL_DEFINES_H_

#include "webrtc/typedefs.h"

namespace webrtc {

#define MASK_32_BITS(x) (0xFFFFFFFF & (x))

inline uint32_t MaskWord64ToUWord32(int64_t w64) {
  return static_cast<uint32_t>(MASK_32_BITS(w64));
}

#define VCM_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define VCM_MIN(a, b) (((a) < (b)) ? (a) : (b))

#define VCM_DEFAULT_CODEC_WIDTH 352
#define VCM_DEFAULT_CODEC_HEIGHT 288
#define VCM_DEFAULT_FRAME_RATE 30
#define VCM_MIN_BITRATE 30
#define VCM_FLUSH_INDICATOR 4

#define VCM_NO_RECEIVER_ID 0

inline int32_t VCMId(const int32_t vcmId, const int32_t receiverId = 0) {
  return static_cast<int32_t>((vcmId << 16) + receiverId);
}

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_INTERNAL_DEFINES_H_
