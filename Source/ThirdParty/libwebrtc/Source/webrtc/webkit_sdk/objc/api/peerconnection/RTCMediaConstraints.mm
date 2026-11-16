/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaConstraints+Private.h"

#import "helpers/NSString+StdString.h"

#include <memory>

NSString *const kRTCMediaConstraintsAudioNetworkAdaptorConfig =
    @(webrtc::MediaConstraints::kAudioNetworkAdaptorConfig);

NSString *const kRTCMediaConstraintsIceRestart = @(webrtc::MediaConstraints::kIceRestart);
NSString *const kRTCMediaConstraintsOfferToReceiveAudio =
    @(webrtc::MediaConstraints::kOfferToReceiveAudio);
NSString *const kRTCMediaConstraintsOfferToReceiveVideo =
    @(webrtc::MediaConstraints::kOfferToReceiveVideo);
NSString *const kRTCMediaConstraintsVoiceActivityDetection =
    @(webrtc::MediaConstraints::kVoiceActivityDetection);

NSString *const kRTCMediaConstraintsValueTrue = @(webrtc::MediaConstraints::kValueTrue);
NSString *const kRTCMediaConstraintsValueFalse = @(webrtc::MediaConstraints::kValueFalse);

@implementation RTCMediaConstraints {
  NSDictionary<NSString *, NSString *> *_mandatory;
  NSDictionary<NSString *, NSString *> *_optional;
}

- (instancetype)initWithMandatoryConstraints:
    (NSDictionary<NSString *, NSString *> *)mandatory
                         optionalConstraints:
    (NSDictionary<NSString *, NSString *> *)optional {
  if (self = [super init]) {
    _mandatory = [[NSDictionary alloc] initWithDictionary:mandatory
                                                copyItems:YES];
    _optional = [[NSDictionary alloc] initWithDictionary:optional
                                               copyItems:YES];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCMediaConstraints:\n%@\n%@",
                                    _mandatory,
                                    _optional];
}

#pragma mark - Private

- (std::unique_ptr<webrtc::MediaConstraints>)nativeConstraints {
  webrtc::MediaConstraints::Constraints mandatory =
      [[self class] nativeConstraintsForConstraints:_mandatory];
  webrtc::MediaConstraints::Constraints optional =
      [[self class] nativeConstraintsForConstraints:_optional];

  webrtc::MediaConstraints *nativeConstraints =
      new webrtc::MediaConstraints(mandatory, optional);
  return std::unique_ptr<webrtc::MediaConstraints>(nativeConstraints);
}

+ (webrtc::MediaConstraints::Constraints)nativeConstraintsForConstraints:
    (NSDictionary<NSString *, NSString *> *)constraints {
  webrtc::MediaConstraints::Constraints nativeConstraints;
  for (NSString *key in constraints) {
    NSAssert([key isKindOfClass:[NSString class]],
             @"%@ is not an NSString.", key);
    NSString *value = [constraints objectForKey:key];
    NSAssert([value isKindOfClass:[NSString class]],
             @"%@ is not an NSString.", value);
    if ([kRTCMediaConstraintsAudioNetworkAdaptorConfig isEqualToString:key]) {
      // This value is base64 encoded.
      NSData *charData = [[NSData alloc] initWithBase64EncodedString:value options:0];
      std::string configValue =
          std::string(reinterpret_cast<const char *>(charData.bytes), charData.length);
      nativeConstraints.push_back(webrtc::MediaConstraints::Constraint(key.stdString, configValue));
    } else {
      nativeConstraints.push_back(
          webrtc::MediaConstraints::Constraint(key.stdString, value.stdString));
    }
  }
  return nativeConstraints;
}

@end
