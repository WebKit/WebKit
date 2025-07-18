/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/fake_frame_source.h"

#include <cstdint>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

FakeFrameSource::FakeFrameSource(int width,
                                 int height,
                                 int interval_us,
                                 int64_t timestamp_offset_us)
    : width_(width),
      height_(height),
      interval_us_(interval_us),
      next_timestamp_us_(timestamp_offset_us) {
  RTC_CHECK_GT(width_, 0);
  RTC_CHECK_GT(height_, 0);
  RTC_CHECK_GT(interval_us_, 0);
  RTC_CHECK_GE(next_timestamp_us_, 0);
}

FakeFrameSource::FakeFrameSource(int width, int height, int interval_us)
    : FakeFrameSource(width, height, interval_us, TimeMicros()) {}

VideoRotation FakeFrameSource::GetRotation() const {
  return rotation_;
}

void FakeFrameSource::SetRotation(VideoRotation rotation) {
  rotation_ = rotation;
}

VideoFrame FakeFrameSource::GetFrameRotationApplied() {
  switch (rotation_) {
    case kVideoRotation_0:
    case kVideoRotation_180:
      return GetFrame(width_, height_, kVideoRotation_0, interval_us_);
    case kVideoRotation_90:
    case kVideoRotation_270:
      return GetFrame(height_, width_, kVideoRotation_0, interval_us_);
  }
  RTC_DCHECK_NOTREACHED() << "Invalid rotation value: "
                          << static_cast<int>(rotation_);
  // Without this return, the Windows Visual Studio compiler complains
  // "not all control paths return a value".
  return GetFrame();
}

VideoFrame FakeFrameSource::GetFrame() {
  return GetFrame(width_, height_, rotation_, interval_us_);
}

VideoFrame FakeFrameSource::GetFrame(int width,
                                     int height,
                                     VideoRotation rotation,
                                     int interval_us) {
  RTC_CHECK_GT(width, 0);
  RTC_CHECK_GT(height, 0);
  RTC_CHECK_GT(interval_us, 0);

  scoped_refptr<I420Buffer> buffer(I420Buffer::Create(width, height));

  buffer->InitializeData();
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(buffer)
                         .set_rotation(rotation)
                         .set_timestamp_us(next_timestamp_us_)
                         .build();

  next_timestamp_us_ += interval_us;
  return frame;
}

}  // namespace webrtc
