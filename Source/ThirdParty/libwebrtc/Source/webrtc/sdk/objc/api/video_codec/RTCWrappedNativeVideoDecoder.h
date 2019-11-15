/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "base/RTCVideoDecoder.h"

#include "api/video_codecs/video_decoder.h"
#include "media/base/codec.h"

__attribute__((objc_runtime_name("WK_RTCWrappedNativeVideoDecoder")))
@interface RTCWrappedNativeVideoDecoder : NSObject <RTCVideoDecoder>

- (instancetype)initWithNativeDecoder:(std::unique_ptr<webrtc::VideoDecoder>)decoder;

/* This moves the ownership of the wrapped decoder to the caller. */
- (std::unique_ptr<webrtc::VideoDecoder>)releaseWrappedDecoder;

@end
