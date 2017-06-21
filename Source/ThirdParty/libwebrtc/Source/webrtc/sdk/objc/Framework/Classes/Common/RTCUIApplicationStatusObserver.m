/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "RTCUIApplicationStatusObserver.h"

#if defined(WEBRTC_IOS)

#import <UIKit/UIKit.h>

@implementation RTCUIApplicationStatusObserver {
  UIApplicationState _state;
}

+ (instancetype)sharedInstance {
  static id sharedInstance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[self alloc] init];
  });

  return sharedInstance;
}

- (id)init {
  if (self = [super init]) {
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserverForName:UIApplicationDidBecomeActiveNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification *note) {
                      _state = [UIApplication sharedApplication].applicationState;
                    }];

    [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification *note) {
                      _state = [UIApplication sharedApplication].applicationState;
                    }];

    dispatch_block_t initializeBlock = ^{
      _state = [UIApplication sharedApplication].applicationState;
    };

    if ([NSThread isMainThread]) {
      initializeBlock();
    } else {
      dispatch_sync(dispatch_get_main_queue(), initializeBlock);
    }
  }

  return self;
}

- (BOOL)isApplicationActive {
  return _state == UIApplicationStateActive;
}

@end

#endif  // WEBRTC_IOS
