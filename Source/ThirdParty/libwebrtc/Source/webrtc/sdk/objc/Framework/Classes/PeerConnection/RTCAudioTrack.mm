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

#import "NSString+StdString.h"
#import "RTCAudioSource+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"

#include "rtc_base/checks.h"

@implementation RTCAudioTrack

@synthesize source = _source;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                         source:(RTCAudioSource *)source
                        trackId:(NSString *)trackId {
  RTC_DCHECK(factory);
  RTC_DCHECK(source);
  RTC_DCHECK(trackId.length);

  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      factory.nativeFactory->CreateAudioTrack(nativeId, source.nativeAudioSource);
  if ([self initWithNativeTrack:track type:RTCMediaStreamTrackTypeAudio]) {
    _source = source;
  }
  return self;
}

- (instancetype)initWithNativeTrack:
    (rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
                               type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(nativeTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeAudio);
  return [super initWithNativeTrack:nativeTrack type:type];
}


- (RTCAudioSource *)source {
  if (!_source) {
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        self.nativeAudioTrack->GetSource();
    if (source) {
      _source = [[RTCAudioSource alloc] initWithNativeAudioSource:source.get()];
    }
  }
  return _source;
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get());
}

@end
