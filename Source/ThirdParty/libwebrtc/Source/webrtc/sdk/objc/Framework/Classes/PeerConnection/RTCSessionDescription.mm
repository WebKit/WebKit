/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCSessionDescription+Private.h"

#import "NSString+StdString.h"
#import "WebRTC/RTCLogging.h"

#include "rtc_base/checks.h"

@implementation RTCSessionDescription

@synthesize type = _type;
@synthesize sdp = _sdp;

+ (NSString *)stringForType:(RTCSdpType)type {
  std::string string = [[self class] stdStringForType:type];
  return [NSString stringForStdString:string];
}

+ (RTCSdpType)typeForString:(NSString *)string {
  std::string typeString = string.stdString;
  return [[self class] typeForStdString:typeString];
}

- (instancetype)initWithType:(RTCSdpType)type sdp:(NSString *)sdp {
  NSParameterAssert(sdp.length);
  if (self = [super init]) {
    _type = type;
    _sdp = [sdp copy];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCSessionDescription:\n%@\n%@",
                                    [[self class] stringForType:_type],
                                    _sdp];
}

#pragma mark - Private

- (webrtc::SessionDescriptionInterface *)nativeDescription {
  webrtc::SdpParseError error;

  webrtc::SessionDescriptionInterface *description =
      webrtc::CreateSessionDescription([[self class] stdStringForType:_type],
                                       _sdp.stdString,
                                       &error);

  if (!description) {
    RTCLogError(@"Failed to create session description: %s\nline: %s",
                error.description.c_str(),
                error.line.c_str());
  }

  return description;
}

- (instancetype)initWithNativeDescription:
    (const webrtc::SessionDescriptionInterface *)nativeDescription {
  NSParameterAssert(nativeDescription);
  std::string sdp;
  nativeDescription->ToString(&sdp);
  RTCSdpType type = [[self class] typeForStdString:nativeDescription->type()];

  return [self initWithType:type
                        sdp:[NSString stringForStdString:sdp]];
}

+ (std::string)stdStringForType:(RTCSdpType)type {
  switch (type) {
    case RTCSdpTypeOffer:
      return webrtc::SessionDescriptionInterface::kOffer;
    case RTCSdpTypePrAnswer:
      return webrtc::SessionDescriptionInterface::kPrAnswer;
    case RTCSdpTypeAnswer:
      return webrtc::SessionDescriptionInterface::kAnswer;
  }
}

+ (RTCSdpType)typeForStdString:(const std::string &)string {
  if (string == webrtc::SessionDescriptionInterface::kOffer) {
    return RTCSdpTypeOffer;
  } else if (string == webrtc::SessionDescriptionInterface::kPrAnswer) {
    return RTCSdpTypePrAnswer;
  } else if (string == webrtc::SessionDescriptionInterface::kAnswer) {
    return RTCSdpTypeAnswer;
  } else {
    RTC_NOTREACHED();
    return RTCSdpTypeOffer;
  }
}

@end
