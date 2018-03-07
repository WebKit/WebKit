/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_IOS)

#import <Foundation/Foundation.h>

@interface RTCUIApplicationStatusObserver : NSObject

+ (instancetype)sharedInstance;
+ (void)prepareForUse;

- (BOOL)isApplicationActive;

@end

#endif  // WEBRTC_IOS
