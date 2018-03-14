/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  WEBRTC VP8 wrapper interface
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_H_

#include <memory>

#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class VP8Encoder : public VideoEncoder {
 public:
  static bool IsSupported();
  static std::unique_ptr<VP8Encoder> Create();

  virtual ~VP8Encoder() {}
};  // end of VP8Encoder class

class VP8Decoder : public VideoDecoder {
 public:
  static bool IsSupported();
  static std::unique_ptr<VP8Decoder> Create();

  virtual ~VP8Decoder() {}
};  // end of VP8Decoder class
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_H_
