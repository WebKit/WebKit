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

@implementation RTCVideoEncoderSettings {
  webrtc::VideoCodec _nativeVideoCodec;
}

@synthesize name = _name;
@synthesize width = _width;
@synthesize height = _height;
@synthesize startBitrate = _startBitrate;
@synthesize maxBitrate = _maxBitrate;
@synthesize minBitrate = _minBitrate;
@synthesize targetBitrate = _targetBitrate;
@synthesize maxFramerate = _maxFramerate;
@synthesize qpMax = _qpMax;
@synthesize mode = _mode;

@end

@implementation RTCVideoEncoderSettings (Private)

- (instancetype)initWithNativeVideoCodec:(const webrtc::VideoCodec *)videoCodec {
  if (self = [super init]) {
    if (videoCodec) {
      const char *codecName = CodecTypeToPayloadString(videoCodec->codecType);
      self.name = [NSString stringWithUTF8String:codecName];

      _nativeVideoCodec = *videoCodec;
      self.width = videoCodec->width;
      self.height = videoCodec->height;
      self.startBitrate = videoCodec->startBitrate;
      self.maxBitrate = videoCodec->maxBitrate;
      self.minBitrate = videoCodec->minBitrate;
      self.targetBitrate = videoCodec->targetBitrate;
      self.maxFramerate = videoCodec->maxFramerate;
      self.qpMax = videoCodec->qpMax;
      self.mode = (RTCVideoCodecMode)videoCodec->mode;
    }
  }

  return self;
}

- (webrtc::VideoCodec)nativeVideoCodec {
  return _nativeVideoCodec;
}

@end

@implementation RTCVideoBitrateAllocation {
  webrtc::VideoBitrateAllocation _nativeVideoBitrateAllocation;
}

@end

@implementation RTCVideoBitrateAllocation (Private)

- (instancetype)initWithNativeVideoBitrateAllocation:(const webrtc::VideoBitrateAllocation *)videoBitrateAllocation {
  if (self = [super init]) {
    _nativeVideoBitrateAllocation = *videoBitrateAllocation;
  }
  return self;
}

- (webrtc::VideoBitrateAllocation)nativeVideoBitrateAllocation {
  return _nativeVideoBitrateAllocation;
}

@end
