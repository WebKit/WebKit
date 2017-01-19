/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_

#include "webrtc/modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class VP9Encoder : public VideoEncoder {
 public:
  static bool IsSupported();
  static VP9Encoder* Create();

  virtual ~VP9Encoder() {}
};

class VP9Decoder : public VideoDecoder {
 public:
  static bool IsSupported();
  static VP9Decoder* Create();

  virtual ~VP9Decoder() {}
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_
