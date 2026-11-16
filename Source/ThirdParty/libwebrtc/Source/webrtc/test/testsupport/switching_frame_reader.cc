/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/switching_frame_reader.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {

std::string ResolvePath(std::string path) {
  if (absl::StartsWith(path, "res://")) {
    path = path.substr(6);
  }
  if (test::FileExists(path)) {
    return path;
  }
  std::string name = path;
  size_t dot_pos = path.rfind('.');
  if (dot_pos != std::string::npos) {
    name = path.substr(0, dot_pos);
    std::string ext = path.substr(dot_pos + 1);
    std::string res_path = test::ResourcePath(name, ext);
    if (test::FileExists(res_path)) {
      return res_path;
    }
  } else {
    std::string yuv_path = test::ResourcePath(name, "yuv");
    if (test::FileExists(yuv_path)) {
      return yuv_path;
    }
    std::string y4m_path = test::ResourcePath(name, "y4m");
    if (test::FileExists(y4m_path)) {
      return y4m_path;
    }
  }
  return path;
}

std::vector<std::unique_ptr<FrameReader>> CreateReaders(
    const std::vector<std::string>& file_paths,
    Resolution target_resolution,
    YuvFrameReaderImpl::RepeatMode repeat_mode) {
  RTC_CHECK(!file_paths.empty()) << "File paths cannot be empty";
  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.reserve(file_paths.size());
  for (const std::string& raw_path : file_paths) {
    std::string path = ResolvePath(raw_path);
    std::unique_ptr<FrameReader> reader;
    if (absl::EndsWith(path, ".y4m")) {
      reader = CreateY4mFrameReader(path, repeat_mode);
    } else {
      Resolution file_res =
          ParseResolutionFromFileName(path).value_or(target_resolution);
      reader = CreateYuvFrameReader(path, file_res, repeat_mode);
    }
    RTC_CHECK(reader != nullptr) << "Failed to open video file: " << path;
    readers.push_back(std::move(reader));
  }
  return readers;
}

}  // namespace

int SwitchingFrameReader::RateScaler::Skip(Ratio framerate_scale) {
  ticks_ = ticks_.value_or(framerate_scale.num);
  int skip = 0;
  while (ticks_ <= 0) {
    *ticks_ += framerate_scale.num;
    ++skip;
  }
  *ticks_ -= framerate_scale.den;
  return skip;
}

SwitchingFrameReader::SwitchingFrameReader(
    std::vector<std::string> file_paths,
    Resolution target_resolution,
    int fps,
    TimeDelta camera_switching_interval,
    YuvFrameReaderImpl::RepeatMode repeat_mode)
    : SwitchingFrameReader(
          CreateReaders(file_paths, target_resolution, repeat_mode),
          target_resolution,
          fps,
          camera_switching_interval) {}

SwitchingFrameReader::SwitchingFrameReader(
    std::vector<std::unique_ptr<FrameReader>> readers,
    Resolution target_resolution,
    int fps,
    TimeDelta camera_switching_interval)
    : readers_(std::move(readers)),
      target_resolution_(target_resolution),
      fps_(fps),
      camera_switching_interval_(camera_switching_interval) {
  RTC_CHECK(!readers_.empty()) << "Readers cannot be empty";
  RTC_CHECK_GT(target_resolution_.width, 0);
  RTC_CHECK_GT(target_resolution_.height, 0);
  RTC_CHECK_GT(fps_, 0);
  RTC_CHECK_GT(camera_switching_interval_.us(), 0);
}

int SwitchingFrameReader::current_reader_index() const {
  return GetFrameLocation(frame_num_).reader_index;
}

SwitchingFrameReader::FrameLocation SwitchingFrameReader::GetFrameLocation(
    int frame_num) const {
  RTC_CHECK_GE(frame_num, 0);

  int64_t total_us =
      static_cast<int64_t>(fps_) * camera_switching_interval_.us();
  if (total_us % 1'000'000 == 0) {
    int64_t frames_per_interval = total_us / 1'000'000;
    int64_t interval = frame_num / frames_per_interval;
    int reader_index = interval % readers_.size();
    int64_t cycle = interval / readers_.size();
    int64_t sub_frame_num =
        cycle * frames_per_interval + (frame_num % frames_per_interval);
    return {.reader_index = reader_index,
            .sub_frame_num = static_cast<int>(sub_frame_num)};
  }

  if (static_cast<size_t>(frame_num) < frame_location_cache_.size()) {
    return frame_location_cache_[frame_num];
  }

  if (count_per_reader_.empty()) {
    count_per_reader_.assign(readers_.size(), 0);
  }

  int start = static_cast<int>(frame_location_cache_.size());
  for (int i = start; i <= frame_num; ++i) {
    int64_t elapsed_us = static_cast<int64_t>(i) * 1'000'000 / fps_;
    int r = (elapsed_us / camera_switching_interval_.us()) % readers_.size();
    frame_location_cache_.push_back(
        {.reader_index = r, .sub_frame_num = count_per_reader_[r]});
    ++count_per_reader_[r];
  }
  return frame_location_cache_[frame_num];
}

