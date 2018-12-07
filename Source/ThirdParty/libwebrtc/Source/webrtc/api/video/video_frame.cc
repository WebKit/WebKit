/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame.h"

#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

VideoFrame::Builder::Builder() = default;

VideoFrame::Builder::~Builder() = default;

VideoFrame VideoFrame::Builder::build() {
  return VideoFrame(video_frame_buffer_, timestamp_us_, timestamp_rtp_,
                    ntp_time_ms_, rotation_, color_space_);
}

VideoFrame::Builder& VideoFrame::Builder::set_video_frame_buffer(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
  video_frame_buffer_ = buffer;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_timestamp_ms(
    int64_t timestamp_ms) {
  timestamp_us_ = timestamp_ms * rtc::kNumMicrosecsPerMillisec;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_timestamp_us(
    int64_t timestamp_us) {
  timestamp_us_ = timestamp_us;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_timestamp_rtp(
    uint32_t timestamp_rtp) {
  timestamp_rtp_ = timestamp_rtp;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_ntp_time_ms(int64_t ntp_time_ms) {
  ntp_time_ms_ = ntp_time_ms;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_rotation(VideoRotation rotation) {
  rotation_ = rotation;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_color_space(
    const ColorSpace& color_space) {
  color_space_ = color_space;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_color_space(
    const ColorSpace* color_space) {
  color_space_ =
      color_space ? absl::make_optional(*color_space) : absl::nullopt;
  return *this;
}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       webrtc::VideoRotation rotation,
                       int64_t timestamp_us)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(0),
      ntp_time_ms_(0),
      timestamp_us_(timestamp_us),
      rotation_(rotation) {}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       uint32_t timestamp_rtp,
                       int64_t render_time_ms,
                       VideoRotation rotation)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(timestamp_rtp),
      ntp_time_ms_(0),
      timestamp_us_(render_time_ms * rtc::kNumMicrosecsPerMillisec),
      rotation_(rotation) {
  RTC_DCHECK(buffer);
}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       int64_t timestamp_us,
                       uint32_t timestamp_rtp,
                       int64_t ntp_time_ms,
                       VideoRotation rotation,
                       const absl::optional<ColorSpace>& color_space)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(timestamp_rtp),
      ntp_time_ms_(ntp_time_ms),
      timestamp_us_(timestamp_us),
      rotation_(rotation),
      color_space_(color_space) {}

VideoFrame::~VideoFrame() = default;

VideoFrame::VideoFrame(const VideoFrame&) = default;
VideoFrame::VideoFrame(VideoFrame&&) = default;
VideoFrame& VideoFrame::operator=(const VideoFrame&) = default;
VideoFrame& VideoFrame::operator=(VideoFrame&&) = default;

int VideoFrame::width() const {
  return video_frame_buffer_ ? video_frame_buffer_->width() : 0;
}

int VideoFrame::height() const {
  return video_frame_buffer_ ? video_frame_buffer_->height() : 0;
}

uint32_t VideoFrame::size() const {
  return width() * height();
}

rtc::scoped_refptr<VideoFrameBuffer> VideoFrame::video_frame_buffer() const {
  return video_frame_buffer_;
}

int64_t VideoFrame::render_time_ms() const {
  return timestamp_us() / rtc::kNumMicrosecsPerMillisec;
}

}  // namespace webrtc
