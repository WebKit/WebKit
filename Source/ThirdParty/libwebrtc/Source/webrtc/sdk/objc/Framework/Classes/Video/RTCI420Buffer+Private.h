/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "api/video/i420_buffer.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCI420Buffer ()

/** Initialize an RTCI420Buffer with its backing I420BufferInterface. */
- (instancetype)initWithFrameBuffer:(rtc::scoped_refptr<webrtc::I420BufferInterface>)i420Buffer;
- (rtc::scoped_refptr<webrtc::I420BufferInterface>)nativeI420Buffer;

@end

NS_ASSUME_NONNULL_END
