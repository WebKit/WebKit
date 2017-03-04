/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

NS_ASSUME_NONNULL_BEGIN

RTC_EXTERN NSString * const kRTCMediaConstraintsMinAspectRatio;
RTC_EXTERN NSString * const kRTCMediaConstraintsMaxAspectRatio;
RTC_EXTERN NSString * const kRTCMediaConstraintsMaxWidth;
RTC_EXTERN NSString * const kRTCMediaConstraintsMinWidth;
RTC_EXTERN NSString * const kRTCMediaConstraintsMaxHeight;
RTC_EXTERN NSString * const kRTCMediaConstraintsMinHeight;
RTC_EXTERN NSString * const kRTCMediaConstraintsMaxFrameRate;
RTC_EXTERN NSString * const kRTCMediaConstraintsMinFrameRate;
RTC_EXTERN NSString * const kRTCMediaConstraintsLevelControl;
/** The value for this key should be a base64 encoded string containing
 *  the data from the serialized configuration proto.
 */
RTC_EXTERN NSString * const kRTCMediaConstraintsAudioNetworkAdaptorConfig;

RTC_EXTERN NSString * const kRTCMediaConstraintsValueTrue;
RTC_EXTERN NSString * const kRTCMediaConstraintsValueFalse;

RTC_EXPORT
@interface RTCMediaConstraints : NSObject

- (instancetype)init NS_UNAVAILABLE;

/** Initialize with mandatory and/or optional constraints. */
- (instancetype)initWithMandatoryConstraints:
    (nullable NSDictionary<NSString *, NSString *> *)mandatory
                         optionalConstraints:
    (nullable NSDictionary<NSString *, NSString *> *)optional
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
