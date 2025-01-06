/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_API_VIDEO_ENCODER_FACTORY_H_
#define SDK_OBJC_NATIVE_API_VIDEO_ENCODER_FACTORY_H_

#include <memory>

#import "base/RTCVideoEncoderFactory.h"

#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

std::unique_ptr<VideoEncoderFactory> ObjCToNativeVideoEncoderFactory(
    id<RTCVideoEncoderFactory> objc_video_encoder_factory);

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_API_VIDEO_ENCODER_FACTORY_H_
