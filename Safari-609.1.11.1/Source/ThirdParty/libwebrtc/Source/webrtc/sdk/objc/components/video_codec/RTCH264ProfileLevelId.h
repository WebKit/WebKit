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

RTC_OBJC_EXPORT extern NSString *const kRTCVideoCodecH264Name;
RTC_OBJC_EXPORT extern NSString *const kRTCLevel31ConstrainedHigh;
RTC_OBJC_EXPORT extern NSString *const kRTCLevel31ConstrainedBaseline;
RTC_OBJC_EXPORT extern NSString *const kRTCMaxSupportedH264ProfileLevelConstrainedHigh;
RTC_OBJC_EXPORT extern NSString *const kRTCMaxSupportedH264ProfileLevelConstrainedBaseline;

/** H264 Profiles and levels. */
typedef NS_ENUM(NSUInteger, RTCH264Profile) {
  RTCH264ProfileConstrainedBaseline,
  RTCH264ProfileBaseline,
  RTCH264ProfileMain,
  RTCH264ProfileConstrainedHigh,
  RTCH264ProfileHigh,
};

typedef NS_ENUM(NSUInteger, RTCH264Level) {
  RTCH264Level1_b = 0,
  RTCH264Level1 = 10,
  RTCH264Level1_1 = 11,
  RTCH264Level1_2 = 12,
  RTCH264Level1_3 = 13,
  RTCH264Level2 = 20,
  RTCH264Level2_1 = 21,
  RTCH264Level2_2 = 22,
  RTCH264Level3 = 30,
  RTCH264Level3_1 = 31,
  RTCH264Level3_2 = 32,
  RTCH264Level4 = 40,
  RTCH264Level4_1 = 41,
  RTCH264Level4_2 = 42,
  RTCH264Level5 = 50,
  RTCH264Level5_1 = 51,
  RTCH264Level5_2 = 52
};

RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCH264ProfileLevelId")))
@interface RTCH264ProfileLevelId : NSObject

@property(nonatomic, readonly) RTCH264Profile profile;
@property(nonatomic, readonly) RTCH264Level level;
@property(nonatomic, readonly) NSString *hexString;

- (instancetype)initWithHexString:(NSString *)hexString;
- (instancetype)initWithProfile:(RTCH264Profile)profile level:(RTCH264Level)level;

@end
