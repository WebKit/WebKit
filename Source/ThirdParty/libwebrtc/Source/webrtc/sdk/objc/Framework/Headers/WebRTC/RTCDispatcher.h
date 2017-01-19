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

#import <WebRTC/RTCMacros.h>

typedef NS_ENUM(NSInteger, RTCDispatcherQueueType) {
  // Main dispatcher queue.
  RTCDispatcherTypeMain,
  // Used for starting/stopping AVCaptureSession, and assigning
  // capture session to AVCaptureVideoPreviewLayer.
  RTCDispatcherTypeCaptureSession,
  // Used for operations on AVAudioSession.
  RTCDispatcherTypeAudioSession,
};

/** Dispatcher that asynchronously dispatches blocks to a specific
 *  shared dispatch queue.
 */
RTC_EXPORT
@interface RTCDispatcher : NSObject

- (instancetype)init NS_UNAVAILABLE;

/** Dispatch the block asynchronously on the queue for dispatchType.
 *  @param dispatchType The queue type to dispatch on.
 *  @param block The block to dispatch asynchronously.
 */
+ (void)dispatchAsyncOnType:(RTCDispatcherQueueType)dispatchType
                      block:(dispatch_block_t)block;

@end
