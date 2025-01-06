/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioTrack+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCVideoTrack+Private.h"

#import "helpers/NSString+StdString.h"

NSString * const kRTCMediaStreamTrackKindAudio =
    @(webrtc::MediaStreamTrackInterface::kAudioKind);
NSString * const kRTCMediaStreamTrackKindVideo =
    @(webrtc::MediaStreamTrackInterface::kVideoKind);

@implementation RTCMediaStreamTrack {
  RTCPeerConnectionFactory *_factory;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> _nativeTrack;
  RTCMediaStreamTrackType _type;
}

- (NSString *)kind {
  return [NSString stringForStdString:_nativeTrack->kind()];
}

- (NSString *)trackId {
  return [NSString stringForStdString:_nativeTrack->id()];
}

- (BOOL)isEnabled {
  return _nativeTrack->enabled();
}

- (void)setIsEnabled:(BOOL)isEnabled {
  _nativeTrack->set_enabled(isEnabled);
}

- (RTCMediaStreamTrackState)readyState {
  return [[self class] trackStateForNativeState:_nativeTrack->state()];
}

- (NSString *)description {
  NSString *readyState = [[self class] stringForState:self.readyState];
  return [NSString stringWithFormat:@"RTCMediaStreamTrack:\n%@\n%@\n%@\n%@",
                                    self.kind,
                                    self.trackId,
                                    self.isEnabled ? @"enabled" : @"disabled",
                                    readyState];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }
  return [self isEqualToTrack:(RTCMediaStreamTrack *)object];
}

- (NSUInteger)hash {
  return (NSUInteger)_nativeTrack.get();
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack {
  return _nativeTrack;
}

@synthesize factory = _factory;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                    nativeTrack:(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
                           type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(nativeTrack);
  NSParameterAssert(factory);
  if (self = [super init]) {
    _factory = factory;
    _nativeTrack = nativeTrack;
    _type = type;
  }
  return self;
}

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                    nativeTrack:(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack {
  NSParameterAssert(nativeTrack);
  if (nativeTrack->kind() ==
      std::string(webrtc::MediaStreamTrackInterface::kAudioKind)) {
    return [self initWithFactory:factory nativeTrack:nativeTrack type:RTCMediaStreamTrackTypeAudio];
  }
  if (nativeTrack->kind() ==
      std::string(webrtc::MediaStreamTrackInterface::kVideoKind)) {
    return [self initWithFactory:factory nativeTrack:nativeTrack type:RTCMediaStreamTrackTypeVideo];
  }
  return nil;
}

- (BOOL)isEqualToTrack:(RTCMediaStreamTrack *)track {
  if (!track) {
    return NO;
  }
  return _nativeTrack == track.nativeTrack;
}

+ (webrtc::MediaStreamTrackInterface::TrackState)nativeTrackStateForState:
    (RTCMediaStreamTrackState)state {
  switch (state) {
    case RTCMediaStreamTrackStateLive:
      return webrtc::MediaStreamTrackInterface::kLive;
    case RTCMediaStreamTrackStateEnded:
      return webrtc::MediaStreamTrackInterface::kEnded;
  }
}

+ (RTCMediaStreamTrackState)trackStateForNativeState:
    (webrtc::MediaStreamTrackInterface::TrackState)nativeState {
  switch (nativeState) {
    case webrtc::MediaStreamTrackInterface::kLive:
      return RTCMediaStreamTrackStateLive;
    case webrtc::MediaStreamTrackInterface::kEnded:
      return RTCMediaStreamTrackStateEnded;
  }
}

+ (NSString *)stringForState:(RTCMediaStreamTrackState)state {
  switch (state) {
    case RTCMediaStreamTrackStateLive:
      return @"Live";
    case RTCMediaStreamTrackStateEnded:
      return @"Ended";
  }
}

+ (RTCMediaStreamTrack *)mediaTrackForNativeTrack:
                             (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
                                          factory:(RTCPeerConnectionFactory *)factory {
  NSParameterAssert(nativeTrack);
  NSParameterAssert(factory);
  if (nativeTrack->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
    return [[RTCAudioTrack alloc] initWithFactory:factory
                                      nativeTrack:nativeTrack
                                             type:RTCMediaStreamTrackTypeAudio];
  } else if (nativeTrack->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
    return [[RTCVideoTrack alloc] initWithFactory:factory
                                      nativeTrack:nativeTrack
                                             type:RTCMediaStreamTrackTypeVideo];
  } else {
    return [[RTCMediaStreamTrack alloc] initWithFactory:factory nativeTrack:nativeTrack];
  }
}

@end
