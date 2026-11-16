/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include "api/video/resolution.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/y4m_header_parser.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kFrameHeaderSize = 6;  // "FRAME\n"
}  // namespace

Y4mFrameReaderImpl::Y4mFrameReaderImpl(std::string filepath,
                                       RepeatMode repeat_mode)
    : YuvFrameReaderImpl(filepath, Resolution(), repeat_mode) {}

void Y4mFrameReaderImpl::Init() {
  file_ = fopen(filepath_.c_str(), "rb");
  RTC_CHECK(file_ != nullptr) << "Cannot open " << filepath_;

  std::optional<Y4mHeader> header = ParseY4mHeaderFromFile(filepath_);
  RTC_CHECK(header.has_value()) << "Cannot parse Y4M header in " << filepath_;
  resolution_ = header->resolution;
  header_size_bytes_ = static_cast<int>(header->header_size.bytes());

  frame_size_bytes_ =
      CalcBufferSize(VideoType::kI420, resolution_.width, resolution_.height);
  frame_size_bytes_ += kFrameHeaderSize;

  size_t file_size_bytes = GetFileSize(filepath_);
  RTC_CHECK_GT(file_size_bytes, 0u) << "File " << filepath_ << " is empty";
  RTC_CHECK_GT(file_size_bytes, header_size_bytes_)
      << "File " << filepath_ << " is too small";

  num_frames_ = static_cast<int>((file_size_bytes - header_size_bytes_) /
                                 frame_size_bytes_);
  RTC_CHECK_GT(num_frames_, 0u) << "File " << filepath_ << " is too small";
  header_size_bytes_ += kFrameHeaderSize;
}

std::unique_ptr<FrameReader> CreateY4mFrameReader(std::string filepath) {
  return CreateY4mFrameReader(filepath,
                              YuvFrameReaderImpl::RepeatMode::kSingle);
}

std::unique_ptr<FrameReader> CreateY4mFrameReader(
    std::string filepath,
    YuvFrameReaderImpl::RepeatMode repeat_mode) {
  Y4mFrameReaderImpl* frame_reader =
      new Y4mFrameReaderImpl(filepath, repeat_mode);
  frame_reader->Init();
  return std::unique_ptr<FrameReader>(frame_reader);
}

}  // namespace test
}  // namespace webrtc
