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
#import "RTCMacros.h"

/** Class for H264 specific config. */
typedef NS_ENUM(NSUInteger, RTCH264PacketizationMode) {
  RTCH264PacketizationModeNonInterleaved = 0,  // Mode 1 - STAP-A, FU-A is allowed
  RTCH264PacketizationModeSingleNalUnit        // Mode 0 - only single NALU allowed
};

RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCCodecSpecificInfoH264")))
@interface RTCCodecSpecificInfoH264 : NSObject <RTCCodecSpecificInfo>

@property(nonatomic, assign) RTCH264PacketizationMode packetizationMode;

@end
