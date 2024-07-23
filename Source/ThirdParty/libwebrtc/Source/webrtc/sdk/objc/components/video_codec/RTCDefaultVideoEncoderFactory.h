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
#import "RTCVideoEncoderFactory.h"

NS_ASSUME_NONNULL_BEGIN

/** This encoder factory include support for all codecs bundled with WebRTC. If using custom
 *  codecs, create custom implementations of RTCVideoEncoderFactory and RTCVideoDecoderFactory.
 */
RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCDefaultVideoEncoderFactory")))
@interface RTCDefaultVideoEncoderFactory : NSObject <RTCVideoEncoderFactory>

- (id)initWithH265:(bool)supportH265 vp9Profile0:(bool)supportsVP9Profile0 vp9Profile2:(bool)supportsVP9Profile2 lowLatencyH264:(bool)useLowLatencyH264 av1:(bool)supportAv1;
+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecs;
+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecsWithH265:(bool)supportsH265 vp9Profile0:(bool)supportsVP9Profile0 vp9Profile2:(bool)supportsVP9Profile2 av1:(bool)supportsAv1;

@end

NS_ASSUME_NONNULL_END
