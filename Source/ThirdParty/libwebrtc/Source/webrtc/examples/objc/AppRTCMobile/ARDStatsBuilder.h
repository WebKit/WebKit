/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

@class RTC_OBJC_TYPE(RTCLegacyStatsReport);

/** Class used to accumulate stats information into a single displayable string.
 */
@interface ARDStatsBuilder : NSObject

/** String that represents the accumulated stats reports passed into this
 *  class.
 */
@property(nonatomic, readonly) NSString *statsString;

/** Parses the information in the stats report into an appropriate internal
 *  format used to generate the stats string.
 */
- (void)parseStatsReport:(RTC_OBJC_TYPE(RTCLegacyStatsReport) *)statsReport;

@end
