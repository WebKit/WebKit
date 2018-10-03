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
#import <WebRTC/RTCVideoCodecFactory.h>

/** Class for H264 specific config. */
typedef NS_ENUM(NSUInteger, RTCH264PacketizationMode) {
  RTCH264PacketizationModeNonInterleaved = 0,  // Mode 1 - STAP-A, FU-A is allowed
  RTCH264PacketizationModeSingleNalUnit        // Mode 0 - only single NALU allowed
};

RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCCodecSpecificInfoH264")))
@interface RTCCodecSpecificInfoH264 : NSObject <RTCCodecSpecificInfo>

@property(nonatomic, assign) RTCH264PacketizationMode packetizationMode;
@property(nonatomic, assign) int simulcastIndex;

@end

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

RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCH264ProfileLevelId")))
@interface RTCH264ProfileLevelId : NSObject

@property(nonatomic, readonly) RTCH264Profile profile;
@property(nonatomic, readonly) RTCH264Level level;
@property(nonatomic, readonly) NSString *hexString;

- (instancetype)initWithHexString:(NSString *)hexString;
- (instancetype)initWithProfile:(RTCH264Profile)profile level:(RTCH264Level)level;

@end

/** Encoder. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoEncoderH264")))
@interface RTCVideoEncoderH264 : NSObject <RTCVideoEncoder>

- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo *)codecInfo;

@end

/** Decoder. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoDecoderH264")))
@interface RTCVideoDecoderH264 : NSObject <RTCVideoDecoder>
@end

/** Encoder factory. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoEncoderFactoryH264")))
@interface RTCVideoEncoderFactoryH264 : NSObject <RTCVideoEncoderFactory>
@end

/** Decoder factory. */
RTC_EXPORT
__attribute__((objc_runtime_name("WK_RTCVideoDecoderFactoryH264")))
@interface RTCVideoDecoderFactoryH264 : NSObject <RTCVideoDecoderFactory>
@end
