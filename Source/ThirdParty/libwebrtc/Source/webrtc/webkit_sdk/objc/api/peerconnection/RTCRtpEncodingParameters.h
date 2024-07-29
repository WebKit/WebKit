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

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/** Corresponds to webrtc::Priority. */
typedef NS_ENUM(NSInteger, RTCPriority) {
  RTCPriorityVeryLow,
  RTCPriorityLow,
  RTCPriorityMedium,
  RTCPriorityHigh
};

RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCRtpEncodingParameters")))
@interface RTCRtpEncodingParameters : NSObject

/** The idenfifier for the encoding layer. This is used in simulcast. */
@property(nonatomic, copy, nullable) NSString *rid;

/** Controls whether the encoding is currently transmitted. */
@property(nonatomic, assign) BOOL isActive;

/** The maximum bitrate to use for the encoding, or nil if there is no
 *  limit.
 */
@property(nonatomic, copy, nullable) NSNumber *maxBitrateBps;

/** The minimum bitrate to use for the encoding, or nil if there is no
 *  limit.
 */
@property(nonatomic, copy, nullable) NSNumber *minBitrateBps;

/** The maximum framerate to use for the encoding, or nil if there is no
 *  limit.
 */
@property(nonatomic, copy, nullable) NSNumber *maxFramerate;

/** The requested number of temporal layers to use for the encoding, or nil
 * if the default should be used.
 */
@property(nonatomic, copy, nullable) NSNumber *numTemporalLayers;

/** Scale the width and height down by this factor for video. If nil,
 * implementation default scaling factor will be used.
 */
@property(nonatomic, copy, nullable) NSNumber *scaleResolutionDownBy;

/** The SSRC being used by this encoding. */
@property(nonatomic, readonly, nullable) NSNumber *ssrc;

/** The relative DiffServ Code Point priority. */
@property(nonatomic, assign) RTCPriority networkPriority;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
