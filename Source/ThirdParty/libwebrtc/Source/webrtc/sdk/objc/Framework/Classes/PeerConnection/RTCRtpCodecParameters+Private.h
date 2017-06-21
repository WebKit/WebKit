/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCRtpCodecParameters.h"

#include "webrtc/api/rtpparameters.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCRtpCodecParameters ()

/** Returns the equivalent native RtpCodecParameters structure. */
@property(nonatomic, readonly) webrtc::RtpCodecParameters nativeParameters;

/** Initialize the object with a native RtpCodecParameters structure. */
- (instancetype)initWithNativeParameters:
    (const webrtc::RtpCodecParameters &)nativeParameters;

@end

NS_ASSUME_NONNULL_END
