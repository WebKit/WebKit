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

/** Class used to estimate bitrate based on byte count. It is expected that
 *  byte count is monotonocially increasing. This class tracks the times that
 *  byte count is updated, and measures the bitrate based on the byte difference
 *  over the interval between updates.
 */
@interface ARDBitrateTracker : NSObject

/** The bitrate in bits per second. */
@property(nonatomic, readonly) double bitrate;
/** The bitrate as a formatted string in bps, Kbps or Mbps. */
@property(nonatomic, readonly) NSString *bitrateString;

/** Converts the bitrate to a readable format in bps, Kbps or Mbps. */
+ (NSString *)bitrateStringForBitrate:(double)bitrate;
/** Updates the tracked bitrate with the new byte count. */
- (void)updateBitrateWithCurrentByteCount:(NSInteger)byteCount;

@end
