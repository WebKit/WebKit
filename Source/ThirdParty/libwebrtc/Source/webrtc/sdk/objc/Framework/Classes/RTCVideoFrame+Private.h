/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/common_video/rotation.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCVideoFrame ()

@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> i420Buffer;

- (instancetype)initWithVideoBuffer:
                    (rtc::scoped_refptr<webrtc::VideoFrameBuffer>)videoBuffer
                           rotation:(webrtc::VideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
