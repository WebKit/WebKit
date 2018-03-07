/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_OBJC_VIDEO_CAPTURE_H_
#define MODULES_VIDEO_CAPTURE_OBJC_VIDEO_CAPTURE_H_

#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/scoped_ref_ptr.h"

@class RTCVideoCaptureIosObjC;

namespace webrtc {
namespace videocapturemodule {
class VideoCaptureIos : public VideoCaptureImpl {
 public:
  VideoCaptureIos();
  virtual ~VideoCaptureIos();

  static rtc::scoped_refptr<VideoCaptureModule> Create(
      const char* device_unique_id_utf8);

  // Implementation of VideoCaptureImpl.
  int32_t StartCapture(const VideoCaptureCapability& capability) override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& settings) override;

 private:
  RTCVideoCaptureIosObjC* capture_device_;
  bool is_capturing_;
  VideoCaptureCapability capability_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_OBJC_VIDEO_CAPTURE_H_
