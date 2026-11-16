/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/y4m_frame_generator.h"

#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/checks.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/y4m_header_parser.h"

namespace webrtc {
namespace test {

Y4mFrameGenerator::Y4mFrameGenerator(absl::string_view filename,
                                     RepeatMode repeat_mode)
    : filename_(filename), repeat_mode_(repeat_mode) {
  std::optional<Y4mHeader> header = ParseY4mHeaderFromFile(filename_);
  RTC_CHECK(header.has_value()) << "Cannot parse Y4M header in " << filename_;
  RTC_CHECK(header->framerate.has_value())
      << "Framerate not specified in Y4M header in " << filename_;

  width_ = static_cast<size_t>(header->resolution.width);
  height_ = static_cast<size_t>(header->resolution.height);
  // Truncate to integer fps to match legacy behavior (e.g. 24000/1001 -> 23
  // fps).
  fps_ = static_cast<int>(header->framerate->millihertz() / 1000);

  // Delegate the actual reads (from NextFrame) to a Y4mReader.
  frame_reader_ = test::CreateY4mFrameReader(
      filename_, ToYuvFrameReaderRepeatMode(repeat_mode_));
}

Y4mFrameGenerator::VideoFrameData Y4mFrameGenerator::NextFrame() {
  VideoFrame::UpdateRect update_rect{.offset_x = 0,
                                     .offset_y = 0,
                                     .width = static_cast<int>(width_),
                                     .height = static_cast<int>(height_)};
  scoped_refptr<I420Buffer> next_frame_buffer = frame_reader_->PullFrame();

  if (!next_frame_buffer ||
      (static_cast<size_t>(next_frame_buffer->width()) == width_ &&
       static_cast<size_t>(next_frame_buffer->height()) == height_)) {
    return VideoFrameData(next_frame_buffer, update_rect);
  }

  // Allocate a new buffer and return scaled version.
  scoped_refptr<I420Buffer> scaled_buffer(I420Buffer::Create(width_, height_));
  I420Buffer::SetBlack(scaled_buffer.get());
  scaled_buffer->ScaleFrom(*next_frame_buffer->ToI420());
  return VideoFrameData(scaled_buffer, update_rect);
}

void Y4mFrameGenerator::SkipNextFrame() {
  frame_reader_->PullFrame();
}

void Y4mFrameGenerator::ChangeResolution(size_t width, size_t height) {
  width_ = width;
  height_ = height;
  RTC_CHECK_GT(width_, 0);
  RTC_CHECK_GT(height_, 0);
}

FrameGeneratorInterface::Resolution Y4mFrameGenerator::GetResolution() const {
  return {.width = width_, .height = height_};
}

YuvFrameReaderImpl::RepeatMode Y4mFrameGenerator::ToYuvFrameReaderRepeatMode(
    RepeatMode repeat_mode) const {
  switch (repeat_mode) {
    case RepeatMode::kSingle:
      return YuvFrameReaderImpl::RepeatMode::kSingle;
    case RepeatMode::kLoop:
      return YuvFrameReaderImpl::RepeatMode::kRepeat;
    case RepeatMode::kPingPong:
      return YuvFrameReaderImpl::RepeatMode::kPingPong;
  }
}

}  // namespace test
}  // namespace webrtc
