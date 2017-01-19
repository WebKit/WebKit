/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_

#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)

#include <Availability.h>
#if defined(WEBRTC_IOS) || \
    defined(__MAC_10_8) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_8
#define WEBRTC_VIDEO_TOOLBOX_SUPPORTED 1
#endif

#endif  // defined(WEBRTC_IOS) || defined(WEBRTC_MAC)

#include "webrtc/modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

// Set to disable the H.264 encoder/decoder implementations that are provided if
// |rtc_use_h264| build flag is true (if false, this function does nothing).
// This function should only be called before or during WebRTC initialization
// and is not thread-safe.
void DisableRtcUseH264();

class H264Encoder : public VideoEncoder {
 public:
  static H264Encoder* Create();
  // If H.264 is supported (any implementation).
  static bool IsSupported();

  ~H264Encoder() override {}
};

class H264Decoder : public VideoDecoder {
 public:
  static H264Decoder* Create();
  static bool IsSupported();

  ~H264Decoder() override {}
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_
