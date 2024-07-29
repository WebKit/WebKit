/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpSource.h"
#import "RTCRtpSource+Private.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include <optional>
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/transport/rtp/rtp_source.h"

@implementation RTC_OBJC_TYPE (RTCRtpSource) {
  std::optional<webrtc::RtpSource> _nativeRtpSource;
}

- (uint32_t)sourceId {
  return _nativeRtpSource.value().source_id();
}

- (CFTimeInterval)timestampUs {
  return _nativeRtpSource.value().timestamp().us();
}

- (uint32_t)rtpTimestamp {
  return _nativeRtpSource.value().rtp_timestamp();
}

- (RTCRtpSourceType)sourceType {
  return [RTC_OBJC_TYPE(RTCRtpSource)
      rtpSourceTypeForNativeRtpSourceType:_nativeRtpSource.value().source_type()];
}

- (NSNumber *)audioLevel {
  absl::optional<uint8_t> level = _nativeRtpSource.value().audio_level();
  if (!level.has_value()) {
    return nil;
  }
  // Converted according to equation defined here:
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource-audiolevel
  uint8_t rfcLevel = level.value();
  if (rfcLevel > 127u) {
    rfcLevel = 127u;
  }
  if (rfcLevel == 127u) {
    return @(0.0);
  }
  return @(std::pow(10.0, -(double)rfcLevel / 20.0));
}

- (NSString *)description {
  return [NSString
      stringWithFormat:@"RTC_OBJC_TYPE(RTCRtpSource) {\n  sourceId: %d, sourceType: %@\n}",
                       self.sourceId,
                       [RTC_OBJC_TYPE(RTCRtpSource) stringForRtpSourceType:self.sourceType]];
}

- (instancetype)initWithNativeRtpSource:(const webrtc::RtpSource &)nativeRtpSource {
  if (self = [super init]) {
    _nativeRtpSource = nativeRtpSource;
  }
  return self;
}

+ (RTCRtpSourceType)rtpSourceTypeForNativeRtpSourceType:(webrtc::RtpSourceType)nativeRtpSourceType {
  switch (nativeRtpSourceType) {
    case webrtc::RtpSourceType::SSRC:
      return RTCRtpSourceTypeSSRC;
    case webrtc::RtpSourceType::CSRC:
      return RTCRtpSourceTypeCSRC;
  }
}

+ (NSString *)stringForRtpSourceType:(RTCRtpSourceType)mediaType {
  switch (mediaType) {
    case RTCRtpSourceTypeSSRC:
      return @"SSRC";
    case RTCRtpSourceTypeCSRC:
      return @"CSRC";
  }
}

@end
