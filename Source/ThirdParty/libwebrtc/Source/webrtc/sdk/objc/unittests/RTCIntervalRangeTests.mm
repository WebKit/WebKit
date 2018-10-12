/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCIntervalRange+Private.h"
#import "api/peerconnection/RTCIntervalRange.h"

@interface RTCIntervalRangeTest : NSObject
- (void)testConversionToNativeConfiguration;
- (void)testNativeConversionToConfiguration;
@end

@implementation RTCIntervalRangeTest

- (void)testConversionToNativeConfiguration {
  NSInteger min = 0;
  NSInteger max = 100;
  RTCIntervalRange *range = [[RTCIntervalRange alloc] initWithMin:min max:max];
  EXPECT_EQ(min, range.min);
  EXPECT_EQ(max, range.max);
  std::unique_ptr<rtc::IntervalRange> nativeRange = range.nativeIntervalRange;
  EXPECT_EQ(min, nativeRange->min());
  EXPECT_EQ(max, nativeRange->max());
}

- (void)testNativeConversionToConfiguration {
  NSInteger min = 0;
  NSInteger max = 100;
  rtc::IntervalRange nativeRange((int)min, (int)max);
  RTCIntervalRange *range =
      [[RTCIntervalRange alloc] initWithNativeIntervalRange:nativeRange];
  EXPECT_EQ(min, range.min);
  EXPECT_EQ(max, range.max);
}

@end

TEST(RTCIntervalRangeTest, NativeConfigurationConversionTest) {
  @autoreleasepool {
    RTCIntervalRangeTest *test = [[RTCIntervalRangeTest alloc] init];
    [test testConversionToNativeConfiguration];
    [test testNativeConversionToConfiguration];
  }
}
