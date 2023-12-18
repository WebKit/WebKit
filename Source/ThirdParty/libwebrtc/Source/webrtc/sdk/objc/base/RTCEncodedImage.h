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
#import "RTCVideoFrame.h"

NS_ASSUME_NONNULL_BEGIN

/** Represents an encoded frame's type. */
typedef NS_ENUM(NSUInteger, RTCFrameType) {
  RTCFrameTypeEmptyFrame = 0,
  RTCFrameTypeAudioFrameSpeech = 1,
  RTCFrameTypeAudioFrameCN = 2,
  RTCFrameTypeVideoFrameKey = 3,
  RTCFrameTypeVideoFrameDelta = 4,
};

typedef NS_ENUM(NSUInteger, RTCVideoContentType) {
  RTCVideoContentTypeUnspecified,
  RTCVideoContentTypeScreenshare,
};

/** Represents an encoded frame. Corresponds to webrtc::EncodedImage. */
RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCEncodedImage")))
@interface RTCEncodedImage : NSObject

@property(nonatomic, strong) NSData *buffer;
@property(nonatomic, assign) int32_t encodedWidth;
@property(nonatomic, assign) int32_t encodedHeight;
@property(nonatomic, assign) int64_t timeStamp;
@property(nonatomic, assign) uint64_t duration;
@property(nonatomic, assign) int64_t captureTimeMs;
@property(nonatomic, assign) int64_t ntpTimeMs;
@property(nonatomic, assign) uint8_t flags;
@property(nonatomic, assign) int64_t encodeStartMs;
@property(nonatomic, assign) int64_t encodeFinishMs;
@property(nonatomic, assign) RTCFrameType frameType;
@property(nonatomic, assign) RTCVideoRotation rotation;
@property(nonatomic, assign) BOOL completeFrame;
@property(nonatomic, assign) int32_t temporalIndex;
@property(nonatomic, strong) NSNumber *qp;
@property(nonatomic, assign) RTCVideoContentType contentType;

@end

NS_ASSUME_NONNULL_END
