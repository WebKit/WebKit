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

#import "RTCCodecSpecificInfo.h"
#import "RTCEncodedImage.h"
#import "RTCMacros.h"
#import "RTCVideoEncoderSettings.h"
#import "RTCVideoFrame.h"

NS_ASSUME_NONNULL_BEGIN

/** Callback block for decoder. */
typedef void (^RTCVideoDecoderCallback)(RTCVideoFrame *frame);

/** Protocol for decoder implementations. */
RTC_OBJC_EXPORT
@protocol RTCVideoDecoder <NSObject>

- (void)setCallback:(RTCVideoDecoderCallback)callback;
- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores;
- (NSInteger)releaseDecoder;
- (NSInteger)decode:(RTCEncodedImage *)encodedImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
         renderTimeMs:(int64_t)renderTimeMs;
- (NSString *)implementationName;

@end

NS_ASSUME_NONNULL_END
