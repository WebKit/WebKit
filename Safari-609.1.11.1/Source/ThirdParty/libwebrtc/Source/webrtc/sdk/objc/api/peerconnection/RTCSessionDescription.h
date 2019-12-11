/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

/**
 * Represents the session description type. This exposes the same types that are
 * in C++, which doesn't include the rollback type that is in the W3C spec.
 */
typedef NS_ENUM(NSInteger, RTCSdpType) {
  RTCSdpTypeOffer,
  RTCSdpTypePrAnswer,
  RTCSdpTypeAnswer,
};

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@interface RTCSessionDescription : NSObject

/** The type of session description. */
@property(nonatomic, readonly) RTCSdpType type;

/** The SDP string representation of this session description. */
@property(nonatomic, readonly) NSString *sdp;

- (instancetype)init NS_UNAVAILABLE;

/** Initialize a session description with a type and SDP string. */
- (instancetype)initWithType:(RTCSdpType)type sdp:(NSString *)sdp NS_DESIGNATED_INITIALIZER;

+ (NSString *)stringForType:(RTCSdpType)type;

+ (RTCSdpType)typeForString:(NSString *)string;

@end

NS_ASSUME_NONNULL_END
