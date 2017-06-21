/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCAudioTrack.h"

#include "webrtc/api/mediastreaminterface.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCPeerConnectionFactory;
@interface RTCAudioTrack ()

/** AudioTrackInterface created or passed in at construction. */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::AudioTrackInterface> nativeAudioTrack;

/** Initialize an RTCAudioTrack with an id. */
- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                         source:(RTCAudioSource *)source
                        trackId:(NSString *)trackId;

@end

NS_ASSUME_NONNULL_END
