/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/vcm_capturer.h"

#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

VcmCapturer::VcmCapturer() : started_(false), sink_(nullptr), vcm_(NULL) {}

bool VcmCapturer::Init(size_t width, size_t height, size_t target_fps) {
  VideoCaptureModule::DeviceInfo* device_info =
      VideoCaptureFactory::CreateDeviceInfo();

  char device_name[256];
  char unique_name[256];
  if (device_info->GetDeviceName(0, device_name, sizeof(device_name),
                                 unique_name, sizeof(unique_name)) !=
      0) {
    Destroy();
    return false;
  }

  vcm_ = webrtc::VideoCaptureFactory::Create(unique_name);
  vcm_->RegisterCaptureDataCallback(this);

  device_info->GetCapability(vcm_->CurrentDeviceName(), 0, capability_);
  delete device_info;

  capability_.width = static_cast<int32_t>(width);
  capability_.height = static_cast<int32_t>(height);
  capability_.maxFPS = static_cast<int32_t>(target_fps);
  capability_.rawType = kVideoI420;

  if (vcm_->StartCapture(capability_) != 0) {
    Destroy();
    return false;
  }

  assert(vcm_->CaptureStarted());

  return true;
}

VcmCapturer* VcmCapturer::Create(size_t width,
                                 size_t height,
                                 size_t target_fps) {
  VcmCapturer* vcm_capturer = new VcmCapturer();
  if (!vcm_capturer->Init(width, height, target_fps)) {
    // TODO(pbos): Log a warning that this failed.
    delete vcm_capturer;
    return NULL;
  }
  return vcm_capturer;
}


void VcmCapturer::Start() {
  rtc::CritScope lock(&crit_);
  started_ = true;
}

void VcmCapturer::Stop() {
  rtc::CritScope lock(&crit_);
  started_ = false;
}

void VcmCapturer::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                                  const rtc::VideoSinkWants& wants) {
  rtc::CritScope lock(&crit_);
  RTC_CHECK(!sink_ || sink_ == sink);
  sink_ = sink;
}

void VcmCapturer::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  rtc::CritScope lock(&crit_);
  RTC_CHECK(sink_ == sink);
  sink_ = nullptr;
}

void VcmCapturer::Destroy() {
  if (!vcm_)
    return;

  vcm_->StopCapture();
  vcm_->DeRegisterCaptureDataCallback();
  // Release reference to VCM.
  vcm_ = nullptr;
}

VcmCapturer::~VcmCapturer() { Destroy(); }

void VcmCapturer::OnFrame(const VideoFrame& frame) {
  rtc::CritScope lock(&crit_);
  if (started_ && sink_)
    sink_->OnFrame(frame);
}

}  // test
}  // webrtc
