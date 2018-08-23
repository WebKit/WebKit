/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "WebRTC/RTCVideoCodecH264.h"

#include "media/base/h264_profile_level_id.h"

@interface RTCH264ProfileLevelId ()

@property(nonatomic, assign) RTCH264Profile profile;
@property(nonatomic, assign) RTCH264Level level;
@property(nonatomic, strong) NSString *hexString;

@end

@implementation RTCH264ProfileLevelId

@synthesize profile = _profile;
@synthesize level = _level;
@synthesize hexString = _hexString;

- (instancetype)initWithHexString:(NSString *)hexString {
  if (self = [super init]) {
    self.hexString = hexString;

    absl::optional<webrtc::H264::ProfileLevelId> profile_level_id =
        webrtc::H264::ParseProfileLevelId([hexString cStringUsingEncoding:NSUTF8StringEncoding]);
    if (profile_level_id.has_value()) {
      self.profile = static_cast<RTCH264Profile>(profile_level_id->profile);
      self.level = static_cast<RTCH264Level>(profile_level_id->level);
    }
  }
  return self;
}

- (instancetype)initWithProfile:(RTCH264Profile)profile level:(RTCH264Level)level {
  if (self = [super init]) {
    self.profile = profile;
    self.level = level;

    absl::optional<std::string> hex_string =
        webrtc::H264::ProfileLevelIdToString(webrtc::H264::ProfileLevelId(
            static_cast<webrtc::H264::Profile>(profile), static_cast<webrtc::H264::Level>(level)));
    self.hexString =
        [NSString stringWithCString:hex_string.value_or("").c_str() encoding:NSUTF8StringEncoding];
  }
  return self;
}

@end
