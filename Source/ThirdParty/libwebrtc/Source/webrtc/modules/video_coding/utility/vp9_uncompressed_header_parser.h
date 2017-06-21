/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

namespace vp9 {

// Gets the QP, QP range: [0, 255].
// Returns true on success, false otherwise.
bool GetQp(const uint8_t* buf, size_t length, int* qp);

}  // namespace vp9

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_
