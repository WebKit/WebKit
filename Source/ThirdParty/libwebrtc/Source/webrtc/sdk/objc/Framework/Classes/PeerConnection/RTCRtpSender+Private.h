/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCRtpSender.h"

#include "api/rtpsenderinterface.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCPeerConnectionFactory;

@interface RTCRtpSender ()

@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::RtpSenderInterface> nativeRtpSender;

/** Initialize an RTCRtpSender with a native RtpSenderInterface. */
- (instancetype)initWithFactory:(RTCPeerConnectionFactory*)factory
                nativeRtpSender:(rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
