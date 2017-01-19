/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

@class RTCSessionDescription;

@interface ARDSDPUtils : NSObject

// Updates the original SDP description to instead prefer the specified video
// codec. We do this by placing the specified codec at the beginning of the
// codec list if it exists in the sdp.
+ (RTCSessionDescription *)
    descriptionForDescription:(RTCSessionDescription *)description
          preferredVideoCodec:(NSString *)codec;

@end
