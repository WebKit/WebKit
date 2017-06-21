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
#import "WebRTC/RTCMediaStreamTrack.h"  // For "kind" strings.

#include "webrtc/base/checks.h"
#include "webrtc/media/base/mediaconstants.h"

const NSString * const kRTCRtxCodecName = @(cricket::kRtxCodecName);
const NSString * const kRTCRedCodecName = @(cricket::kRedCodecName);
const NSString * const kRTCUlpfecCodecName = @(cricket::kUlpfecCodecName);
const NSString * const kRTCFlexfecCodecName = @(cricket::kFlexfecCodecName);
const NSString * const kRTCOpusCodecName = @(cricket::kOpusCodecName);
const NSString * const kRTCIsacCodecName = @(cricket::kIsacCodecName);
const NSString * const kRTCL16CodecName  = @(cricket::kL16CodecName);
const NSString * const kRTCG722CodecName = @(cricket::kG722CodecName);
const NSString * const kRTCIlbcCodecName = @(cricket::kIlbcCodecName);
const NSString * const kRTCPcmuCodecName = @(cricket::kPcmuCodecName);
const NSString * const kRTCPcmaCodecName = @(cricket::kPcmaCodecName);
const NSString * const kRTCDtmfCodecName = @(cricket::kDtmfCodecName);
const NSString * const kRTCComfortNoiseCodecName =
    @(cricket::kComfortNoiseCodecName);
const NSString * const kRTCVp8CodecName = @(cricket::kVp8CodecName);
const NSString * const kRTCVp9CodecName = @(cricket::kVp9CodecName);
const NSString * const kRTCH264CodecName = @(cricket::kH264CodecName);

@implementation RTCRtpCodecParameters

@synthesize payloadType = _payloadType;
@synthesize name = _name;
@synthesize kind = _kind;
@synthesize clockRate = _clockRate;
@synthesize numChannels = _numChannels;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpCodecParameters &)nativeParameters {
  if (self = [self init]) {
    _payloadType = nativeParameters.payload_type;
    _name = [NSString stringForStdString:nativeParameters.name];
    switch (nativeParameters.kind) {
      case cricket::MEDIA_TYPE_AUDIO:
        _kind = kRTCMediaStreamTrackKindAudio;
        break;
      case cricket::MEDIA_TYPE_VIDEO:
        _kind = kRTCMediaStreamTrackKindVideo;
        break;
      case cricket::MEDIA_TYPE_DATA:
        RTC_NOTREACHED();
        break;
    }
    if (nativeParameters.clock_rate) {
      _clockRate = [NSNumber numberWithInt:*nativeParameters.clock_rate];
    }
    if (nativeParameters.num_channels) {
      _numChannels = [NSNumber numberWithInt:*nativeParameters.num_channels];
    }
  }
  return self;
}

- (webrtc::RtpCodecParameters)nativeParameters {
  webrtc::RtpCodecParameters parameters;
  parameters.payload_type = _payloadType;
  parameters.name = [NSString stdStringForString:_name];
  // NSString pointer comparison is safe here since "kind" is readonly and only
  // populated above.
  if (_kind == kRTCMediaStreamTrackKindAudio) {
    parameters.kind = cricket::MEDIA_TYPE_AUDIO;
  } else if (_kind == kRTCMediaStreamTrackKindVideo) {
    parameters.kind = cricket::MEDIA_TYPE_VIDEO;
  } else {
    RTC_NOTREACHED();
  }
  if (_clockRate != nil) {
    parameters.clock_rate = rtc::Optional<int>(_clockRate.intValue);
  }
  if (_numChannels != nil) {
    parameters.num_channels = rtc::Optional<int>(_numChannels.intValue);
  }
  return parameters;
}

@end
