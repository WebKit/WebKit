/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_MF_H_
#define MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_MF_H_

#include "modules/video_capture/video_capture_impl.h"

namespace webrtc {
namespace videocapturemodule {

// VideoCapture implementation that uses the Media Foundation API on Windows.
// This will replace the DirectShow based implementation on Vista and higher.
// TODO(tommi): Finish implementing and switch out the DS in the factory method
// for supported platforms.
class VideoCaptureMF : public VideoCaptureImpl {
 public:
  VideoCaptureMF();

  int32_t Init(const char* device_id);

  // Overrides from VideoCaptureImpl.
  virtual int32_t StartCapture(const VideoCaptureCapability& capability);
  virtual int32_t StopCapture();
  virtual bool CaptureStarted();
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings);  // NOLINT

 protected:
  virtual ~VideoCaptureMF();
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_MF_H_