scoped_refptr<I420Buffer> SwitchingFrameReader::Scale(
    scoped_refptr<I420Buffer> buffer,
    Resolution resolution) const {
  if (!buffer) {
    return nullptr;
  }
  if (buffer->width() == resolution.width &&
      buffer->height() == resolution.height) {
    return buffer;
  }
  scoped_refptr<I420Buffer> scaled =
      I420Buffer::Create(resolution.width, resolution.height);
  scaled->ScaleFrom(*buffer);
  return scaled;
}

scoped_refptr<I420Buffer> SwitchingFrameReader::PullFrame() {
  return PullFrame(/*frame_num=*/nullptr);
}

scoped_refptr<I420Buffer> SwitchingFrameReader::PullFrame(int* frame_num) {
  return PullFrame(frame_num, target_resolution_, /*framerate_scale=*/kNoScale);
}

scoped_refptr<I420Buffer> SwitchingFrameReader::PullFrame(
    int* frame_num,
    Resolution resolution,
    Ratio framerate_scale) {
  int skip = framerate_scaler_.Skip(framerate_scale);
  if (!last_frame_) {
    skip = 1;
  }

  if (skip == 0) {
    if (frame_num != nullptr) {
      *frame_num = last_frame_num_;
    }
    if (resolution.width <= 0 || resolution.height <= 0) {
      resolution = target_resolution_;
    }
    return Scale(last_frame_, resolution);
  }

  frame_num_ += (skip - 1);
  scoped_refptr<I420Buffer> buffer = ReadFrame(frame_num_, resolution);
  if (!buffer) {
    return nullptr;
  }
  last_frame_ = buffer;
  last_frame_num_ = frame_num_;
  if (frame_num != nullptr) {
    *frame_num = frame_num_;
  }
  ++frame_num_;
  return buffer;
}

scoped_refptr<I420Buffer> SwitchingFrameReader::ReadFrame(int frame_num) {
  return ReadFrame(frame_num, target_resolution_);
}

scoped_refptr<I420Buffer> SwitchingFrameReader::ReadFrame(
    int frame_num,
    Resolution resolution) {
  if (resolution.width <= 0 || resolution.height <= 0) {
    resolution = target_resolution_;
  }
  FrameLocation loc = GetFrameLocation(frame_num);
  scoped_refptr<I420Buffer> buffer =
      readers_[loc.reader_index]->ReadFrame(loc.sub_frame_num, resolution);
  return Scale(buffer, resolution);
}

int SwitchingFrameReader::num_frames() const {
  int total = 0;
  for (const auto& reader : readers_) {
    total += reader->num_frames();
  }
  return total;
}

std::unique_ptr<FrameReader> CreateSwitchingFrameReader(
    std::vector<std::string> file_paths,
    Resolution target_resolution,
    int fps,
    TimeDelta camera_switching_interval,
    YuvFrameReaderImpl::RepeatMode repeat_mode) {
  return std::make_unique<SwitchingFrameReader>(
      std::move(file_paths), target_resolution, fps, camera_switching_interval,
      repeat_mode);
}

namespace {

class SwitchingFrameGenerator : public FrameGeneratorInterface {
 public:
  SwitchingFrameGenerator(std::unique_ptr<FrameReader> reader,
                          webrtc::Resolution resolution,
                          int fps)
      : reader_(std::move(reader)), resolution_(resolution), fps_(fps) {}

  VideoFrameData NextFrame() override {
    scoped_refptr<I420Buffer> buffer =
        reader_->PullFrame(/*frame_num=*/nullptr, resolution_,
                           /*framerate_scale=*/FrameReader::kNoScale);
    if (!buffer) {
      return VideoFrameData(nullptr, std::nullopt);
    }
    VideoFrame::UpdateRect update_rect{.offset_x = 0,
                                       .offset_y = 0,
                                       .width = buffer->width(),
                                       .height = buffer->height()};
    return VideoFrameData(buffer, update_rect);
  }

  void SkipNextFrame() override { reader_->PullFrame(); }

  void ChangeResolution(size_t width, size_t height) override {
    resolution_ = {.width = static_cast<int>(width),
                   .height = static_cast<int>(height)};
  }

  FrameGeneratorInterface::Resolution GetResolution() const override {
    return {.width = static_cast<size_t>(resolution_.width),
            .height = static_cast<size_t>(resolution_.height)};
  }

  std::optional<int> fps() const override { return fps_; }

 private:
  const std::unique_ptr<FrameReader> reader_;
  webrtc::Resolution resolution_;
  const int fps_;
};

}  // namespace

std::unique_ptr<FrameGeneratorInterface> CreateSwitchingFrameGenerator(
    std::vector<std::string> file_paths,
    Resolution target_resolution,
    int fps,
    TimeDelta camera_switching_interval,
    YuvFrameReaderImpl::RepeatMode repeat_mode) {
  auto reader =
      CreateSwitchingFrameReader(std::move(file_paths), target_resolution, fps,
                                 camera_switching_interval, repeat_mode);
  return std::make_unique<SwitchingFrameGenerator>(std::move(reader),
                                                   target_resolution, fps);
}

}  // namespace test
}  // namespace webrtc
