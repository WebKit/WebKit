/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCPeerConnectionFactoryOptions.h"

#include "api/peerconnectioninterface.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCPeerConnectionFactoryOptions ()

/** Returns the equivalent native PeerConnectionFactoryInterface::Options
 * structure. */
@property(nonatomic, readonly)
    webrtc::PeerConnectionFactoryInterface::Options nativeOptions;

@end

NS_ASSUME_NONNULL_END
