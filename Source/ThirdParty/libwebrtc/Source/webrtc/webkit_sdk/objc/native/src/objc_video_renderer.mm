/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/src/objc_video_renderer.h"

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoRenderer.h"

#include "sdk/objc/native/src/objc_video_frame.h"

namespace webrtc {

ObjCVideoRenderer::ObjCVideoRenderer(id<RTCVideoRenderer> renderer)
    : renderer_(renderer), size_(CGSizeZero) {}

void ObjCVideoRenderer::OnFrame(const VideoFrame& nativeVideoFrame) {
  RTCVideoFrame* videoFrame = ToObjCVideoFrame(nativeVideoFrame);

  CGSize current_size = (videoFrame.rotation % 180 == 0) ?
      CGSizeMake(videoFrame.width, videoFrame.height) :
      CGSizeMake(videoFrame.height, videoFrame.width);

  if (!CGSizeEqualToSize(size_, current_size)) {
    size_ = current_size;
    [renderer_ setSize:size_];
  }
  [renderer_ renderFrame:videoFrame];
}

}  // namespace webrtc
