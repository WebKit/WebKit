/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/api/video_frame_buffer.h"

#include "sdk/objc/native/src/objc_frame_buffer.h"

namespace webrtc {

rtc::scoped_refptr<VideoFrameBuffer> ObjCToNativeVideoFrameBuffer(
    id<RTCVideoFrameBuffer> objc_video_frame_buffer) {
  return new rtc::RefCountedObject<ObjCFrameBuffer>(objc_video_frame_buffer);
}

id<RTCVideoFrameBuffer> NativeToObjCVideoFrameBuffer(
    const rtc::scoped_refptr<VideoFrameBuffer> &buffer) {
  return ToObjCVideoFrameBuffer(buffer);
}

}  // namespace webrtc
