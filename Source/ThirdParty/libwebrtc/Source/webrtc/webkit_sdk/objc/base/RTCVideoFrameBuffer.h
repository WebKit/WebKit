/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@protocol RTCI420Buffer;

// RTCVideoFrameBuffer is an ObjectiveC version of webrtc::VideoFrameBuffer.
RTC_OBJC_EXPORT
@protocol RTCVideoFrameBuffer <NSObject>

@property(nonatomic, readonly) int width;
@property(nonatomic, readonly) int height;

- (id<RTCI420Buffer>)toI420;

#if defined(WEBRTC_WEBKIT_BUILD)
- (void)close;
#endif
@end

NS_ASSUME_NONNULL_END
