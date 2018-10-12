/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaSource.h"

#include "api/mediastreaminterface.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCPeerConnectionFactory;

typedef NS_ENUM(NSInteger, RTCMediaSourceType) {
  RTCMediaSourceTypeAudio,
  RTCMediaSourceTypeVideo,
};

@interface RTCMediaSource ()

@property(nonatomic, readonly) rtc::scoped_refptr<webrtc::MediaSourceInterface> nativeMediaSource;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
              nativeMediaSource:(rtc::scoped_refptr<webrtc::MediaSourceInterface>)nativeMediaSource
                           type:(RTCMediaSourceType)type NS_DESIGNATED_INITIALIZER;

+ (webrtc::MediaSourceInterface::SourceState)nativeSourceStateForState:(RTCSourceState)state;

+ (RTCSourceState)sourceStateForNativeState:(webrtc::MediaSourceInterface::SourceState)nativeState;

+ (NSString *)stringForState:(RTCSourceState)state;

@end

NS_ASSUME_NONNULL_END
