/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoCodecInfo+HumanReadable.h"

@implementation RTCVideoCodecInfo (HumanReadable)

- (NSString *)humanReadableDescription {
  if ([self.name isEqualToString:@"H264"]) {
    NSString *profileId = self.parameters[@"profile-level-id"];
    if ([profileId isEqualToString:@"640c1f"]) {
      return @"H264 (High)";
    } else if ([profileId isEqualToString:@"42e01f"]) {
      return @"H264 (Baseline)";
    } else {
      return [NSString stringWithFormat:@"H264 (%@)", profileId];
    }
  } else {
    return self.name;
  }
}

@end
