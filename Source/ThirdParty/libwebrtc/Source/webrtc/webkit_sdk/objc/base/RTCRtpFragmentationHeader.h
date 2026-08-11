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

NS_ASSUME_NONNULL_BEGIN

/** Information for header. Corresponds to webrtc::RTPFragmentationHeader. */
RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCRtpFragmentationHeader")))
@interface RTCRtpFragmentationHeader : NSObject

@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationOffset;
@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationLength;
@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationTimeDiff;
@property(nonatomic, strong) NSArray<NSNumber *> *fragmentationPlType;

@end

NS_ASSUME_NONNULL_END
