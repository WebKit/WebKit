/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

/**
 * Represents the state of the track. This exposes the same states in C++.
 */
typedef NS_ENUM(NSInteger, RTCMediaStreamTrackState) {
  RTCMediaStreamTrackStateLive,
  RTCMediaStreamTrackStateEnded
};

NS_ASSUME_NONNULL_BEGIN

RTC_EXTERN NSString *const kRTCMediaStreamTrackKindAudio;
RTC_EXTERN NSString *const kRTCMediaStreamTrackKindVideo;

RTC_OBJC_EXPORT
@interface RTCMediaStreamTrack : NSObject

/**
 * The kind of track. For example, "audio" if this track represents an audio
 * track and "video" if this track represents a video track.
 */
@property(nonatomic, readonly) NSString *kind;

/** An identifier string. */
@property(nonatomic, readonly) NSString *trackId;

/** The enabled state of the track. */
@property(nonatomic, assign) BOOL isEnabled;

/** The state of the track. */
@property(nonatomic, readonly) RTCMediaStreamTrackState readyState;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
