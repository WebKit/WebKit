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
#include "sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"
#include "system_wrappers/include/field_trial.h"

const char kHighProfileExperiment[] = "WebRTC-H264HighProfile";

bool IsHighProfileEnabled() {
  return webrtc::field_trial::IsEnabled(kHighProfileExperiment);
}

// H264 specific settings.
@implementation RTCCodecSpecificInfoH264

@synthesize packetizationMode = _packetizationMode;

- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo {
  webrtc::CodecSpecificInfo codecSpecificInfo;
  codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  codecSpecificInfo.codec_name = [kRTCVideoCodecH264Name cStringUsingEncoding:NSUTF8StringEncoding];
  codecSpecificInfo.codecSpecific.H264.packetization_mode =
      (webrtc::H264PacketizationMode)_packetizationMode;

  return codecSpecificInfo;
}

@end

// Encoder factory.
@implementation RTCVideoEncoderFactoryH264

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSMutableArray<RTCVideoCodecInfo *> *codecs = [NSMutableArray array];
  NSString *codecName = kRTCVideoCodecH264Name;

  if (IsHighProfileEnabled()) {
    NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
      @"profile-level-id" : kRTCLevel31ConstrainedHigh,
      @"level-asymmetry-allowed" : @"1",
      @"packetization-mode" : @"1",
    };
    RTCVideoCodecInfo *constrainedHighInfo =
        [[RTCVideoCodecInfo alloc] initWithName:codecName parameters:constrainedHighParams];
    [codecs addObject:constrainedHighInfo];
  }

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kRTCLevel31ConstrainedBaseline,
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
