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

#import "helpers/NSString+StdString.h"

@implementation RTCRtpEncodingParameters

@synthesize rid = _rid;
@synthesize isActive = _isActive;
@synthesize maxBitrateBps = _maxBitrateBps;
@synthesize minBitrateBps = _minBitrateBps;
@synthesize maxFramerate = _maxFramerate;
@synthesize numTemporalLayers = _numTemporalLayers;
@synthesize scaleResolutionDownBy = _scaleResolutionDownBy;
@synthesize ssrc = _ssrc;
@synthesize networkPriority = _networkPriority;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithNativeParameters:
    (const webrtc::RtpEncodingParameters &)nativeParameters {
  if (self = [self init]) {
    if (!nativeParameters.rid.empty()) {
      _rid = [NSString stringForStdString:nativeParameters.rid];
    }
    _isActive = nativeParameters.active;
    if (nativeParameters.max_bitrate_bps) {
      _maxBitrateBps =
          [NSNumber numberWithInt:*nativeParameters.max_bitrate_bps];
    }
    if (nativeParameters.min_bitrate_bps) {
      _minBitrateBps =
          [NSNumber numberWithInt:*nativeParameters.min_bitrate_bps];
    }
    if (nativeParameters.max_framerate) {
      _maxFramerate = [NSNumber numberWithInt:*nativeParameters.max_framerate];
    }
    if (nativeParameters.num_temporal_layers) {
      _numTemporalLayers = [NSNumber numberWithInt:*nativeParameters.num_temporal_layers];
    }
    if (nativeParameters.scale_resolution_down_by) {
      _scaleResolutionDownBy =
          [NSNumber numberWithDouble:*nativeParameters.scale_resolution_down_by];
    }
    if (nativeParameters.ssrc) {
      _ssrc = [NSNumber numberWithUnsignedLong:*nativeParameters.ssrc];
    }
    _networkPriority = nativeParameters.network_priority;
  }
  return self;
}

- (webrtc::RtpEncodingParameters)nativeParameters {
  webrtc::RtpEncodingParameters parameters;
  if (_rid != nil) {
    parameters.rid = [NSString stdStringForString:_rid];
  }
  parameters.active = _isActive;
  if (_maxBitrateBps != nil) {
    parameters.max_bitrate_bps = absl::optional<int>(_maxBitrateBps.intValue);
  }
  if (_minBitrateBps != nil) {
    parameters.min_bitrate_bps = absl::optional<int>(_minBitrateBps.intValue);
  }
  if (_maxFramerate != nil) {
    parameters.max_framerate = absl::optional<int>(_maxFramerate.intValue);
  }
  if (_numTemporalLayers != nil) {
    parameters.num_temporal_layers = absl::optional<int>(_numTemporalLayers.intValue);
  }
  if (_scaleResolutionDownBy != nil) {
    parameters.scale_resolution_down_by =
        absl::optional<double>(_scaleResolutionDownBy.doubleValue);
  }
  if (_ssrc != nil) {
    parameters.ssrc = absl::optional<uint32_t>(_ssrc.unsignedLongValue);
  }
  parameters.network_priority = _networkPriority;
  return parameters;
}

@end
