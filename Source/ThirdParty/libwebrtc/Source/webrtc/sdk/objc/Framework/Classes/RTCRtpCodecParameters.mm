/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCodecParameters+Private.h"

#import "NSString+StdString.h"

#include "webrtc/media/base/mediaconstants.h"

const NSString * const kRTCRtxCodecMimeType = @(cricket::kRtxCodecName);
const NSString * const kRTCRedCodecMimeType = @(cricket::kRedCodecName);
const NSString * const kRTCUlpfecCodecMimeType = @(cricket::kUlpfecCodecName);
const NSString * const kRTCFlexfecCodecMimeType = @(cricket::kFlexfecCodecName);
const NSString * const kRTCOpusCodecMimeType = @(cricket::kOpusCodecName);
const NSString * const kRTCIsacCodecMimeType = @(cricket::kIsacCodecName);
const NSString * const kRTCL16CodecMimeType  = @(cricket::kL16CodecName);
const NSString * const kRTCG722CodecMimeType = @(cricket::kG722CodecName);
const NSString * const kRTCIlbcCodecMimeType = @(cricket::kIlbcCodecName);
const NSString * const kRTCPcmuCodecMimeType = @(cricket::kPcmuCodecName);
const NSString * const kRTCPcmaCodecMimeType = @(cricket::kPcmaCodecName);
const NSString * const kRTCDtmfCodecMimeType = @(cricket::kDtmfCodecName);
const NSString * const kRTCComfortNoiseCodecMimeType =
    @(cricket::kComfortNoiseCodecName);
const NSString * const kVp8CodecMimeType = @(cricket::kVp8CodecName);
const NSString * const kVp9CodecMimeType = @(cricket::kVp9CodecName);
const NSString * const kH264CodecMimeType = @(cricket::kH264CodecName);

@implementation RTCRtpCodecParameters

@synthesize payloadType = _payloadType;
@synthesize mimeType = _mimeType;
@synthesize clockRate = _clockRate;
@synthesize channels = _channels;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpCodecParameters &)nativeParameters {
  if (self = [self init]) {
    _payloadType = nativeParameters.payload_type;
    _mimeType = [NSString stringForStdString:nativeParameters.mime_type];
    _clockRate = nativeParameters.clock_rate;
    _channels = nativeParameters.channels;
  }
  return self;
}

- (webrtc::RtpCodecParameters)nativeParameters {
  webrtc::RtpCodecParameters parameters;
  parameters.payload_type = _payloadType;
  parameters.mime_type = [NSString stdStringForString:_mimeType];
  parameters.clock_rate = _clockRate;
  parameters.channels = _channels;
  return parameters;
}

@end
