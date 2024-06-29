/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCVideoEncoderVP9.h"
#import "RTCWrappedNativeVideoEncoder.h"

#include "api/environment/environment_factory.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "sdk/objc/api/peerconnection/RTCVideoCodecInfo+Private.h"

@implementation RTCVideoEncoderVP9

+ (id<RTCVideoEncoder>)vp9Encoder:(RTCVideoCodecInfo *)codecInfo {
  return [[RTCWrappedNativeVideoEncoder alloc]
      initWithNativeEncoder:std::unique_ptr<webrtc::VideoEncoder>(webrtc::CreateVp9Encoder(webrtc::EnvironmentFactory().Create(), { webrtc::VP9Profile::kProfile0 }))];
}

@end
