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

#include <stdio.h>
#include <string.h>

#include <string>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
// Reading 30 bytes from the Y4M header should be enough to get width
// and heigth.
// The header starts with: `YUV4MPEG2 W<WIDTH> H<HEIGTH>`.
constexpr int kHeaderBytesToRead = 30;
}  // namespace

Y4mFrameGenerator::Y4mFrameGenerator(absl::string_view filename,
                                     RepeatMode repeat_mode)
    : filename_(filename), repeat_mode_(repeat_mode) {
  // Read resolution from the Y4M header.
  FILE* file = fopen(filename_.c_str(), "r");
  RTC_CHECK(file != NULL) << "Cannot open " << filename_;
  char header[kHeaderBytesToRead];
  RTC_CHECK(fgets(header, sizeof(header), file) != nullptr)
      << "File " << filename_ << " is too small";
  fclose(file);
  int fps_denominator;
  RTC_CHECK_EQ(sscanf(header, "YUV4MPEG2 W%zu H%zu F%i:%i", &width_, &height_,
                      &fps_, &fps_denominator),
               4);
  fps_ /= fps_denominator;
  RTC_CHECK_GT(width_, 0);
  RTC_CHECK_GT(height_, 0);

  // Delegate the actual reads (from NextFrame) to a Y4mReader.
  frame_reader_ = webrtc::test::CreateY4mFrameReader(
      filename_, ToYuvFrameReaderRepeatMode(repeat_mode_));
}

Y4mFrameGenerator::VideoFrameData Y4mFrameGenerator::NextFrame() {
  webrtc::VideoFrame::UpdateRect update_rect{0, 0, static_cast<int>(width_),
                                             static_cast<int>(height_)};
  rtc::scoped_refptr<webrtc::I420Buffer> next_frame_buffer =
      frame_reader_->PullFrame();

  if (!next_frame_buffer ||
      (static_cast<size_t>(next_frame_buffer->width()) == width_ &&
       static_cast<size_t>(next_frame_buffer->height()) == height_)) {
    return VideoFrameData(next_frame_buffer, update_rect);
  }

  // Allocate a new buffer and return scaled version.
  rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer(
      I420Buffer::Create(width_, height_));
  webrtc::I420Buffer::SetBlack(scaled_buffer.get());
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
