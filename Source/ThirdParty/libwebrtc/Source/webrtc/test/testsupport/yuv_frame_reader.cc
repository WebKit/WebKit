/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "test/frame_utils.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
namespace {
using RepeatMode = YuvFrameReaderImpl::RepeatMode;

int WrapFrameNum(int frame_num, int num_frames, RepeatMode mode) {
  RTC_CHECK_GE(frame_num, 0) << "frame_num cannot be negative";
  RTC_CHECK_GT(num_frames, 0) << "num_frames must be greater than 0";
  if (mode == RepeatMode::kSingle) {
    return frame_num;
  }
  if (mode == RepeatMode::kRepeat) {
    return frame_num % num_frames;
  }

  RTC_CHECK_EQ(RepeatMode::kPingPong, mode);
  int cycle_len = std::max(1, 2 * (num_frames - 1));
  int wrapped_num = frame_num % cycle_len;
  if (wrapped_num >= num_frames) {
    return cycle_len - wrapped_num;
  }
  return wrapped_num;
}

scoped_refptr<I420Buffer> Scale(scoped_refptr<I420Buffer> buffer,
                                Resolution resolution) {
  if (buffer->width() == resolution.width &&
      buffer->height() == resolution.height) {
    return buffer;
  }
  scoped_refptr<I420Buffer> scaled(
      I420Buffer::Create(resolution.width, resolution.height));
  scaled->ScaleFrom(*buffer);
  return scaled;
}
}  // namespace

int YuvFrameReaderImpl::RateScaler::Skip(Ratio framerate_scale) {
  ticks_ = ticks_.value_or(framerate_scale.num);
  int skip = 0;
  while (ticks_ <= 0) {
    *ticks_ += framerate_scale.num;
    ++skip;
  }
  *ticks_ -= framerate_scale.den;
  return skip;
}

YuvFrameReaderImpl::YuvFrameReaderImpl(std::string filepath,
                                       Resolution resolution,
                                       RepeatMode repeat_mode)
    : filepath_(filepath),
      resolution_(resolution),
      repeat_mode_(repeat_mode),
      num_frames_(0),
      frame_num_(0),
      frame_size_bytes_(0),
      header_size_bytes_(0),
      file_(nullptr) {}

YuvFrameReaderImpl::~YuvFrameReaderImpl() {
  if (file_ != nullptr) {
    fclose(file_);
    file_ = nullptr;
  }
}

void YuvFrameReaderImpl::Init() {
  RTC_CHECK_GT(resolution_.width, 0) << "Width must be positive";
  RTC_CHECK_GT(resolution_.height, 0) << "Height must be positive";
  frame_size_bytes_ =
      CalcBufferSize(VideoType::kI420, resolution_.width, resolution_.height);

  file_ = fopen(filepath_.c_str(), "rb");
  RTC_CHECK(file_ != nullptr) << "Cannot open " << filepath_;

  size_t file_size_bytes = GetFileSize(filepath_);
  RTC_CHECK_GT(file_size_bytes, 0u) << "File " << filepath_ << " is empty";

  num_frames_ = static_cast<int>(file_size_bytes / frame_size_bytes_);
  RTC_CHECK_GT(num_frames_, 0u) << "File " << filepath_ << " is too small";
}

scoped_refptr<I420Buffer> YuvFrameReaderImpl::PullFrame() {
  return PullFrame(/*frame_num=*/nullptr);
}

scoped_refptr<I420Buffer> YuvFrameReaderImpl::PullFrame(int* frame_num) {
  return PullFrame(frame_num, resolution_, /*framerate_scale=*/kNoScale);
}

scoped_refptr<I420Buffer> YuvFrameReaderImpl::PullFrame(int* frame_num,
                                                        Resolution resolution,
                                                        Ratio framerate_scale) {
  frame_num_ += framerate_scaler_.Skip(framerate_scale);
  auto buffer = ReadFrame(frame_num_, resolution);
  if (frame_num != nullptr) {
    *frame_num = frame_num_;
  }
  return buffer;
}

scoped_refptr<I420Buffer> YuvFrameReaderImpl::ReadFrame(int frame_num) {
  return ReadFrame(frame_num, resolution_);
}

scoped_refptr<I420Buffer> YuvFrameReaderImpl::ReadFrame(int frame_num,
                                                        Resolution resolution) {
  int wrapped_num = WrapFrameNum(frame_num, num_frames_, repeat_mode_);
  if (wrapped_num >= num_frames_) {
    RTC_CHECK_EQ(RepeatMode::kSingle, repeat_mode_);
    return nullptr;
  }
  fseek(file_, header_size_bytes_ + wrapped_num * frame_size_bytes_, SEEK_SET);
  auto buffer = ReadI420Buffer(resolution_.width, resolution_.height, file_);
  RTC_CHECK(buffer != nullptr);

  return Scale(buffer, resolution);
}

