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
 * DEPRECATED Use RTCCameraVideoCapturer instead.
 *
 * RTCAVFoundationVideoSource is a video source that uses
 * webrtc::AVFoundationVideoCapturer. We do not currently provide a wrapper for
 * that capturer because cricket::VideoCapturer is not ref counted and we cannot
 * guarantee its lifetime. Instead, we expose its properties through the ref
 * counted video source interface.
 */
RTC_EXPORT
@interface RTCAVFoundationVideoSource : RTCVideoSource

- (instancetype)init NS_UNAVAILABLE;

/**
 * Calling this function will cause frames to be scaled down to the
 * requested resolution. Also, frames will be cropped to match the
 * requested aspect ratio, and frames will be dropped to match the
 * requested fps. The requested aspect ratio is orientation agnostic and
 * will be adjusted to maintain the input orientation, so it doesn't
 * matter if e.g. 1280x720 or 720x1280 is requested.
 */
- (void)adaptOutputFormatToWidth:(int)width height:(int)height fps:(int)fps;

/** Returns whether rear-facing camera is available for use. */
@property(nonatomic, readonly) BOOL canUseBackCamera;

/** Switches the camera being used (either front or back). */
@property(nonatomic, assign) BOOL useBackCamera;

/** Returns the active capture session. */
@property(nonatomic, readonly) AVCaptureSession *captureSession;

@end

NS_ASSUME_NONNULL_END
