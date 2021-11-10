/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDStatsBuilder.h"

#import "sdk/objc/api/peerconnection/RTCLegacyStatsReport.h"
#import "sdk/objc/base/RTCMacros.h"

#import "ARDUtilities.h"

@implementation ARDStatsBuilder

@synthesize stats = _stats;

- (NSString *)statsString {
  NSMutableString *result = [NSMutableString string];

  [result appendFormat:@"(cpu)%ld%%\n", (long)ARDGetCpuUsagePercentage()];

  for (NSString *key in _stats.statistics) {
    RTC_OBJC_TYPE(RTCStatistics) *stat = _stats.statistics[key];
    [result appendFormat:@"%@\n", stat.description];
  }

  return result;
}

@end

