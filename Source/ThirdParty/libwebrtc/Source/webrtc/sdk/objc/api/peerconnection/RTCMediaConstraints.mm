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

NSString * const kRTCMediaConstraintsMinAspectRatio =
    @(webrtc::MediaConstraintsInterface::kMinAspectRatio);
NSString * const kRTCMediaConstraintsMaxAspectRatio =
    @(webrtc::MediaConstraintsInterface::kMaxAspectRatio);
NSString * const kRTCMediaConstraintsMinWidth =
    @(webrtc::MediaConstraintsInterface::kMinWidth);
NSString * const kRTCMediaConstraintsMaxWidth =
    @(webrtc::MediaConstraintsInterface::kMaxWidth);
NSString * const kRTCMediaConstraintsMinHeight =
    @(webrtc::MediaConstraintsInterface::kMinHeight);
NSString * const kRTCMediaConstraintsMaxHeight =
    @(webrtc::MediaConstraintsInterface::kMaxHeight);
NSString * const kRTCMediaConstraintsMinFrameRate =
    @(webrtc::MediaConstraintsInterface::kMinFrameRate);
NSString * const kRTCMediaConstraintsMaxFrameRate =
    @(webrtc::MediaConstraintsInterface::kMaxFrameRate);
NSString * const kRTCMediaConstraintsAudioNetworkAdaptorConfig =
    @(webrtc::MediaConstraintsInterface::kAudioNetworkAdaptorConfig);

NSString * const kRTCMediaConstraintsIceRestart =
    @(webrtc::MediaConstraintsInterface::kIceRestart);
NSString * const kRTCMediaConstraintsOfferToReceiveAudio =
    @(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio);
NSString * const kRTCMediaConstraintsOfferToReceiveVideo =
    @(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo);
NSString * const kRTCMediaConstraintsVoiceActivityDetection =
    @(webrtc::MediaConstraintsInterface::kVoiceActivityDetection);

NSString * const kRTCMediaConstraintsValueTrue =
    @(webrtc::MediaConstraintsInterface::kValueTrue);
NSString * const kRTCMediaConstraintsValueFalse =
    @(webrtc::MediaConstraintsInterface::kValueFalse);

namespace webrtc {

MediaConstraints::~MediaConstraints() {}

MediaConstraints::MediaConstraints() {}

MediaConstraints::MediaConstraints(
    const MediaConstraintsInterface::Constraints& mandatory,
    const MediaConstraintsInterface::Constraints& optional)
    : mandatory_(mandatory), optional_(optional) {}

const MediaConstraintsInterface::Constraints&
MediaConstraints::GetMandatory() const {
  return mandatory_;
}

const MediaConstraintsInterface::Constraints&
MediaConstraints::GetOptional() const {
  return optional_;
}

}  // namespace webrtc


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
  webrtc::MediaConstraintsInterface::Constraints mandatory =
      [[self class] nativeConstraintsForConstraints:_mandatory];
  webrtc::MediaConstraintsInterface::Constraints optional =
      [[self class] nativeConstraintsForConstraints:_optional];

  webrtc::MediaConstraints *nativeConstraints =
      new webrtc::MediaConstraints(mandatory, optional);
  return std::unique_ptr<webrtc::MediaConstraints>(nativeConstraints);
}

+ (webrtc::MediaConstraintsInterface::Constraints)
    nativeConstraintsForConstraints:
        (NSDictionary<NSString *, NSString *> *)constraints {
  webrtc::MediaConstraintsInterface::Constraints nativeConstraints;
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
      nativeConstraints.push_back(webrtc::MediaConstraintsInterface::Constraint(
          key.stdString, configValue));
    } else {
      nativeConstraints.push_back(webrtc::MediaConstraintsInterface::Constraint(
          key.stdString, value.stdString));
    }
  }
  return nativeConstraints;
}

@end
