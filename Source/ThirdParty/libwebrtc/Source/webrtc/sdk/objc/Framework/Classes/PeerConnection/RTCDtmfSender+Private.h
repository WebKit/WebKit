/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCDtmfSender.h"

#include "api/dtmfsenderinterface.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCDtmfSender : NSObject <RTCDtmfSender>

@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::DtmfSenderInterface> nativeDtmfSender;

- (instancetype)init NS_UNAVAILABLE;

/** Initialize an RTCDtmfSender with a native DtmfSenderInterface. */
- (instancetype)initWithNativeDtmfSender:
        (rtc::scoped_refptr<webrtc::DtmfSenderInterface>)nativeDtmfSender NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
