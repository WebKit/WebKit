/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDExternalSampleCapturer.h"

#import <WebRTC/RTCCVPixelBuffer.h>
#import <WebRTC/RTCVideoFrameBuffer.h>

@implementation ARDExternalSampleCapturer

- (instancetype)initWithDelegate:(__weak id<RTCVideoCapturerDelegate>)delegate {
  return [super initWithDelegate:delegate];
}

#pragma mark - ARDExternalSampleDelegate

- (void)didCaptureSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  if (CMSampleBufferGetNumSamples(sampleBuffer) != 1 || !CMSampleBufferIsValid(sampleBuffer) ||
      !CMSampleBufferDataIsReady(sampleBuffer)) {
    return;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (pixelBuffer == nil) {
    return;
  }

  RTCCVPixelBuffer *rtcPixelBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer];
  int64_t timeStampNs =
      CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) * NSEC_PER_SEC;
  RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:rtcPixelBuffer
                                                           rotation:RTCVideoRotation_0
                                                        timeStampNs:timeStampNs];
  [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
}

@end
