/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCRtpParameters.h"

#include "webrtc/api/rtpparameters.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCRtpParameters ()

/** Returns the equivalent native RtpParameters structure. */
@property(nonatomic, readonly) webrtc::RtpParameters nativeParameters;

/** Initialize the object with a native RtpParameters structure. */
- (instancetype)initWithNativeParameters:
    (const webrtc::RtpParameters &)nativeParameters;

@end

NS_ASSUME_NONNULL_END