std::unique_ptr<FrameReader> CreateYuvFrameReader(std::string filepath,
                                                  Resolution resolution) {
  return CreateYuvFrameReader(filepath, resolution,
                              YuvFrameReaderImpl::RepeatMode::kSingle);
}

std::unique_ptr<FrameReader> CreateYuvFrameReader(
    std::string filepath,
    Resolution resolution,
    YuvFrameReaderImpl::RepeatMode repeat_mode) {
  YuvFrameReaderImpl* frame_reader =
      new YuvFrameReaderImpl(filepath, resolution, repeat_mode);
  frame_reader->Init();
  return std::unique_ptr<FrameReader>(frame_reader);
}

std::optional<Resolution> ParseResolutionFromFileName(absl::string_view path) {
  const size_t last_slash = path.find_last_of("/\\");
  const absl::string_view filename = (last_slash == absl::string_view::npos)
                                         ? path
                                         : path.substr(last_slash + 1);

  const std::string lower = absl::AsciiStrToLower(filename);
  if (absl::StrContains(lower, "qcif")) {
    return Resolution{.width = 176, .height = 144};
  }
  if (absl::StrContains(lower, "cif")) {
    return Resolution{.width = 352, .height = 288};
  }
  if (absl::StrContains(lower, "qvga")) {
    return Resolution{.width = 320, .height = 240};
  }
  if (absl::StrContains(lower, "vga")) {
    return Resolution{.width = 640, .height = 480};
  }
  if (absl::StrContains(lower, "180p")) {
    return Resolution{.width = 320, .height = 180};
  }
  if (absl::StrContains(lower, "360p")) {
    return Resolution{.width = 640, .height = 360};
  }
  if (absl::StrContains(lower, "480p")) {
    return Resolution{.width = 640, .height = 480};
  }
  if (absl::StrContains(lower, "720p")) {
    return Resolution{.width = 1280, .height = 720};
  }
  if (absl::StrContains(lower, "1080p")) {
    return Resolution{.width = 1920, .height = 1080};
  }
  if (absl::StrContains(lower, "1440p")) {
    return Resolution{.width = 2560, .height = 1440};
  }
  if (absl::StrContains(lower, "2160p") || absl::StrContains(lower, "4k")) {
    return Resolution{.width = 3840, .height = 2160};
  }

  // 1. Look for <width>x<height> pattern (e.g. "1280x720", "128x96").
  for (size_t i = 0; i < filename.size(); ++i) {
    if ((filename[i] == 'x' || filename[i] == 'X') && i > 0 &&
        absl::ascii_isdigit(filename[i - 1]) && i + 1 < filename.size() &&
        absl::ascii_isdigit(filename[i + 1])) {
      size_t start_w = i - 1;
      while (start_w > 0 && absl::ascii_isdigit(filename[start_w - 1])) {
        --start_w;
      }
      if (start_w > 0 && absl::ascii_isalnum(filename[start_w - 1])) {
        continue;
      }
      size_t end_h = i + 1;
      while (end_h < filename.size() && absl::ascii_isdigit(filename[end_h])) {
        ++end_h;
      }
      if (end_h < filename.size() && absl::ascii_isalnum(filename[end_h])) {
        continue;
      }
      int width = 0;
      for (size_t k = start_w; k < i; ++k) {
        width = width * 10 + (filename[k] - '0');
      }
      int height = 0;
      for (size_t k = i + 1; k < end_h; ++k) {
        height = height * 10 + (filename[k] - '0');
      }
      if (width >= 16 && height >= 16) {
        return Resolution{.width = width, .height = height};
      }
    }
  }

  // 2. Look for <width>_<height> pattern (e.g. "1280_720", "1850_1110").
  for (size_t i = 0; i < filename.size(); ++i) {
    if (filename[i] == '_' && i > 0 && absl::ascii_isdigit(filename[i - 1]) &&
        i + 1 < filename.size() && absl::ascii_isdigit(filename[i + 1])) {
      size_t start_w = i - 1;
      while (start_w > 0 && absl::ascii_isdigit(filename[start_w - 1])) {
        --start_w;
      }
      if (start_w > 0 && absl::ascii_isalnum(filename[start_w - 1])) {
        continue;
      }
      size_t end_h = i + 1;
      while (end_h < filename.size() && absl::ascii_isdigit(filename[end_h])) {
        ++end_h;
      }
      if (end_h < filename.size() && absl::ascii_isalnum(filename[end_h])) {
        continue;
      }
      int width = 0;
      for (size_t k = start_w; k < i; ++k) {
        width = width * 10 + (filename[k] - '0');
      }
      int height = 0;
      for (size_t k = i + 1; k < end_h; ++k) {
        height = height * 10 + (filename[k] - '0');
      }
      if (width >= 64 && height >= 64) {
        return Resolution{.width = width, .height = height};
      }
    }
  }

  return std::nullopt;
}

}  // namespace test
}  // namespace webrtc
