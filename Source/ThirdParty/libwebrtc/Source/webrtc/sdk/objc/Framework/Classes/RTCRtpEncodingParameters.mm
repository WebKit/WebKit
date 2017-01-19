/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpEncodingParameters+Private.h"

@implementation RTCRtpEncodingParameters

@synthesize isActive = _isActive;
@synthesize maxBitrateBps = _maxBitrateBps;

static const int kBitrateUnlimited = -1;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpEncodingParameters &)nativeParameters {
  if (self = [self init]) {
    _isActive = nativeParameters.active;
    // TODO(skvlad): Replace with rtc::Optional once the C++ code is updated.
    if (nativeParameters.max_bitrate_bps != kBitrateUnlimited) {
      _maxBitrateBps =
          [NSNumber numberWithInt:nativeParameters.max_bitrate_bps];
    }
  }
  return self;
}

- (webrtc::RtpEncodingParameters)nativeParameters {
  webrtc::RtpEncodingParameters parameters;
  parameters.active = _isActive;
  if (_maxBitrateBps != nil) {
    parameters.max_bitrate_bps = _maxBitrateBps.intValue;
  }
  return parameters;
}

@end
