/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCCodecSpecificInfoH264+Private.h"

#import "RTCH264ProfileLevelId.h"

// H264 specific settings.
@implementation RTCCodecSpecificInfoH264

@synthesize packetizationMode = _packetizationMode;

- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo {
  webrtc::CodecSpecificInfo codecSpecificInfo;
  codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
  codecSpecificInfo.codec_name = [kRTCVideoCodecH264Name cStringUsingEncoding:NSUTF8StringEncoding];
  codecSpecificInfo.codecSpecific.H264.packetization_mode =
      (webrtc::H264PacketizationMode)_packetizationMode;

  return codecSpecificInfo;
}

@end
