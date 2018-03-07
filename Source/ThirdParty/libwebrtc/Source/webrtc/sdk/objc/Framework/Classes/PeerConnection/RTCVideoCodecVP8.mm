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

#import "RTCWrappedNativeVideoDecoder.h"
#import "RTCWrappedNativeVideoEncoder.h"
#import "WebRTC/RTCVideoDecoderVP8.h"
#import "WebRTC/RTCVideoEncoderVP8.h"

#include "modules/video_coding/codecs/vp8/include/vp8.h"

#pragma mark - Encoder

@implementation RTCVideoEncoderVP8

+ (id<RTCVideoEncoder>)vp8Encoder {
  return [[RTCWrappedNativeVideoEncoder alloc]
      initWithNativeEncoder:std::unique_ptr<webrtc::VideoEncoder>(webrtc::VP8Encoder::Create())];
}

@end

#pragma mark - Decoder

@implementation RTCVideoDecoderVP8

+ (id<RTCVideoDecoder>)vp8Decoder {
  return [[RTCWrappedNativeVideoDecoder alloc]
      initWithNativeDecoder:std::unique_ptr<webrtc::VideoDecoder>(webrtc::VP8Decoder::Create())];
}

@end
