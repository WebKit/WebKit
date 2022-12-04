/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDefaultVideoDecoderFactory.h"

#import "RTCH264ProfileLevelId.h"
#import "RTCVideoDecoderH264.h"
#import "api/video_codec/RTCVideoCodecConstants.h"
#import "api/video_codec/RTCVideoDecoderVP8.h"
#import "base/RTCVideoCodecInfo.h"
#if defined(RTC_ENABLE_VP9)
#import "api/video_codec/RTCVideoDecoderVP9.h"
#import "RTCVideoDecoderVTBVP9.h"
#endif
#if !defined(RTC_DISABLE_H265)
#import "RTCH265ProfileLevelId.h"
#import "RTCVideoDecoderH265.h"
#endif
#if !defined(DISABLE_RTC_AV1)
#import "sdk/objc/api/video_codec/RTCVideoDecoderAV1.h"
#endif

#import <VideoToolbox/VideoToolbox.h>

@implementation RTCDefaultVideoDecoderFactory {
  bool _supportsH265;
  bool _supportsVP9Profile0;
  bool _supportsVP9Profile2;
  bool _supportsVP9VTB;
  bool _supportsAv1;
}

- (id)initWithH265:(bool)supportsH265 vp9Profile0:(bool)supportsVP9Profile0 vp9Profile2:(bool)supportsVP9Profile2 vp9VTB:(bool)supportsVP9VTB av1:(bool)supportsAv1
{
  self = [super init];
  if (self) {
      _supportsH265 = supportsH265;
      _supportsVP9Profile0 = supportsVP9Profile0;
      _supportsVP9Profile2 = supportsVP9Profile2;
      // Use kCMVideoCodecType_VP9 once added to CMFormatDescription.h
      _supportsVP9VTB = (supportsVP9Profile0 || supportsVP9Profile2) && supportsVP9VTB;
      _supportsAv1 = supportsAv1;
  }
  return self;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSMutableArray<RTCVideoCodecInfo *> *codecs = [[NSMutableArray alloc] initWithCapacity:5];

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
  if (_supportsH265) {
    RTCVideoCodecInfo *h265Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH265Name];
    [codecs addObject:h265Info];
  }
#endif

  RTCVideoCodecInfo *vp8Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp8Name];
  [codecs addObject:vp8Info];

#if defined(RTC_ENABLE_VP9)
  if (_supportsVP9Profile0) {
    [codecs addObject:[[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name parameters: @{
      @"profile-id" : @"0",
    }]];
  }
  if (_supportsVP9Profile2) {
    [codecs addObject:[[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name parameters: @{
      @"profile-id" : @"2",
    }]];
  }
#endif
#if !defined(DISABLE_RTC_AV1)
  if (_supportsAv1) {
    [codecs addObject:[[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecAv1Name]];
  }
#endif
  return codecs;
}

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  if ([info.name isEqualToString:kRTCVideoCodecH264Name]) {
    return [[RTCVideoDecoderH264 alloc] init];
  } else if ([info.name isEqualToString:kRTCVideoCodecVp8Name]) {
    return [RTCVideoDecoderVP8 vp8Decoder];
#if !defined(RTC_DISABLE_H265)
  } else if ([info.name isEqualToString:kRTCVideoCodecH265Name]) {
    return [[RTCVideoDecoderH265 alloc] init];
#endif
#if defined(RTC_ENABLE_VP9)
  } else if ([info.name isEqualToString:kRTCVideoCodecVp9Name]) {
      if (_supportsVP9VTB) {
        return [[RTCVideoDecoderVTBVP9 alloc] init];
      } else {
        return [RTCVideoDecoderVP9 vp9Decoder];
      }
#endif
#if !defined(DISABLE_RTC_AV1)
  } else if ([info.name isEqualToString:kRTCVideoCodecAv1Name]) {
    return [RTCVideoDecoderAV1 av1Decoder];
  }
#endif

  return nil;
}

@end
