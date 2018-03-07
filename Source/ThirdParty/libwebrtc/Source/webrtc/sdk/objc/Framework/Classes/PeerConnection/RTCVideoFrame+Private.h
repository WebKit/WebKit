/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCI420Buffer+Private.h"
#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#include "api/video/video_frame.h"
#include "sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCVideoFrame ()

- (instancetype)initWithNativeVideoFrame:(const webrtc::VideoFrame &)frame;
- (webrtc::VideoFrame)nativeVideoFrame;

@end

NS_ASSUME_NONNULL_END
