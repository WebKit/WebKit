/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpCapabilities+Private.h"

#import "RTCRtpCodecCapability+Private.h"
#import "RTCRtpHeaderExtensionCapability+Private.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCRtpCapabilities)

@synthesize codecs = _codecs;
@synthesize headerExtensions = _headerExtensions;

- (instancetype)init {
  webrtc::RtpCapabilities nativeRtpCapabilities;
  return [self initWithNativeRtpCapabilities:nativeRtpCapabilities];
}

- (instancetype)initWithNativeRtpCapabilities:
    (const webrtc::RtpCapabilities &)nativeRtpCapabilities {
  if (self = [super init]) {
    NSMutableArray *codecs = [[NSMutableArray alloc] init];
    for (const auto &codec : nativeRtpCapabilities.codecs) {
      [codecs addObject:[[RTC_OBJC_TYPE(RTCRtpCodecCapability) alloc]
                            initWithNativeRtpCodecCapability:codec]];
    }
    _codecs = codecs;

    NSMutableArray *headerExtensions = [[NSMutableArray alloc] init];
    for (const auto &headerExtension : nativeRtpCapabilities.header_extensions) {
      [headerExtensions addObject:[[RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) alloc]
                                      initWithNativeRtpHeaderExtensionCapability:headerExtension]];
    }
    _headerExtensions = headerExtensions;
  }
  return self;
}

- (webrtc::RtpCapabilities)nativeRtpCapabilities {
  webrtc::RtpCapabilities rtpCapabilities;
  for (RTC_OBJC_TYPE(RTCRtpCodecCapability) * codec in _codecs) {
    rtpCapabilities.codecs.push_back(codec.nativeRtpCodecCapability);
  }
  for (RTC_OBJC_TYPE(RTCRtpHeaderExtensionCapability) * headerExtension in _headerExtensions) {
    rtpCapabilities.header_extensions.push_back(headerExtension.nativeRtpHeaderExtensionCapability);
  }
  return rtpCapabilities;
}

@end
