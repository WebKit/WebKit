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
#import "RTCRtpFragmentationHeader.h"
#import "RTCVideoEncoderQpThresholds.h"
#import "RTCVideoEncoderSettings.h"
#import "RTCVideoFrame.h"

NS_ASSUME_NONNULL_BEGIN

/** Callback block for encoder. */
typedef BOOL (^RTCVideoEncoderCallback)(RTCEncodedImage *frame,
                                        id<RTCCodecSpecificInfo> info,
                                        RTCRtpFragmentationHeader* __nullable header);

typedef void (^RTCVideoEncoderDescriptionCallback)(const uint8_t *frame, size_t size);

/** Protocol for encoder implementations. */
RTC_OBJC_EXPORT
@protocol RTCVideoEncoder <NSObject>

- (void)setCallback:(RTCVideoEncoderCallback)callback;
- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores;
- (NSInteger)releaseEncoder;
- (NSInteger)encode:(RTCVideoFrame *)frame
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info
           frameTypes:(NSArray<NSNumber *> *)frameTypes;
- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate;
- (NSString *)implementationName;

/** Returns QP scaling settings for encoder. The quality scaler adjusts the resolution in order to
 *  keep the QP from the encoded images within the given range. Returning nil from this function
 *  disables quality scaling. */
- (nullable RTCVideoEncoderQpThresholds *)scalingSettings;

@end

NS_ASSUME_NONNULL_END
