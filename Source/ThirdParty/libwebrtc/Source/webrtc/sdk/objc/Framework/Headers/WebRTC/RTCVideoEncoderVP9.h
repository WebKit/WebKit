/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCVideoCodec.h>

RTC_EXPORT
@interface RTCVideoEncoderVP9 : NSObject

/* This returns a VP9 encoder that can be returned from a RTCVideoEncoderFactory injected into
 * RTCPeerConnectionFactory. Even though it implements the RTCVideoEncoder protocol, it can not be
 * used independently from the RTCPeerConnectionFactory.
 */
+ (id<RTCVideoEncoder>)vp9Encoder;

@end
