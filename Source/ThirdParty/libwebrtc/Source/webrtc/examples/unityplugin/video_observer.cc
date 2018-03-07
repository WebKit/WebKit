/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/unityplugin/video_observer.h"

void VideoObserver::SetVideoCallback(I420FRAMEREADY_CALLBACK callback) {
  std::lock_guard<std::mutex> lock(mutex);
  OnI420FrameReady = callback;
}

void VideoObserver::OnFrame(const webrtc::VideoFrame& frame) {
  std::unique_lock<std::mutex> lock(mutex);
  rtc::scoped_refptr<webrtc::PlanarYuvBuffer> buffer(
      frame.video_frame_buffer()->ToI420());
  if (OnI420FrameReady) {
    OnI420FrameReady(buffer->DataY(), buffer->DataU(), buffer->DataV(),
                     buffer->StrideY(), buffer->StrideU(), buffer->StrideV(),
                     frame.width(), frame.height());
  }
}
