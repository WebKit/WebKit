/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMacros.h"
#import "RTCMediaStreamTrack.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCAudioSource;

RTC_OBJC_EXPORT
@interface RTCAudioTrack : RTCMediaStreamTrack

- (instancetype)init NS_UNAVAILABLE;

/** The audio source for this audio track. */
@property(nonatomic, readonly) RTCAudioSource *source;

@end

NS_ASSUME_NONNULL_END
