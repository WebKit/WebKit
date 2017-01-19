/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDBitrateTracker.h"

#import <QuartzCore/QuartzCore.h>

@implementation ARDBitrateTracker {
  CFTimeInterval _prevTime;
  NSInteger _prevByteCount;
}

@synthesize bitrate = _bitrate;

+ (NSString *)bitrateStringForBitrate:(double)bitrate {
  if (bitrate > 1e6) {
    return [NSString stringWithFormat:@"%.2fMbps", bitrate * 1e-6];
  } else if (bitrate > 1e3) {
    return [NSString stringWithFormat:@"%.0fKbps", bitrate * 1e-3];
  } else {
    return [NSString stringWithFormat:@"%.0fbps", bitrate];
  }
}

- (NSString *)bitrateString {
  return [[self class] bitrateStringForBitrate:_bitrate];
}

- (void)updateBitrateWithCurrentByteCount:(NSInteger)byteCount {
  CFTimeInterval currentTime = CACurrentMediaTime();
  if (_prevTime && (byteCount > _prevByteCount)) {
    _bitrate = (byteCount - _prevByteCount) * 8 / (currentTime - _prevTime);
  }
  _prevByteCount = byteCount;
  _prevTime = currentTime;
}

@end
