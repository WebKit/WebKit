/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCodecCapability+Private.h"

#import "RTCMediaStreamTrack.h"
#import "helpers/NSString+StdString.h"

#include "media/base/media_constants.h"
#include "rtc_base/checks.h"

@implementation RTC_OBJC_TYPE (RTCRtpCodecCapability)

@synthesize preferredPayloadType = _preferredPayloadType;
@synthesize name = _name;
@synthesize kind = _kind;
@synthesize clockRate = _clockRate;
@synthesize numChannels = _numChannels;
@synthesize parameters = _parameters;
@synthesize mimeType = _mimeType;

- (instancetype)init {
  webrtc::RtpCodecCapability rtpCodecCapability;
  return [self initWithNativeRtpCodecCapability:rtpCodecCapability];
}

- (instancetype)initWithNativeRtpCodecCapability:
    (const webrtc::RtpCodecCapability &)nativeRtpCodecCapability {
  if (self = [super init]) {
    if (nativeRtpCodecCapability.preferred_payload_type) {
      _preferredPayloadType =
          [NSNumber numberWithInt:*nativeRtpCodecCapability.preferred_payload_type];
    }
    _name = [NSString stringForStdString:nativeRtpCodecCapability.name];
    switch (nativeRtpCodecCapability.kind) {
      case cricket::MEDIA_TYPE_AUDIO:
        _kind = kRTCMediaStreamTrackKindAudio;
        break;
      case cricket::MEDIA_TYPE_VIDEO:
        _kind = kRTCMediaStreamTrackKindVideo;
        break;
      case cricket::MEDIA_TYPE_DATA:
        RTC_DCHECK_NOTREACHED();
        break;
      case cricket::MEDIA_TYPE_UNSUPPORTED:
        RTC_DCHECK_NOTREACHED();
        break;
    }
    if (nativeRtpCodecCapability.clock_rate) {
      _clockRate = [NSNumber numberWithInt:*nativeRtpCodecCapability.clock_rate];
    }
    if (nativeRtpCodecCapability.num_channels) {
      _numChannels = [NSNumber numberWithInt:*nativeRtpCodecCapability.num_channels];
    }
    NSMutableDictionary *parameters = [NSMutableDictionary dictionary];
    for (const auto &parameter : nativeRtpCodecCapability.parameters) {
      [parameters setObject:[NSString stringForStdString:parameter.second]
                     forKey:[NSString stringForStdString:parameter.first]];
    }
    _parameters = parameters;
    _mimeType = [NSString stringForStdString:nativeRtpCodecCapability.mime_type()];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTC_OBJC_TYPE(RTCRtpCodecCapability) {\n  "
                                    @"preferredPayloadType: %@\n  name: %@\n  kind: %@\n  "
                                    @"clockRate: %@\n  numChannels: %@\n  parameters: %@\n  "
                                    @"mimeType: %@\n}",
                                    _preferredPayloadType,
                                    _name,
                                    _kind,
                                    _clockRate,
                                    _numChannels,
                                    _parameters,
                                    _mimeType];
}

- (webrtc::RtpCodecCapability)nativeRtpCodecCapability {
  webrtc::RtpCodecCapability rtpCodecCapability;
  if (_preferredPayloadType != nil) {
    rtpCodecCapability.preferred_payload_type = absl::optional<int>(_preferredPayloadType.intValue);
  }
  rtpCodecCapability.name = [NSString stdStringForString:_name];
  // NSString pointer comparison is safe here since "kind" is readonly and only
  // populated above.
  if (_kind == kRTCMediaStreamTrackKindAudio) {
    rtpCodecCapability.kind = cricket::MEDIA_TYPE_AUDIO;
  } else if (_kind == kRTCMediaStreamTrackKindVideo) {
    rtpCodecCapability.kind = cricket::MEDIA_TYPE_VIDEO;
  } else {
    RTC_DCHECK_NOTREACHED();
  }
  if (_clockRate != nil) {
    rtpCodecCapability.clock_rate = absl::optional<int>(_clockRate.intValue);
  }
  if (_numChannels != nil) {
    rtpCodecCapability.num_channels = absl::optional<int>(_numChannels.intValue);
  }
  for (NSString *paramKey in _parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:_parameters[paramKey]];
    rtpCodecCapability.parameters[key] = value;
  }
  return rtpCodecCapability;
}

@end
