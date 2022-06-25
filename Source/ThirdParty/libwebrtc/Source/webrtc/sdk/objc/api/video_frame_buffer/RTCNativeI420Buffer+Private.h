/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCNativeI420Buffer.h"

#include "api/video/i420_buffer.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCI420Buffer () {
 @protected
  rtc::scoped_refptr<webrtc::I420BufferInterface> _i420Buffer;
}

/** Initialize an RTCI420Buffer with its backing I420BufferInterface. */
- (instancetype)initWithFrameBuffer:(rtc::scoped_refptr<webrtc::I420BufferInterface>)i420Buffer;
- (rtc::scoped_refptr<webrtc::I420BufferInterface>)nativeI420Buffer;

#if defined(WEBRTC_WEBKIT_BUILD)
- (void)close;
#endif
@end

NS_ASSUME_NONNULL_END
