/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpParameters+Private.h"

#import "RTCRtcpParameters+Private.h"
#import "RTCRtpCodecParameters+Private.h"
#import "RTCRtpEncodingParameters+Private.h"
#import "RTCRtpHeaderExtension+Private.h"
#import "helpers/NSString+StdString.h"

@implementation RTCRtpParameters

@synthesize transactionId = _transactionId;
@synthesize rtcp = _rtcp;
@synthesize headerExtensions = _headerExtensions;
@synthesize encodings = _encodings;
@synthesize codecs = _codecs;
@synthesize degradationPreference = _degradationPreference;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpParameters &)nativeParameters {
  if (self = [self init]) {
    _transactionId = [NSString stringForStdString:nativeParameters.transaction_id];
    _rtcp = [[RTCRtcpParameters alloc] initWithNativeParameters:nativeParameters.rtcp];

    NSMutableArray *headerExtensions = [[NSMutableArray alloc] init];
    for (const auto &headerExtension : nativeParameters.header_extensions) {
      [headerExtensions
          addObject:[[RTCRtpHeaderExtension alloc] initWithNativeParameters:headerExtension]];
    }
    _headerExtensions = headerExtensions;

    NSMutableArray *encodings = [[NSMutableArray alloc] init];
    for (const auto &encoding : nativeParameters.encodings) {
      [encodings addObject:[[RTCRtpEncodingParameters alloc]
                               initWithNativeParameters:encoding]];
    }
    _encodings = encodings;

    NSMutableArray *codecs = [[NSMutableArray alloc] init];
    for (const auto &codec : nativeParameters.codecs) {
      [codecs addObject:[[RTCRtpCodecParameters alloc]
                            initWithNativeParameters:codec]];
    }
    _codecs = codecs;

    _degradationPreference = [RTCRtpParameters
        degradationPreferenceFromNativeDegradationPreference:nativeParameters
                                                                 .degradation_preference];
  }
  return self;
}

- (webrtc::RtpParameters)nativeParameters {
  webrtc::RtpParameters parameters;
  parameters.transaction_id = [NSString stdStringForString:_transactionId];
  parameters.rtcp = [_rtcp nativeParameters];
  for (RTCRtpHeaderExtension *headerExtension in _headerExtensions) {
    parameters.header_extensions.push_back(headerExtension.nativeParameters);
  }
  for (RTCRtpEncodingParameters *encoding in _encodings) {
    parameters.encodings.push_back(encoding.nativeParameters);
  }
  for (RTCRtpCodecParameters *codec in _codecs) {
    parameters.codecs.push_back(codec.nativeParameters);
  }
  if (_degradationPreference) {
    parameters.degradation_preference = [RTCRtpParameters
        nativeDegradationPreferenceFromDegradationPreference:(RTCDegradationPreference)
                                                                 _degradationPreference.intValue];
  }
  return parameters;
}

+ (webrtc::DegradationPreference)nativeDegradationPreferenceFromDegradationPreference:
    (RTCDegradationPreference)degradationPreference {
  switch (degradationPreference) {
    case RTCDegradationPreferenceDisabled:
      return webrtc::DegradationPreference::DISABLED;
    case RTCDegradationPreferenceMaintainFramerate:
      return webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    case RTCDegradationPreferenceMaintainResolution:
      return webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
    case RTCDegradationPreferenceBalanced:
      return webrtc::DegradationPreference::BALANCED;
  }
}

+ (NSNumber *)degradationPreferenceFromNativeDegradationPreference:
    (absl::optional<webrtc::DegradationPreference>)nativeDegradationPreference {
  if (!nativeDegradationPreference.has_value()) {
    return nil;
  }

  switch (*nativeDegradationPreference) {
    case webrtc::DegradationPreference::DISABLED:
      return @(RTCDegradationPreferenceDisabled);
    case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
      return @(RTCDegradationPreferenceMaintainFramerate);
    case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
      return @(RTCDegradationPreferenceMaintainResolution);
    case webrtc::DegradationPreference::BALANCED:
      return @(RTCDegradationPreferenceBalanced);
  }
}

@end
