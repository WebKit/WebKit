/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIntervalRange+Private.h"

#include "rtc_base/checks.h"

@implementation RTCIntervalRange

@synthesize min = _min;
@synthesize max = _max;

- (instancetype)init {
  return [self initWithMin:0 max:0];
}

- (instancetype)initWithMin:(NSInteger)min
                        max:(NSInteger)max {
  RTC_DCHECK_LE(min, max);
  if (self = [super init]) {
    _min = min;
    _max = max;
  }
  return self;
}

- (instancetype)initWithNativeIntervalRange:(const rtc::IntervalRange &)config {
  return [self initWithMin:config.min() max:config.max()];
}

- (NSString *)description {
  return [NSString stringWithFormat:@"[%ld, %ld]", (long)_min, (long)_max];
}

#pragma mark - Private

- (std::unique_ptr<rtc::IntervalRange>)nativeIntervalRange {
  std::unique_ptr<rtc::IntervalRange> nativeIntervalRange(
      new rtc::IntervalRange((int)_min, (int)_max));
  return nativeIntervalRange;
}

@end
