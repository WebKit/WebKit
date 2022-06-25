/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoDecoder.h"

RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoDecoderH264")))
@interface RTCVideoDecoderH264 : NSObject <RTCVideoDecoder>
- (NSInteger)decodeData:(const uint8_t *)data
    size:(size_t)size
    timeStamp:(uint32_t)timeStamp;
@end
