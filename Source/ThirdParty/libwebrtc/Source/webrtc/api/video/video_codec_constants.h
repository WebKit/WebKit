/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_CODEC_CONSTANTS_H_
#define API_VIDEO_VIDEO_CODEC_CONSTANTS_H_

#include <cstddef>

namespace webrtc {

inline constexpr size_t kMaxEncoderBuffers = 8;
inline constexpr size_t kMaxSimulcastStreams = 3;
inline constexpr size_t kMaxSpatialLayers = 5;
inline constexpr size_t kMaxTemporalStreams = 4;
inline constexpr size_t kMaxPreferredPixelFormats = 5;

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_CODEC_CONSTANTS_H_
