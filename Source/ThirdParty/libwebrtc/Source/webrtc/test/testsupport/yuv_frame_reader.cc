/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <string>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "test/frame_utils.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
size_t FrameSizeBytes(int width, int height) {
  int half_width = (width + 1) / 2;
  size_t size_y = static_cast<size_t>(width) * height;
  size_t size_uv = static_cast<size_t>(half_width) * ((height + 1) / 2);
  return size_y + 2 * size_uv;
}

YuvFrameReaderImpl::DropperUtil::DropperUtil(int source_fps, int target_fps)
    : frame_size_buckets_(
          std::max(1.0, static_cast<double>(source_fps) / target_fps)),
      bucket_level_(0.0) {}

YuvFrameReaderImpl::DropperUtil::DropDecision
YuvFrameReaderImpl::DropperUtil::UpdateLevel() {
  DropDecision decision;
  if (bucket_level_ <= 0.0) {
    decision = DropDecision::kKeepFrame;
    bucket_level_ += frame_size_buckets_;
  } else {
    decision = DropDecision::kDropframe;
  }
  bucket_level_ -= 1.0;
  return decision;
}

YuvFrameReaderImpl::YuvFrameReaderImpl(std::string input_filename,
                                       int width,
                                       int height)
    : YuvFrameReaderImpl(input_filename,
                         width,
                         height,
                         width,
                         height,
                         RepeatMode::kSingle,
                         30,
                         30) {}
YuvFrameReaderImpl::YuvFrameReaderImpl(std::string input_filename,
                                       int input_width,
                                       int input_height,
                                       int desired_width,
                                       int desired_height,
                                       RepeatMode repeat_mode,
                                       absl::optional<int> clip_fps,
                                       int target_fps)
    : input_filename_(input_filename),
      frame_length_in_bytes_(input_width * input_height +
                             2 * ((input_width + 1) / 2) *
                                 ((input_height + 1) / 2)),
      input_width_(input_width),
      input_height_(input_height),
      desired_width_(desired_width),
      desired_height_(desired_height),
      frame_size_bytes_(FrameSizeBytes(input_width, input_height)),
      repeat_mode_(repeat_mode),
      number_of_frames_(-1),
      current_frame_index_(-1),
      dropper_(clip_fps.has_value() ? new DropperUtil(*clip_fps, target_fps)
                                    : nullptr),
      input_file_(nullptr) {}

YuvFrameReaderImpl::~YuvFrameReaderImpl() {
  Close();
}

bool YuvFrameReaderImpl::Init() {
  if (input_width_ <= 0 || input_height_ <= 0) {
    fprintf(stderr, "Frame width and height must be >0, was %d x %d\n",
            input_width_, input_height_);
    return false;
  }
  input_file_ = fopen(input_filename_.c_str(), "rb");
  if (input_file_ == nullptr) {
    fprintf(stderr, "Couldn't open input file for reading: %s\n",
            input_filename_.c_str());
    return false;
  }
  // Calculate total number of frames.
  size_t source_file_size = GetFileSize(input_filename_);
  if (source_file_size <= 0u) {
    fprintf(stderr, "Found empty file: %s\n", input_filename_.c_str());
    return false;
  }
  number_of_frames_ =
      static_cast<int>(source_file_size / frame_length_in_bytes_);
  current_frame_index_ = 0;
  return true;
}

rtc::scoped_refptr<I420Buffer> YuvFrameReaderImpl::ReadFrame() {
  if (input_file_ == nullptr) {
    fprintf(stderr,
            "YuvFrameReaderImpl is not initialized (input file is NULL)\n");
    return nullptr;
  }

  rtc::scoped_refptr<I420Buffer> buffer;

  do {
    if (current_frame_index_ >= number_of_frames_) {
      switch (repeat_mode_) {
        case RepeatMode::kSingle:
          return nullptr;
        case RepeatMode::kRepeat:
          fseek(input_file_, 0, SEEK_SET);
          current_frame_index_ = 0;
          break;
        case RepeatMode::kPingPong:
          if (current_frame_index_ == number_of_frames_ * 2) {
            fseek(input_file_, 0, SEEK_SET);
            current_frame_index_ = 0;
          } else {
            int reverse_frame_index = current_frame_index_ - number_of_frames_;
            int seek_frame_pos = (number_of_frames_ - reverse_frame_index - 1);
            fseek(input_file_, seek_frame_pos * frame_size_bytes_, SEEK_SET);
          }
          break;
      }
    }
    ++current_frame_index_;

    buffer = ReadI420Buffer(input_width_, input_height_, input_file_);
    if (!buffer && ferror(input_file_)) {
      fprintf(stderr, "Error reading from input file: %s\n",
              input_filename_.c_str());
    }
  } while (dropper_ &&
           dropper_->UpdateLevel() == DropperUtil::DropDecision::kDropframe);

  if (input_width_ == desired_width_ && input_height_ == desired_height_) {
    return buffer;
  }

  rtc::scoped_refptr<I420Buffer> rescaled_buffer(
      I420Buffer::Create(desired_width_, desired_height_));
  rescaled_buffer->ScaleFrom(*buffer.get());

  return rescaled_buffer;
}

void YuvFrameReaderImpl::Close() {
  if (input_file_ != nullptr) {
    fclose(input_file_);
    input_file_ = nullptr;
  }
}

size_t YuvFrameReaderImpl::FrameLength() {
  return frame_length_in_bytes_;
}

int YuvFrameReaderImpl::NumberOfFrames() {
  return number_of_frames_;
}

}  // namespace test
}  // namespace webrtc
