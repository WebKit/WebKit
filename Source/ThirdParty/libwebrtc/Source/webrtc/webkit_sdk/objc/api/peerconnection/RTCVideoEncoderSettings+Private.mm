/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoEncoderSettings+Private.h"

#import "helpers/NSString+StdString.h"

@implementation RTCVideoEncoderSettings (Private)

- (instancetype)initWithNativeVideoCodec:(const webrtc::VideoCodec *)videoCodec {
  if (self = [super init]) {
    if (videoCodec) {
      const char *codecName = CodecTypeToPayloadString(videoCodec->codecType);
      self.name = [NSString stringWithUTF8String:codecName];

      self.width = videoCodec->width;
      self.height = videoCodec->height;
      self.startBitrate = videoCodec->startBitrate;
      self.maxBitrate = videoCodec->maxBitrate;
      self.minBitrate = videoCodec->minBitrate;
      self.maxFramerate = videoCodec->maxFramerate;
      self.qpMax = videoCodec->qpMax;
      self.mode = (RTCVideoCodecMode)videoCodec->mode;
    }
  }

  return self;
}

- (webrtc::VideoCodec)nativeVideoCodec {
  webrtc::VideoCodec videoCodec;
  videoCodec.width = self.width;
  videoCodec.height = self.height;
  videoCodec.startBitrate = self.startBitrate;
  videoCodec.maxBitrate = self.maxBitrate;
  videoCodec.minBitrate = self.minBitrate;
  videoCodec.maxBitrate = self.maxBitrate;
  videoCodec.qpMax = self.qpMax;
  videoCodec.mode = (webrtc::VideoCodecMode)self.mode;

  return videoCodec;
}

@end
