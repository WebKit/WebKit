/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/windows/video_capture_mf.h"

namespace webrtc {
namespace videocapturemodule {

VideoCaptureMF::VideoCaptureMF(const int32_t id) : VideoCaptureImpl(id) {}
VideoCaptureMF::~VideoCaptureMF() {}

int32_t VideoCaptureMF::Init(const int32_t id, const char* device_id) {
  return 0;
}

int32_t VideoCaptureMF::StartCapture(
    const VideoCaptureCapability& capability) {
  return -1;
}

int32_t VideoCaptureMF::StopCapture() {
  return -1;
}

bool VideoCaptureMF::CaptureStarted() {
  return false;
}

int32_t VideoCaptureMF::CaptureSettings(
    VideoCaptureCapability& settings) {
  return -1;
}

}  // namespace videocapturemodule
}  // namespace webrtc
