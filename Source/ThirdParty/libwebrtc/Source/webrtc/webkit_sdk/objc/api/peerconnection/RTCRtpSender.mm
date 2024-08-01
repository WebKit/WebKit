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

#import "RTCDtmfSender+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCRtpParameters+Private.h"
#import "RTCRtpSender+Native.h"
#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/media_stream_interface.h"

@implementation RTCRtpSender {
  RTCPeerConnectionFactory *_factory;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> _nativeRtpSender;
}

@synthesize dtmfSender = _dtmfSender;

- (NSString *)senderId {
  return [NSString stringForStdString:_nativeRtpSender->id()];
}

- (RTCRtpParameters *)parameters {
  return [[RTCRtpParameters alloc]
      initWithNativeParameters:_nativeRtpSender->GetParameters()];
}

- (void)setParameters:(RTCRtpParameters *)parameters {
  if (!_nativeRtpSender->SetParameters(parameters.nativeParameters).ok()) {
    RTCLogError(@"RTCRtpSender(%p): Failed to set parameters: %@", self,
        parameters);
  }
}

- (RTCMediaStreamTrack *)track {
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> nativeTrack(
    _nativeRtpSender->track());
  if (nativeTrack) {
    return [RTCMediaStreamTrack mediaTrackForNativeTrack:nativeTrack factory:_factory];
  }
  return nil;
}

- (void)setTrack:(RTCMediaStreamTrack *)track {
  if (!_nativeRtpSender->SetTrack(track.nativeTrack)) {
    RTCLogError(@"RTCRtpSender(%p): Failed to set track %@", self, track);
  }
}

- (NSArray<NSString *> *)streamIds {
  std::vector<std::string> nativeStreamIds = _nativeRtpSender->stream_ids();
  NSMutableArray *streamIds = [NSMutableArray arrayWithCapacity:nativeStreamIds.size()];
  for (const auto &s : nativeStreamIds) {
    [streamIds addObject:[NSString stringForStdString:s]];
  }
  return streamIds;
}

- (void)setStreamIds:(NSArray<NSString *> *)streamIds {
  std::vector<std::string> nativeStreamIds;
  for (NSString *streamId in streamIds) {
    nativeStreamIds.push_back([streamId UTF8String]);
  }
  _nativeRtpSender->SetStreams(nativeStreamIds);
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

#pragma mark - Native

- (void)setFrameEncryptor:(rtc::scoped_refptr<webrtc::FrameEncryptorInterface>)frameEncryptor {
  _nativeRtpSender->SetFrameEncryptor(frameEncryptor);
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  return _nativeRtpSender;
}

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                nativeRtpSender:(rtc::scoped_refptr<webrtc::RtpSenderInterface>)nativeRtpSender {
  NSParameterAssert(factory);
  NSParameterAssert(nativeRtpSender);
  if (self = [super init]) {
    _factory = factory;
    _nativeRtpSender = nativeRtpSender;
    rtc::scoped_refptr<webrtc::DtmfSenderInterface> nativeDtmfSender(
        _nativeRtpSender->GetDtmfSender());
    if (nativeDtmfSender) {
      _dtmfSender = [[RTCDtmfSender alloc] initWithNativeDtmfSender:nativeDtmfSender];
    }
    RTCLogInfo(@"RTCRtpSender(%p): created sender: %@", self, self.description);
  }
  return self;
}

@end
