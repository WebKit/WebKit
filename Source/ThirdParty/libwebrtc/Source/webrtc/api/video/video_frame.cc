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

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       webrtc::VideoRotation rotation,
                       int64_t timestamp_us)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(0),
      ntp_time_ms_(0),
      timestamp_us_(timestamp_us),
      rotation_(rotation) {}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       uint32_t timestamp,
                       int64_t render_time_ms,
                       VideoRotation rotation)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(timestamp),
      ntp_time_ms_(0),
      timestamp_us_(render_time_ms * rtc::kNumMicrosecsPerMillisec),
      rotation_(rotation) {
  RTC_DCHECK(buffer);
}

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
