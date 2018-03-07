/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#import "NSString+StdString.h"
#import "RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodecFactory.h"

#include "media/base/mediaconstants.h"

NSString *const kRTCVideoCodecVp8Name = @(cricket::kVp8CodecName);
NSString *const kRTCVideoCodecVp9Name = @(cricket::kVp9CodecName);
NSString *const kRTCVideoCodecH264Name = @(cricket::kH264CodecName);
NSString *const kRTCLevel31ConstrainedHigh = @"640c1f";
NSString *const kRTCLevel31ConstrainedBaseline = @"42e01f";

@implementation RTCVideoCodecInfo

@synthesize name = _name;
@synthesize parameters = _parameters;

- (instancetype)initWithName:(NSString *)name {
  return [self initWithName:name parameters:nil];
}

- (instancetype)initWithName:(NSString *)name
                  parameters:(nullable NSDictionary<NSString *, NSString *> *)parameters {
  if (self = [super init]) {
    _name = name;
    _parameters = (parameters ? parameters : @{});
  }

  return self;
}

- (instancetype)initWithNativeSdpVideoFormat:(webrtc::SdpVideoFormat)format {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = format.parameters.begin(); it != format.parameters.end(); ++it) {
    [params setObject:[NSString stringForStdString:it->second]
               forKey:[NSString stringForStdString:it->first]];
  }
  return [self initWithName:[NSString stringForStdString:format.name] parameters:params];
}

- (instancetype)initWithNativeVideoCodec:(cricket::VideoCodec)videoCodec {
  return [self
      initWithNativeSdpVideoFormat:webrtc::SdpVideoFormat(videoCodec.name, videoCodec.params)];
}

- (BOOL)isEqualToCodecInfo:(RTCVideoCodecInfo *)info {
  if (!info ||
      ![self.name isEqualToString:info.name] ||
      ![self.parameters isEqualToDictionary:info.parameters]) {
    return NO;
  }
  return YES;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;
  if (![object isKindOfClass:[self class]])
    return NO;
  return [self isEqualToCodecInfo:object];
}

- (NSUInteger)hash {
  return [self.name hash] ^ [self.parameters hash];
}

- (webrtc::SdpVideoFormat)nativeSdpVideoFormat {
  std::map<std::string, std::string> parameters;
  for (NSString *paramKey in _parameters.allKeys) {
    std::string key = [NSString stdStringForString:paramKey];
    std::string value = [NSString stdStringForString:_parameters[paramKey]];
    parameters[key] = value;
  }

  return webrtc::SdpVideoFormat([NSString stdStringForString:_name], parameters);
}

- (cricket::VideoCodec)nativeVideoCodec {
  cricket::VideoCodec codec([NSString stdStringForString:_name]);
  for (NSString *paramKey in _parameters.allKeys) {
    codec.SetParam([NSString stdStringForString:paramKey],
                   [NSString stdStringForString:_parameters[paramKey]]);
  }

  return codec;
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder *)decoder {
  return [self initWithName:[decoder decodeObjectForKey:@"name"]
                 parameters:[decoder decodeObjectForKey:@"parameters"]];
}

- (void)encodeWithCoder:(NSCoder *)encoder {
  [encoder encodeObject:_name forKey:@"name"];
  [encoder encodeObject:_parameters forKey:@"parameters"];
}

@end

@implementation RTCVideoEncoderQpThresholds

@synthesize low = _low;
@synthesize high = _high;

- (instancetype)initWithThresholdsLow:(NSInteger)low high:(NSInteger)high {
  if (self = [super init]) {
    _low = low;
    _high = high;
  }
  return self;
}

@end
