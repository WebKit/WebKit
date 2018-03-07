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

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCVideoCodec.h>

NS_ASSUME_NONNULL_BEGIN

/** RTCVideoEncoderFactory is an Objective-C version of webrtc::VideoEncoderFactory. */
RTC_EXPORT
@protocol RTCVideoEncoderFactory <NSObject>

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info;
- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@end

/** RTCVideoDecoderFactory is an Objective-C version of webrtc::VideoDecoderFactory. */
RTC_EXPORT
@protocol RTCVideoDecoderFactory <NSObject>

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info;
- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs;  // TODO(andersc): "supportedFormats" instead?

@end

#pragma mark - Default factories

/** These codec factories include support for all codecs bundled with WebRTC. If using custom
 *  codecs, create custom implementations of RTCVideoEncoderFactory and RTCVideoDecoderFactory.
 */
RTC_EXPORT
@interface RTCDefaultVideoEncoderFactory : NSObject <RTCVideoEncoderFactory>

@property(nonatomic, retain) RTCVideoCodecInfo *preferredCodec;

+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecs;

@end

RTC_EXPORT
@interface RTCDefaultVideoDecoderFactory : NSObject <RTCVideoDecoderFactory>
@end

NS_ASSUME_NONNULL_END
