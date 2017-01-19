/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCVideoSource.h>

@class AVCaptureSession;
@class RTCMediaConstraints;
@class RTCPeerConnectionFactory;

NS_ASSUME_NONNULL_BEGIN

/**
 * RTCAVFoundationVideoSource is a video source that uses
 * webrtc::AVFoundationVideoCapturer. We do not currently provide a wrapper for
 * that capturer because cricket::VideoCapturer is not ref counted and we cannot
 * guarantee its lifetime. Instead, we expose its properties through the ref
 * counted video source interface.
 */
RTC_EXPORT
@interface RTCAVFoundationVideoSource : RTCVideoSource

- (instancetype)init NS_UNAVAILABLE;

/** Returns whether rear-facing camera is available for use. */
@property(nonatomic, readonly) BOOL canUseBackCamera;

/** Switches the camera being used (either front or back). */
@property(nonatomic, assign) BOOL useBackCamera;

/** Returns the active capture session. */
@property(nonatomic, readonly) AVCaptureSession *captureSession;

@end

NS_ASSUME_NONNULL_END
