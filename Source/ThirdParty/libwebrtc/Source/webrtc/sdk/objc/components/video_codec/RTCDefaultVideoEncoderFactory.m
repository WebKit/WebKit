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

@implementation RTCDefaultVideoEncoderFactory {
  bool _supportsH265;
  bool _supportsVP9;
}

- (id)initWithH265:(bool)supportsH265 vp9:(bool)supportsVP9
{
  self = [super init];
  if (self) {
      _supportsH265 = supportsH265;
      _supportsVP9 = supportsVP9;
  }
  return self;
}

+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
    return [self supportedCodecsWithH265:true vp9:true];
}

+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecsWithH265:(bool)supportsH265 vp9:(bool)supportsVP9 {
  NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedHighInfo =
      [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name
                                   parameters:constrainedHighParams];

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedBaselineInfo =
      [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name
                                   parameters:constrainedBaselineParams];

  NSMutableArray<RTCVideoCodecInfo *> *codecs = [[NSMutableArray alloc] initWithCapacity:5];

  [codecs addObject:constrainedHighInfo];
  [codecs addObject:constrainedBaselineInfo];
#if !defined(RTC_DISABLE_H265)
  if (supportsH265) {
    RTCVideoCodecInfo *h265Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH265Name];
    [codecs addObject:h265Info];
  }
#endif

  RTCVideoCodecInfo *vp8Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp8Name];
  [codecs addObject:vp8Info];

#if defined(RTC_ENABLE_VP9)
  if (supportsVP9) {
    RTCVideoCodecInfo *vp9Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name];
    [codecs addObject:vp9Info];
  }
#endif

  return codecs;
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  if ([info.name isEqualToString:kRTCVideoCodecH264Name]) {
    return [[RTCVideoEncoderH264 alloc] initWithCodecInfo:info];
  } else if ([info.name isEqualToString:kRTCVideoCodecVp8Name]) {
    return [RTCVideoEncoderVP8 vp8Encoder];
#if defined(RTC_ENABLE_VP9)
  } else if ([info.name isEqualToString:kRTCVideoCodecVp9Name]) {
    return [RTCVideoEncoderVP9 vp9Encoder];
#endif
#if !defined(DISABLE_H265)
  } else if (@available(iOS 11, *)) {
    if ([info.name isEqualToString:kRTCVideoCodecH265Name]) {
      return [[RTCVideoEncoderH265 alloc] initWithCodecInfo:info];
    }
#endif
  }

  return nil;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return [[self class] supportedCodecsWithH265:_supportsH265 vp9: _supportsVP9];
}

@end
