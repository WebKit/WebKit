/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDFileCaptureController.h"

#import "WebRTC/RTCFileVideoCapturer.h"

@interface ARDFileCaptureController ()

@property(nonatomic, strong) RTCFileVideoCapturer *fileCapturer;

@end

@implementation ARDFileCaptureController
@synthesize fileCapturer = _fileCapturer;

- (instancetype)initWithCapturer:(RTCFileVideoCapturer *)capturer {
  if (self = [super init]) {
    _fileCapturer = capturer;
  }
  return self;
}

- (void)startCapture {
  [self startFileCapture];
}

- (void)startFileCapture {
  [self.fileCapturer startCapturingFromFileNamed:@"foreman.mp4"
                                         onError:^(NSError *_Nonnull error) {
                                           NSLog(@"Error %@", error.userInfo);
                                         }];
}

- (void)stopCapture {
  [self.fileCapturer stopCapture];
}
@end
