/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEO_VIDEO_CONTENT_TYPE_H_
#define WEBRTC_API_VIDEO_VIDEO_CONTENT_TYPE_H_

#include <stdint.h>

namespace webrtc {

enum class VideoContentType : uint8_t {
  UNSPECIFIED = 0,
  SCREENSHARE = 1,
  TOTAL_CONTENT_TYPES  // Must be the last value in the enum.
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEO_VIDEO_CONTENT_TYPE_H_
