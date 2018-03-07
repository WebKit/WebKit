/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodecFactory.h"

#import "WebRTC/RTCVideoCodecH264.h"
#import "WebRTC/RTCVideoDecoderVP8.h"
#import "WebRTC/RTCVideoDecoderVP9.h"

@implementation RTCDefaultVideoDecoderFactory

- (id<RTCVideoDecoder>)createDecoder:(RTCVideoCodecInfo *)info {
  if ([info.name isEqualToString:kRTCVideoCodecH264Name]) {
    return [[RTCVideoDecoderH264 alloc] init];
  } else if ([info.name isEqualToString:kRTCVideoCodecVp8Name]) {
    return [RTCVideoDecoderVP8 vp8Decoder];
  } else if ([info.name isEqualToString:kRTCVideoCodecVp9Name]) {
    return [RTCVideoDecoderVP9 vp9Decoder];
  }

  return nil;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  return @[
    [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name],
    [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp8Name],
    [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name]
  ];
}

@end
