/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCVideoDecoderVP9.h"
#import "RTCWrappedNativeVideoDecoder.h"

#include "modules/video_coding/codecs/vp9/include/vp9.h"

@implementation RTCVideoDecoderVP9

+ (id<RTCVideoDecoder>)vp9Decoder {
  return [[RTCWrappedNativeVideoDecoder alloc]
      initWithNativeDecoder:std::unique_ptr<webrtc::VideoDecoder>(webrtc::VP9Decoder::Create())];
}

@end
