/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoDecoderAV1.h"
#import "RTCWrappedNativeVideoDecoder.h"

#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"

@implementation RTC_OBJC_TYPE (RTCVideoDecoderAV1)

+ (id<RTC_OBJC_TYPE(RTCVideoDecoder)>)av1Decoder {
  std::unique_ptr<webrtc::VideoDecoder> nativeDecoder(webrtc::CreateLibaomAv1Decoder());
  if (nativeDecoder == nullptr) {
    return nil;
  }
  return [[RTC_OBJC_TYPE(RTCWrappedNativeVideoDecoder) alloc]
      initWithNativeDecoder:std::move(nativeDecoder)];
}

+ (bool)isSupported {
  return webrtc::kIsLibaomAv1DecoderSupported;
}

@end
