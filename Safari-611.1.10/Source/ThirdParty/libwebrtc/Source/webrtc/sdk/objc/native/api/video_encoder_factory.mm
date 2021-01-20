/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/api/video_encoder_factory.h"

#include <memory>

#include "sdk/objc/native/src/objc_video_encoder_factory.h"

namespace webrtc {

std::unique_ptr<VideoEncoderFactory> ObjCToNativeVideoEncoderFactory(
    id<RTCVideoEncoderFactory> objc_video_encoder_factory) {
  return std::make_unique<ObjCVideoEncoderFactory>(objc_video_encoder_factory);
}

}  // namespace webrtc
