/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@interface RTCRtpEncodingParameters : NSObject

/** Controls whether the encoding is currently transmitted. */
@property(nonatomic, assign) BOOL isActive;

/** The maximum bitrate to use for the encoding, or nil if there is no
 *  limit.
 */
@property(nonatomic, copy, nullable) NSNumber *maxBitrateBps;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
