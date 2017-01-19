/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCRtpSender+Private.h"

#import "NSString+StdString.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCRtpParameters+Private.h"
#import "WebRTC/RTCLogging.h"

#include "webrtc/api/mediastreaminterface.h"

@implementation RTCRtpSender {
  rtc::scoped_refptr<webrtc::RtpSenderInterface> _nativeRtpSender;
}

- (NSString *)senderId {
  return [NSString stringForStdString:_nativeRtpSender->id()];
}

- (RTCRtpParameters *)parameters {
  return [[RTCRtpParameters alloc]
      initWithNativeParameters:_nativeRtpSender->GetParameters()];
}

- (void)setParameters:(RTCRtpParameters *)parameters {
  if (!_nativeRtpSender->SetParameters(parameters.nativeParameters)) {
    RTCLogError(@"RTCRtpSender(%p): Failed to set parameters: %@", self,
        parameters);
  }
}

- (RTCMediaStreamTrack *)track {
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack(
    _nativeRtpSender->track());
  if (nativeTrack) {
    return [[RTCMediaStreamTrack alloc] initWithNativeTrack:nativeTrack];
  }
  return nil;
}

- (void)setTrack:(RTCMediaStreamTrack *)track {
  if (!_nativeRtpSender->SetTrack(track.nativeTrack)) {
    RTCLogError(@"RTCRtpSender(%p): Failed to set track %@", self, track);
  }
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCRtpSender {\n  senderId: %@\n}",
      self.senderId];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (object == nil) {
    return NO;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }
  RTCRtpSender *sender = (RTCRtpSender *)object;
  return _nativeRtpSender == sender.nativeRtpSender;
}

- (NSUInteger)hash {
  return (NSUInteger)_nativeRtpSender.get();
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  return _nativeRtpSender;
}

- (instancetype)initWithNativeRtpSender:
    (rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  NSParameterAssert(nativeRtpSender);
  if (self = [super init]) {
    _nativeRtpSender = nativeRtpSender;
    RTCLogInfo(@"RTCRtpSender(%p): created sender: %@", self, self.description);
  }
  return self;
}

@end
