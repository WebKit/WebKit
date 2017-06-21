/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#ifdef __cplusplus
#include "avfoundationvideocapturer.h"
#endif

NS_ASSUME_NONNULL_BEGIN
// This class is an implementation detail of AVFoundationVideoCapturer and handles
// the ObjC integration with the AVFoundation APIs.
// It is meant to be owned by an instance of AVFoundationVideoCapturer.
// The reason for this is because other webrtc objects own cricket::VideoCapturer, which is not
// ref counted. To prevent bad behavior we do not expose this class directly.
@interface RTCAVFoundationVideoCapturerInternal
    : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

@property(nonatomic, readonly) AVCaptureSession *captureSession;
@property(nonatomic, readonly) dispatch_queue_t frameQueue;
@property(nonatomic, readonly) BOOL canUseBackCamera;
@property(nonatomic, assign) BOOL useBackCamera;  // Defaults to NO.
@property(atomic, assign) BOOL isRunning;  // Whether the capture session is running.
@property(atomic, assign) BOOL hasStarted;  // Whether we have an unmatched start.

// We keep a pointer back to AVFoundationVideoCapturer to make callbacks on it
// when we receive frames. This is safe because this object should be owned by
// it.
- (instancetype)initWithCapturer:(webrtc::AVFoundationVideoCapturer *)capturer;
- (AVCaptureDevice *)getActiveCaptureDevice;

- (nullable AVCaptureDevice *)frontCaptureDevice;
- (nullable AVCaptureDevice *)backCaptureDevice;

// Starts and stops the capture session asynchronously. We cannot do this
// synchronously without blocking a WebRTC thread.
- (void)start;
- (void)stop;

@end
NS_ASSUME_NONNULL_END
