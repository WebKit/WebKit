/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaSource+Private.h"

#include "rtc_base/checks.h"

@implementation RTCMediaSource {
  RTCMediaSourceType _type;
}

@synthesize nativeMediaSource = _nativeMediaSource;

- (instancetype)initWithNativeMediaSource:
   (rtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource
                                     type:(RTCMediaSourceType)type {
  RTC_DCHECK(nativeMediaSource);
  if (self = [super init]) {
    _nativeMediaSource = nativeMediaSource;
    _type = type;
  }
  return self;
}

- (RTCSourceState)state {
  return [[self class] sourceStateForNativeState:_nativeMediaSource->state()];
}

#pragma mark - Private

+ (webrtc::MediaSourceInterface::SourceState)nativeSourceStateForState:
    (RTCSourceState)state {
  switch (state) {
    case RTCSourceStateInitializing:
      return webrtc::MediaSourceInterface::kInitializing;
    case RTCSourceStateLive:
      return webrtc::MediaSourceInterface::kLive;
    case RTCSourceStateEnded:
      return webrtc::MediaSourceInterface::kEnded;
    case RTCSourceStateMuted:
      return webrtc::MediaSourceInterface::kMuted;
  }
}

+ (RTCSourceState)sourceStateForNativeState:
    (webrtc::MediaSourceInterface::SourceState)nativeState {
  switch (nativeState) {
    case webrtc::MediaSourceInterface::kInitializing:
      return RTCSourceStateInitializing;
    case webrtc::MediaSourceInterface::kLive:
      return RTCSourceStateLive;
    case webrtc::MediaSourceInterface::kEnded:
      return RTCSourceStateEnded;
    case webrtc::MediaSourceInterface::kMuted:
      return RTCSourceStateMuted;
  }
}

+ (NSString *)stringForState:(RTCSourceState)state {
  switch (state) {
    case RTCSourceStateInitializing:
      return @"Initializing";
    case RTCSourceStateLive:
      return @"Live";
    case RTCSourceStateEnded:
      return @"Ended";
    case RTCSourceStateMuted:
      return @"Muted";
  }
}

@end
