/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCodecInfo+Private.h"

#import "helpers/NSString+StdString.h"

@implementation RTCVideoCodecInfo (Private)

- (instancetype)initWithNativeSdpVideoFormat:(webrtc::SdpVideoFormat)format {
  NSMutableDictionary *params = [NSMutableDictionary dictionary];
  for (auto it = format.parameters.begin(); it != format.parameters.end(); ++it) {
    [params setObject:[NSString rtcStringForStdString:it->second]
               forKey:[NSString rtcStringForStdString:it->first]];
  }
  return [self initWithName:[NSString rtcStringForStdString:format.name] parameters:params];
}

- (webrtc::SdpVideoFormat)nativeSdpVideoFormat {
  std::map<std::string, std::string> parameters;
  for (NSString *paramKey in self.parameters.allKeys) {
    std::string key = [NSString rtcStdStringForString:paramKey];
    std::string value = [NSString rtcStdStringForString:self.parameters[paramKey]];
    parameters[key] = value;
  }

  return webrtc::SdpVideoFormat([NSString rtcStdStringForString:self.name], parameters);
}

@end
