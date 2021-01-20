/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/vcm_capturer.h"

#include <stdint.h>

#include <memory>

#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

VcmCapturer::VcmCapturer() : vcm_(nullptr) {}

bool VcmCapturer::Init(size_t width,
                       size_t height,
                       size_t target_fps,
                       size_t capture_device_index) {
  std::unique_ptr<VideoCaptureModule::DeviceInfo> device_info(
      VideoCaptureFactory::CreateDeviceInfo());

  char device_name[256];
  char unique_name[256];
  if (device_info->GetDeviceName(static_cast<uint32_t>(capture_device_index),
                                 device_name, sizeof(device_name), unique_name,
                                 sizeof(unique_name)) != 0) {
    Destroy();
    return false;
  }

  vcm_ = webrtc::VideoCaptureFactory::Create(unique_name);
  if (!vcm_) {
    return false;
  }
  vcm_->RegisterCaptureDataCallback(this);

  device_info->GetCapability(vcm_->CurrentDeviceName(), 0, capability_);

  capability_.width = static_cast<int32_t>(width);
  capability_.height = static_cast<int32_t>(height);
  capability_.maxFPS = static_cast<int32_t>(target_fps);
  capability_.videoType = VideoType::kI420;

  if (vcm_->StartCapture(capability_) != 0) {
    Destroy();
    return false;
  }

  RTC_CHECK(vcm_->CaptureStarted());

  return true;
}

VcmCapturer* VcmCapturer::Create(size_t width,
                                 size_t height,
                                 size_t target_fps,
                                 size_t capture_device_index) {
  std::unique_ptr<VcmCapturer> vcm_capturer(new VcmCapturer());
  if (!vcm_capturer->Init(width, height, target_fps, capture_device_index)) {
    RTC_LOG(LS_WARNING) << "Failed to create VcmCapturer(w = " << width
                        << ", h = " << height << ", fps = " << target_fps
                        << ")";
    return nullptr;
  }
  return vcm_capturer.release();
}

void VcmCapturer::Destroy() {
  if (!vcm_)
    return;

  vcm_->StopCapture();
  vcm_->DeRegisterCaptureDataCallback();
  // Release reference to VCM.
  vcm_ = nullptr;
}

VcmCapturer::~VcmCapturer() {
  Destroy();
}

void VcmCapturer::OnFrame(const VideoFrame& frame) {
  TestVideoCapturer::OnFrame(frame);
}

}  // namespace test
}  // namespace webrtc
