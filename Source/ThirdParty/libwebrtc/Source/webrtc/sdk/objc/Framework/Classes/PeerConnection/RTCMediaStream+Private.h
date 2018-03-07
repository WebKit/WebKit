/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCMediaStream.h"

#include "api/mediastreaminterface.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCMediaStream ()

/**
 * MediaStreamInterface representation of this RTCMediaStream object. This is
 * needed to pass to the underlying C++ APIs.
 */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::MediaStreamInterface> nativeMediaStream;

/** Initialize an RTCMediaStream with an id. */
- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                       streamId:(NSString *)streamId;

/** Initialize an RTCMediaStream from a native MediaStreamInterface. */
- (instancetype)initWithNativeMediaStream:
    (rtc::scoped_refptr<webrtc::MediaStreamInterface>)nativeMediaStream;

@end

NS_ASSUME_NONNULL_END
