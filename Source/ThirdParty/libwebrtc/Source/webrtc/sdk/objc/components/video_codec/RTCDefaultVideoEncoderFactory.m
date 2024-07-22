/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDefaultVideoEncoderFactory.h"

#import "RTCH264ProfileLevelId.h"
#import "RTCVideoEncoderH264.h"
#import "api/video_codec/RTCVideoCodecConstants.h"
#import "api/video_codec/RTCVideoEncoderVP8.h"
#import "base/RTCVideoCodecInfo.h"
#if defined(RTC_ENABLE_VP9)
#import "api/video_codec/RTCVideoEncoderVP9.h"
#endif
#if !defined(DISABLE_H265)
#import "RTCH265ProfileLevelId.h"
#import "RTCVideoEncoderH265.h"
#endif
#if !defined(DISABLE_RTC_AV1)
#import "sdk/objc/api/video_codec/RTCVideoEncoderAV1.h"
#endif

@implementation RTCDefaultVideoEncoderFactory {
  bool _supportsH265;
  bool _supportsVP9Profile0;
  bool _supportsVP9Profile2;
  bool _useLowLatencyH264;
  bool _supportsAv1;
}

- (id)initWithH265:(bool)supportsH265 vp9Profile0:(bool)supportsVP9Profile0 vp9Profile2:(bool)supportsVP9Profile2 lowLatencyH264:(bool)useLowLatencyH264 av1:(bool)supportsAv1
{
  self = [super init];
  if (self) {
    _supportsH265 = supportsH265;
    _supportsVP9Profile0 = supportsVP9Profile0;
    _supportsVP9Profile2 = supportsVP9Profile2;
    _useLowLatencyH264 = useLowLatencyH264;
    _supportsAv1 = supportsAv1;
  }
  return self;
}

+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
    return [self supportedCodecsWithH265:true vp9Profile0:true vp9Profile2:true av1:true];
}

+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecsWithH265:(bool)supportsH265 vp9Profile0:(bool)supportsVP9Profile0 vp9Profile2:(bool)supportsVP9Profile2 av1:(bool)supportsAv1 {

   NSMutableArray<RTCVideoCodecInfo *> *codecs = [[NSMutableArray alloc] initWithCapacity:8];

  [codecs addObject: [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name parameters: @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  }]];
  [codecs addObject: [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name parameters: @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  }]];
  [codecs addObject: [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name parameters: @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"0",
  }]];
  [codecs addObject: [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name parameters: @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"0",
  }]];

#if !defined(RTC_DISABLE_H265)
  if (supportsH265) {
    RTCVideoCodecInfo *h265Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH265Name];
    [codecs addObject:h265Info];
  }
#endif

  RTCVideoCodecInfo *vp8Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp8Name];
  [codecs addObject:vp8Info];

#if defined(RTC_ENABLE_VP9)
  if (supportsVP9Profile0) {
    [codecs addObject:[[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name parameters: @{
      @"profile-id" : @"0",
    }]];
  }
  if (supportsVP9Profile2) {
    [codecs addObject:[[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name parameters: @{
      @"profile-id" : @"2",
    }]];
  }
#endif
#if !defined(DISABLE_RTC_AV1)
  if (supportsAv1) {
    [codecs addObject:[[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecAv1Name]];
  }
#endif

  return codecs;
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  if ([info.name isEqualToString:kRTCVideoCodecH264Name]) {
    RTCVideoEncoderH264* encoder = [[RTCVideoEncoderH264 alloc] initWithCodecInfo:info];
    [encoder setH264LowLatencyEncoderEnabled:_useLowLatencyH264];
    return encoder;
  } else if ([info.name isEqualToString:kRTCVideoCodecVp8Name]) {
    return [RTCVideoEncoderVP8 vp8Encoder];
#if defined(RTC_ENABLE_VP9)
  } else if ([info.name isEqualToString:kRTCVideoCodecVp9Name]) {
    return [RTCVideoEncoderVP9 vp9Encoder:info];
#endif
#if !defined(DISABLE_H265)
  } else if (@available(iOS 11, *)) {
    if ([info.name isEqualToString:kRTCVideoCodecH265Name]) {
      return [[RTCVideoEncoderH265 alloc] initWithCodecInfo:info];
    }
#endif
  }
#if !defined(DISABLE_RTC_AV1)
  if ([info.name isEqualToString:kRTCVideoCodecAv1Name]) {
    return [RTCVideoEncoderAV1 av1Encoder];
  }
#endif

  return nil;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [[self class] supportedCodecsWithH265:_supportsH265 vp9Profile0:_supportsVP9Profile0 vp9Profile2: _supportsVP9Profile2 av1: _supportsAv1];
}

@end
