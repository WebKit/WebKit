/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodecH264.h"

#include <vector>

#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodec.h"

#include "rtc_base/timeutils.h"

// H264 specific settings.
@implementation RTCCodecSpecificInfoH264 { }

@synthesize packetizationMode = _packetizationMode;
@synthesize simulcastIndex = _simulcastIndex;

- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo {
  webrtc::CodecSpecificInfo codecSpecificInfo;
  codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  codecSpecificInfo.codec_name = [kRTCVideoCodecH264Name cStringUsingEncoding:NSUTF8StringEncoding];
  codecSpecificInfo.codecSpecific.H264.packetization_mode =
      (webrtc::H264PacketizationMode)_packetizationMode;
  codecSpecificInfo.codecSpecific.H264.simulcast_idx = _simulcastIndex;

  return codecSpecificInfo;
}

@end

// Encoder factory.
@implementation RTCVideoEncoderFactoryH264

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSMutableArray<RTCVideoCodecInfo *> *codecs = [NSMutableArray array];
  NSString *codecName = kRTCVideoCodecH264Name;

  NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedHighInfo =
      [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:constrainedHighParams];
  [codecs addObject:constrainedHighInfo];

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kRTCMaxSupportedH264ProfileLevelConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedBaselineInfo =
      [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:constrainedBaselineParams];
  [codecs addObject:constrainedBaselineInfo];

  return [codecs copy];
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  return [[RTCVideoEncoderH264 alloc] initWithCodecInfo:info];
}

@end

// Decoder factory.
@implementation RTCVideoDecoderFactoryH264

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  return [[RTCVideoDecoderH264 alloc] init];
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSString *codecName = kRTCVideoCodecH264Name;
  return @[ [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:nil] ];
}

@end
