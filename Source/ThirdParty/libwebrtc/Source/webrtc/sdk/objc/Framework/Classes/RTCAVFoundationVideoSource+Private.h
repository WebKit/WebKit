/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCAVFoundationVideoSource.h"

#include "avfoundationvideocapturer.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCAVFoundationVideoSource ()

@property(nonatomic, readonly) webrtc::AVFoundationVideoCapturer *capturer;

/** Initialize an RTCAVFoundationVideoSource with constraints. */
- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                    constraints:(nullable RTCMediaConstraints *)constraints;

@end

NS_ASSUME_NONNULL_END
