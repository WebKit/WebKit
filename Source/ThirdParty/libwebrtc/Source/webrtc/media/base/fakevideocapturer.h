/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
#define WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_

#include <string.h>

#include <memory>
#include <vector>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"

namespace cricket {

// Fake video capturer that allows the test to manually pump in frames.
class FakeVideoCapturer : public cricket::VideoCapturer {
 public:
  explicit FakeVideoCapturer(bool is_screencast)
      : running_(false),
        initial_timestamp_(rtc::TimeNanos()),
        next_timestamp_(rtc::kNumNanosecsPerMillisec),
        is_screencast_(is_screencast),
        rotation_(webrtc::kVideoRotation_0) {
    // Default supported formats. Use ResetSupportedFormats to over write.
    std::vector<cricket::VideoFormat> formats;
    formats.push_back(cricket::VideoFormat(1280, 720,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(320, 240,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(160, 120,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(1280, 720,
        cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420));
    ResetSupportedFormats(formats);
  }
  FakeVideoCapturer() : FakeVideoCapturer(false) {}

  ~FakeVideoCapturer() {
    SignalDestroyed(this);
  }

  void ResetSupportedFormats(const std::vector<cricket::VideoFormat>& formats) {
    SetSupportedFormats(formats);
  }
  bool CaptureFrame() {
    if (!GetCaptureFormat()) {
      return false;
    }
    return CaptureCustomFrame(GetCaptureFormat()->width,
                              GetCaptureFormat()->height,
                              GetCaptureFormat()->interval,
                              GetCaptureFormat()->fourcc);
  }
  bool CaptureCustomFrame(int width, int height, uint32_t fourcc) {
    // default to 30fps
    return CaptureCustomFrame(width, height, 33333333, fourcc);
  }
  bool CaptureCustomFrame(int width,
                          int height,
                          int64_t timestamp_interval,
                          uint32_t fourcc) {
    if (!running_) {
      return false;
    }
    RTC_CHECK(fourcc == FOURCC_I420);
    RTC_CHECK(width > 0);
    RTC_CHECK(height > 0);

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
    if (AdaptFrame(width, height, 0, 0, &adapted_width, &adapted_height,
                   &crop_width, &crop_height, &crop_x, &crop_y, nullptr)) {
      rtc::scoped_refptr<webrtc::I420Buffer> buffer(
          webrtc::I420Buffer::Create(adapted_width, adapted_height));
      buffer->InitializeData();

      OnFrame(webrtc::VideoFrame(
                  buffer, rotation_,
                  next_timestamp_ / rtc::kNumNanosecsPerMicrosec),
              width, height);
    }
    next_timestamp_ += timestamp_interval;

    return true;
  }

  sigslot::signal1<FakeVideoCapturer*> SignalDestroyed;

  cricket::CaptureState Start(const cricket::VideoFormat& format) override {
    SetCaptureFormat(&format);
    running_ = true;
    SetCaptureState(cricket::CS_RUNNING);
    return cricket::CS_RUNNING;
  }
  void Stop() override {
    running_ = false;
    SetCaptureFormat(NULL);
    SetCaptureState(cricket::CS_STOPPED);
  }
  bool IsRunning() override { return running_; }
  bool IsScreencast() const override { return is_screencast_; }
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override {
    fourccs->push_back(cricket::FOURCC_I420);
    fourccs->push_back(cricket::FOURCC_MJPG);
    return true;
  }

  void SetRotation(webrtc::VideoRotation rotation) {
    rotation_ = rotation;
  }

  webrtc::VideoRotation GetRotation() { return rotation_; }

 private:
  bool running_;
  int64_t initial_timestamp_;
  int64_t next_timestamp_;
  const bool is_screencast_;
  webrtc::VideoRotation rotation_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
