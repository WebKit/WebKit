/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/fakevideocapturer.h"

#include "rtc_base/arraysize.h"
#include "rtc_base/timeutils.h"

namespace cricket {

FakeVideoCapturer::FakeVideoCapturer(bool is_screencast)
    : running_(false),
      is_screencast_(is_screencast),
      rotation_(webrtc::kVideoRotation_0) {
  // Default supported formats. Use ResetSupportedFormats to over write.
  using cricket::VideoFormat;
  static const VideoFormat formats[] = {
      {1280, 720, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {640, 480, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {320, 240, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {160, 120, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {1280, 720, VideoFormat::FpsToInterval(60), cricket::FOURCC_I420},
  };
  ResetSupportedFormats({&formats[0], &formats[arraysize(formats)]});
}

FakeVideoCapturer::FakeVideoCapturer() : FakeVideoCapturer(false) {}

FakeVideoCapturer::~FakeVideoCapturer() {
  SignalDestroyed(this);
}

void FakeVideoCapturer::ResetSupportedFormats(
    const std::vector<cricket::VideoFormat>& formats) {
  SetSupportedFormats(formats);
}

bool FakeVideoCapturer::CaptureFrame() {
  if (!GetCaptureFormat()) {
    return false;
  }
  RTC_CHECK_EQ(GetCaptureFormat()->fourcc, FOURCC_I420);
  return CaptureFrame(frame_source_->GetFrame());
}

bool FakeVideoCapturer::CaptureCustomFrame(int width, int height) {
  // Default to 30fps.
  // TODO(nisse): Would anything break if we always stick to
  // the configure frame interval?
  return CaptureFrame(frame_source_->GetFrame(width, height, rotation_,
                                              rtc::kNumMicrosecsPerSec / 30));
}

bool FakeVideoCapturer::CaptureFrame(const webrtc::VideoFrame& frame) {
  if (!running_) {
    return false;
  }

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  // TODO(nisse): It's a bit silly to have this logic in a fake
  // class. Child classes of VideoCapturer are expected to call
  // AdaptFrame, and the test case
  // VideoCapturerTest.SinkWantsMaxPixelAndMaxPixelCountStepUp
  // depends on this.
  if (AdaptFrame(frame.width(), frame.height(), frame.timestamp_us(),
                 frame.timestamp_us(), &adapted_width, &adapted_height,
                 &crop_width, &crop_height, &crop_x, &crop_y, nullptr)) {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer(
        webrtc::I420Buffer::Create(adapted_width, adapted_height));
    buffer->InitializeData();

    OnFrame(webrtc::VideoFrame(buffer, frame.rotation(), frame.timestamp_us()),
            frame.width(), frame.height());
  }

  return true;
}

cricket::CaptureState FakeVideoCapturer::Start(
    const cricket::VideoFormat& format) {
  SetCaptureFormat(&format);
  running_ = true;
  SetCaptureState(cricket::CS_RUNNING);
  frame_source_ = absl::make_unique<FakeFrameSource>(
      format.width, format.height,
      format.interval / rtc::kNumNanosecsPerMicrosec, rtc::TimeMicros());
  frame_source_->SetRotation(rotation_);
  return cricket::CS_RUNNING;
}

void FakeVideoCapturer::Stop() {
  running_ = false;
  SetCaptureFormat(NULL);
  SetCaptureState(cricket::CS_STOPPED);
}

bool FakeVideoCapturer::IsRunning() {
  return running_;
}

bool FakeVideoCapturer::IsScreencast() const {
  return is_screencast_;
}

bool FakeVideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
  fourccs->push_back(cricket::FOURCC_I420);
  fourccs->push_back(cricket::FOURCC_MJPG);
  return true;
}

void FakeVideoCapturer::SetRotation(webrtc::VideoRotation rotation) {
  rotation_ = rotation;
  if (frame_source_)
    frame_source_->SetRotation(rotation_);
}

webrtc::VideoRotation FakeVideoCapturer::GetRotation() {
  return rotation_;
}

FakeVideoCapturerWithTaskQueue::FakeVideoCapturerWithTaskQueue(
    bool is_screencast)
    : FakeVideoCapturer(is_screencast) {}

FakeVideoCapturerWithTaskQueue::FakeVideoCapturerWithTaskQueue() {}

bool FakeVideoCapturerWithTaskQueue::CaptureFrame() {
  if (task_queue_.IsCurrent())
    return FakeVideoCapturer::CaptureFrame();
  bool ret = false;
  task_queue_.SendTask(
      [this, &ret]() { ret = FakeVideoCapturer::CaptureFrame(); });
  return ret;
}

bool FakeVideoCapturerWithTaskQueue::CaptureCustomFrame(int width, int height) {
  if (task_queue_.IsCurrent())
    return FakeVideoCapturer::CaptureCustomFrame(width, height);
  bool ret = false;
  task_queue_.SendTask([this, &ret, width, height]() {
    ret = FakeVideoCapturer::CaptureCustomFrame(width, height);
  });
  return ret;
}

}  // namespace cricket
