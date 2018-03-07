/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCPeerConnectionFactory.h"

#include "api/peerconnectioninterface.h"
#include "rtc_base/scoped_ref_ptr.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCPeerConnectionFactory ()

/**
 * PeerConnectionFactoryInterface created and held by this
 * RTCPeerConnectionFactory object. This is needed to pass to the underlying
 * C++ APIs.
 */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> nativeFactory;

@end

NS_ASSUME_NONNULL_END
