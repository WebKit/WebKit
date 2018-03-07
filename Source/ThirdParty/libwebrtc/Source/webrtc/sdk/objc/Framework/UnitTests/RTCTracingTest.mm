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

#include <vector>

#include "rtc_base/gunit.h"

#import "NSString+StdString.h"
#import "WebRTC/RTCTracing.h"

@interface RTCTracingTest : NSObject
- (void)tracingTestNoInitialization;
@end

@implementation RTCTracingTest

- (NSString *)documentsFilePathForFileName:(NSString *)fileName {
  NSParameterAssert(fileName.length);
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = paths.firstObject;
  NSString *filePath =
  [documentsDirPath stringByAppendingPathComponent:fileName];
  return filePath;
}

- (void)tracingTestNoInitialization {
  NSString *filePath = [self documentsFilePathForFileName:@"webrtc-trace.txt"];
  EXPECT_EQ(NO, RTCStartInternalCapture(filePath));
  RTCStopInternalCapture();
}

@end


TEST(RTCTracingTest, TracingTestNoInitialization) {
  @autoreleasepool {
    RTCTracingTest *test = [[RTCTracingTest alloc] init];
    [test tracingTestNoInitialization];
  }
}

