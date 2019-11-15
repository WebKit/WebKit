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

#include <algorithm>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

void VideoFrame::UpdateRect::Union(const UpdateRect& other) {
  if (other.IsEmpty())
    return;
  if (IsEmpty()) {
    *this = other;
    return;
  }
  int right = std::max(offset_x + width, other.offset_x + other.width);
  int bottom = std::max(offset_y + height, other.offset_y + other.height);
  offset_x = std::min(offset_x, other.offset_x);
  offset_y = std::min(offset_y, other.offset_y);
  width = right - offset_x;
  height = bottom - offset_y;
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
}

void VideoFrame::UpdateRect::Intersect(const UpdateRect& other) {
  if (other.IsEmpty() || IsEmpty()) {
    MakeEmptyUpdate();
    return;
  }

  int right = std::min(offset_x + width, other.offset_x + other.width);
  int bottom = std::min(offset_y + height, other.offset_y + other.height);
  offset_x = std::max(offset_x, other.offset_x);
  offset_y = std::max(offset_y, other.offset_y);
  width = right - offset_x;
  height = bottom - offset_y;
  if (width <= 0 || height <= 0) {
    MakeEmptyUpdate();
  }
}

void VideoFrame::UpdateRect::MakeEmptyUpdate() {
  width = height = offset_x = offset_y = 0;
}

bool VideoFrame::UpdateRect::IsEmpty() const {
  return width == 0 && height == 0;
}

VideoFrame::Builder::Builder() = default;

VideoFrame::Builder::~Builder() = default;

VideoFrame VideoFrame::Builder::build() {
  RTC_CHECK(video_frame_buffer_ != nullptr);
  return VideoFrame(id_, video_frame_buffer_, timestamp_us_, timestamp_rtp_,
                    ntp_time_ms_, rotation_, color_space_, update_rect_,
                    packet_infos_);
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
    const absl::optional<ColorSpace>& color_space) {
  color_space_ = color_space;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_color_space(
    const ColorSpace* color_space) {
  color_space_ =
      color_space ? absl::make_optional(*color_space) : absl::nullopt;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_id(uint16_t id) {
  id_ = id;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_update_rect(
    const VideoFrame::UpdateRect& update_rect) {
  update_rect_ = update_rect;
  return *this;
}

VideoFrame::Builder& VideoFrame::Builder::set_packet_infos(
    RtpPacketInfos packet_infos) {
  packet_infos_ = std::move(packet_infos);
  return *this;
}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       webrtc::VideoRotation rotation,
                       int64_t timestamp_us)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(0),
      ntp_time_ms_(0),
      timestamp_us_(timestamp_us),
      rotation_(rotation),
      update_rect_{0, 0, buffer->width(), buffer->height()} {}

VideoFrame::VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       uint32_t timestamp_rtp,
                       int64_t render_time_ms,
                       VideoRotation rotation)
    : video_frame_buffer_(buffer),
      timestamp_rtp_(timestamp_rtp),
      ntp_time_ms_(0),
      timestamp_us_(render_time_ms * rtc::kNumMicrosecsPerMillisec),
      rotation_(rotation),
      update_rect_{0, 0, buffer->width(), buffer->height()} {
  RTC_DCHECK(buffer);
}

VideoFrame::VideoFrame(uint16_t id,
                       const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
                       int64_t timestamp_us,
                       uint32_t timestamp_rtp,
                       int64_t ntp_time_ms,
                       VideoRotation rotation,
                       const absl::optional<ColorSpace>& color_space,
                       const absl::optional<UpdateRect>& update_rect,
                       RtpPacketInfos packet_infos)
    : id_(id),
      video_frame_buffer_(buffer),
      timestamp_rtp_(timestamp_rtp),
      ntp_time_ms_(ntp_time_ms),
      timestamp_us_(timestamp_us),
      rotation_(rotation),
      color_space_(color_space),
      update_rect_(update_rect.value_or(UpdateRect{
          0, 0, video_frame_buffer_->width(), video_frame_buffer_->height()})),
      packet_infos_(std::move(packet_infos)) {
  RTC_DCHECK_GE(update_rect_.offset_x, 0);
  RTC_DCHECK_GE(update_rect_.offset_y, 0);
  RTC_DCHECK_LE(update_rect_.offset_x + update_rect_.width, width());
  RTC_DCHECK_LE(update_rect_.offset_y + update_rect_.height, height());
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

void VideoFrame::set_video_frame_buffer(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
  RTC_CHECK(buffer);
  video_frame_buffer_ = buffer;
}

int64_t VideoFrame::render_time_ms() const {
  return timestamp_us() / rtc::kNumMicrosecsPerMillisec;
}

}  // namespace webrtc
