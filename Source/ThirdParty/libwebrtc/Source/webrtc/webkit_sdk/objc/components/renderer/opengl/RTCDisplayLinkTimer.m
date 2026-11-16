/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDisplayLinkTimer.h"

#import <UIKit/UIKit.h>

@implementation RTCDisplayLinkTimer {
  CADisplayLink *_displayLink;
  void (^_timerHandler)(void);
}

- (instancetype)initWithTimerHandler:(void (^)(void))timerHandler {
  NSParameterAssert(timerHandler);
  if (self = [super init]) {
    _timerHandler = timerHandler;
    _displayLink =
        [CADisplayLink displayLinkWithTarget:self
                                    selector:@selector(displayLinkDidFire:)];
    _displayLink.paused = YES;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
    _displayLink.preferredFramesPerSecond = 30;
#else
    [_displayLink setFrameInterval:2];
#endif
    [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  return self;
}

- (void)dealloc {
  [self invalidate];
}

- (BOOL)isPaused {
  return _displayLink.paused;
}

- (void)setIsPaused:(BOOL)isPaused {
  _displayLink.paused = isPaused;
}

- (void)invalidate {
  [_displayLink invalidate];
}

- (void)displayLinkDidFire:(CADisplayLink *)displayLink {
  _timerHandler();
}

@end
