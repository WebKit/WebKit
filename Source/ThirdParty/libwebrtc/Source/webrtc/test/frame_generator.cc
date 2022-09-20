/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/frame_generator.h"

#include <string.h>

#include <cstdint>
#include <cstdio>
#include <memory>

#include "api/video/i010_buffer.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_rotation.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "test/frame_utils.h"

namespace webrtc {
namespace test {

SquareGenerator::SquareGenerator(int width,
                                 int height,
                                 OutputType type,
                                 int num_squares)
    : type_(type) {
  ChangeResolution(width, height);
  for (int i = 0; i < num_squares; ++i) {
    squares_.emplace_back(new Square(width, height, i + 1));
  }
}

void SquareGenerator::ChangeResolution(size_t width, size_t height) {
  MutexLock lock(&mutex_);
  width_ = static_cast<int>(width);
  height_ = static_cast<int>(height);
  RTC_CHECK(width_ > 0);
  RTC_CHECK(height_ > 0);
}

rtc::scoped_refptr<I420Buffer> SquareGenerator::CreateI420Buffer(int width,
                                                                 int height) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(width, height));
  memset(buffer->MutableDataY(), 127, height * buffer->StrideY());
  memset(buffer->MutableDataU(), 127,
         buffer->ChromaHeight() * buffer->StrideU());
  memset(buffer->MutableDataV(), 127,
         buffer->ChromaHeight() * buffer->StrideV());
  return buffer;
}

FrameGeneratorInterface::VideoFrameData SquareGenerator::NextFrame() {
  MutexLock lock(&mutex_);

  rtc::scoped_refptr<VideoFrameBuffer> buffer = nullptr;
  switch (type_) {
    case OutputType::kI420:
    case OutputType::kI010:
    case OutputType::kNV12: {
      buffer = CreateI420Buffer(width_, height_);
      break;
    }
    case OutputType::kI420A: {
      rtc::scoped_refptr<I420Buffer> yuv_buffer =
          CreateI420Buffer(width_, height_);
      rtc::scoped_refptr<I420Buffer> axx_buffer =
          CreateI420Buffer(width_, height_);
      buffer = WrapI420ABuffer(yuv_buffer->width(), yuv_buffer->height(),
                               yuv_buffer->DataY(), yuv_buffer->StrideY(),
                               yuv_buffer->DataU(), yuv_buffer->StrideU(),
                               yuv_buffer->DataV(), yuv_buffer->StrideV(),
                               axx_buffer->DataY(), axx_buffer->StrideY(),
                               // To keep references alive.
                               [yuv_buffer, axx_buffer] {});
      break;
    }
    default:
      RTC_DCHECK_NOTREACHED() << "The given output format is not supported.";
  }

  for (const auto& square : squares_)
    square->Draw(buffer);

  if (type_ == OutputType::kI010) {
    buffer = I010Buffer::Copy(*buffer->ToI420());
  } else if (type_ == OutputType::kNV12) {
    buffer = NV12Buffer::Copy(*buffer->ToI420());
  }

  return VideoFrameData(buffer, absl::nullopt);
}

SquareGenerator::Square::Square(int width, int height, int seed)
    : random_generator_(seed),
      x_(random_generator_.Rand(0, width)),
      y_(random_generator_.Rand(0, height)),
      length_(random_generator_.Rand(1, width > 4 ? width / 4 : 1)),
      yuv_y_(random_generator_.Rand(0, 255)),
      yuv_u_(random_generator_.Rand(0, 255)),
      yuv_v_(random_generator_.Rand(0, 255)),
      yuv_a_(random_generator_.Rand(0, 255)) {}

void SquareGenerator::Square::Draw(
    const rtc::scoped_refptr<VideoFrameBuffer>& frame_buffer) {
  RTC_DCHECK(frame_buffer->type() == VideoFrameBuffer::Type::kI420 ||
             frame_buffer->type() == VideoFrameBuffer::Type::kI420A);
  rtc::scoped_refptr<I420BufferInterface> buffer = frame_buffer->ToI420();
  int length_cap = std::min(buffer->height(), buffer->width()) / 4;
  int length = std::min(length_, length_cap);
  x_ = (x_ + random_generator_.Rand(0, 4)) % (buffer->width() - length);
  y_ = (y_ + random_generator_.Rand(0, 4)) % (buffer->height() - length);
  for (int y = y_; y < y_ + length; ++y) {
    uint8_t* pos_y =
        (const_cast<uint8_t*>(buffer->DataY()) + x_ + y * buffer->StrideY());
    memset(pos_y, yuv_y_, length);
  }

  for (int y = y_; y < y_ + length; y = y + 2) {
    uint8_t* pos_u = (const_cast<uint8_t*>(buffer->DataU()) + x_ / 2 +
                      y / 2 * buffer->StrideU());
    memset(pos_u, yuv_u_, length / 2);
    uint8_t* pos_v = (const_cast<uint8_t*>(buffer->DataV()) + x_ / 2 +
                      y / 2 * buffer->StrideV());
    memset(pos_v, yuv_v_, length / 2);
  }

  if (frame_buffer->type() == VideoFrameBuffer::Type::kI420)
    return;

  // Optionally draw on alpha plane if given.
  const webrtc::I420ABufferInterface* yuva_buffer = frame_buffer->GetI420A();
  for (int y = y_; y < y_ + length; ++y) {
    uint8_t* pos_y = (const_cast<uint8_t*>(yuva_buffer->DataA()) + x_ +
                      y * yuva_buffer->StrideA());
    memset(pos_y, yuv_a_, length);
  }
}

YuvFileGenerator::YuvFileGenerator(std::vector<FILE*> files,
                                   size_t width,
                                   size_t height,
                                   int frame_repeat_count)
    : file_index_(0),
      frame_index_(std::numeric_limits<size_t>::max()),
      files_(files),
      width_(width),
      height_(height),
      frame_size_(CalcBufferSize(VideoType::kI420,
                                 static_cast<int>(width_),
                                 static_cast<int>(height_))),
      frame_buffer_(new uint8_t[frame_size_]),
      frame_display_count_(frame_repeat_count),
      current_display_count_(0) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GT(frame_repeat_count, 0);
}

YuvFileGenerator::~YuvFileGenerator() {
  for (FILE* file : files_)
    fclose(file);
}

FrameGeneratorInterface::VideoFrameData YuvFileGenerator::NextFrame() {
  // Empty update by default.
  VideoFrame::UpdateRect update_rect{0, 0, 0, 0};
  if (current_display_count_ == 0) {
    const bool got_new_frame = ReadNextFrame();
    // Full update on a new frame from file.
    if (got_new_frame) {
      update_rect = VideoFrame::UpdateRect{0, 0, static_cast<int>(width_),
                                           static_cast<int>(height_)};
    }
  }
  if (++current_display_count_ >= frame_display_count_)
    current_display_count_ = 0;

  return VideoFrameData(last_read_buffer_, update_rect);
}

bool YuvFileGenerator::ReadNextFrame() {
  size_t prev_frame_index = frame_index_;
  size_t prev_file_index = file_index_;
  last_read_buffer_ = test::ReadI420Buffer(
      static_cast<int>(width_), static_cast<int>(height_), files_[file_index_]);
  ++frame_index_;
  if (!last_read_buffer_) {
    // No more frames to read in this file, rewind and move to next file.
    rewind(files_[file_index_]);

    frame_index_ = 0;
    file_index_ = (file_index_ + 1) % files_.size();
    last_read_buffer_ =
        test::ReadI420Buffer(static_cast<int>(width_),
                             static_cast<int>(height_), files_[file_index_]);
    RTC_CHECK(last_read_buffer_);
  }
  return frame_index_ != prev_frame_index || file_index_ != prev_file_index;
}

SlideGenerator::SlideGenerator(int width, int height, int frame_repeat_count)
    : width_(width),
      height_(height),
      frame_display_count_(frame_repeat_count),
      current_display_count_(0),
      random_generator_(1234) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GT(frame_repeat_count, 0);
}

FrameGeneratorInterface::VideoFrameData SlideGenerator::NextFrame() {
  if (current_display_count_ == 0)
    GenerateNewFrame();
  if (++current_display_count_ >= frame_display_count_)
    current_display_count_ = 0;

  return VideoFrameData(buffer_, absl::nullopt);
}

void SlideGenerator::GenerateNewFrame() {
  // The squares should have a varying order of magnitude in order
  // to simulate variation in the slides' complexity.
  const int kSquareNum = 1 << (4 + (random_generator_.Rand(0, 3) * 2));

  buffer_ = I420Buffer::Create(width_, height_);
  memset(buffer_->MutableDataY(), 127, height_ * buffer_->StrideY());
  memset(buffer_->MutableDataU(), 127,
         buffer_->ChromaHeight() * buffer_->StrideU());
  memset(buffer_->MutableDataV(), 127,
         buffer_->ChromaHeight() * buffer_->StrideV());

  for (int i = 0; i < kSquareNum; ++i) {
    int length = random_generator_.Rand(1, width_ > 4 ? width_ / 4 : 1);
    // Limit the length of later squares so that they don't overwrite the
    // previous ones too much.
    length = (length * (kSquareNum - i)) / kSquareNum;

    int x = random_generator_.Rand(0, width_ - length);
    int y = random_generator_.Rand(0, height_ - length);
    uint8_t yuv_y = random_generator_.Rand(0, 255);
    uint8_t yuv_u = random_generator_.Rand(0, 255);
    uint8_t yuv_v = random_generator_.Rand(0, 255);

    for (int yy = y; yy < y + length; ++yy) {
      uint8_t* pos_y = (buffer_->MutableDataY() + x + yy * buffer_->StrideY());
      memset(pos_y, yuv_y, length);
    }
    for (int yy = y; yy < y + length; yy += 2) {
      uint8_t* pos_u =
          (buffer_->MutableDataU() + x / 2 + yy / 2 * buffer_->StrideU());
      memset(pos_u, yuv_u, length / 2);
      uint8_t* pos_v =
          (buffer_->MutableDataV() + x / 2 + yy / 2 * buffer_->StrideV());
      memset(pos_v, yuv_v, length / 2);
    }
  }
}

ScrollingImageFrameGenerator::ScrollingImageFrameGenerator(
    Clock* clock,
    const std::vector<FILE*>& files,
    size_t source_width,
    size_t source_height,
    size_t target_width,
    size_t target_height,
    int64_t scroll_time_ms,
    int64_t pause_time_ms)
    : clock_(clock),
      start_time_(clock->TimeInMilliseconds()),
      scroll_time_(scroll_time_ms),
      pause_time_(pause_time_ms),
      num_frames_(files.size()),
      target_width_(static_cast<int>(target_width)),
      target_height_(static_cast<int>(target_height)),
      current_frame_num_(num_frames_ - 1),
      prev_frame_not_scrolled_(false),
      current_source_frame_(nullptr, absl::nullopt),
      current_frame_(nullptr, absl::nullopt),
      file_generator_(files, source_width, source_height, 1) {
  RTC_DCHECK(clock_ != nullptr);
  RTC_DCHECK_GT(num_frames_, 0);
  RTC_DCHECK_GE(source_height, target_height);
  RTC_DCHECK_GE(source_width, target_width);
  RTC_DCHECK_GE(scroll_time_ms, 0);
  RTC_DCHECK_GE(pause_time_ms, 0);
  RTC_DCHECK_GT(scroll_time_ms + pause_time_ms, 0);
}

FrameGeneratorInterface::VideoFrameData
ScrollingImageFrameGenerator::NextFrame() {
  const int64_t kFrameDisplayTime = scroll_time_ + pause_time_;
  const int64_t now = clock_->TimeInMilliseconds();
  int64_t ms_since_start = now - start_time_;

  size_t frame_num = (ms_since_start / kFrameDisplayTime) % num_frames_;
  UpdateSourceFrame(frame_num);

  bool cur_frame_not_scrolled;

  double scroll_factor;
  int64_t time_into_frame = ms_since_start % kFrameDisplayTime;
  if (time_into_frame < scroll_time_) {
    scroll_factor = static_cast<double>(time_into_frame) / scroll_time_;
    cur_frame_not_scrolled = false;
  } else {
    scroll_factor = 1.0;
    cur_frame_not_scrolled = true;
  }
  CropSourceToScrolledImage(scroll_factor);

  bool same_scroll_position =
      prev_frame_not_scrolled_ && cur_frame_not_scrolled;
  if (!same_scroll_position) {
    // If scrolling is not finished yet, force full frame update.
    current_frame_.update_rect =
        VideoFrame::UpdateRect{0, 0, target_width_, target_height_};
  }
  prev_frame_not_scrolled_ = cur_frame_not_scrolled;

  return current_frame_;
}

void ScrollingImageFrameGenerator::UpdateSourceFrame(size_t frame_num) {
  VideoFrame::UpdateRect acc_update{0, 0, 0, 0};
  while (current_frame_num_ != frame_num) {
    current_source_frame_ = file_generator_.NextFrame();
    if (current_source_frame_.update_rect) {
      acc_update.Union(*current_source_frame_.update_rect);
    }
    current_frame_num_ = (current_frame_num_ + 1) % num_frames_;
  }
  current_source_frame_.update_rect = acc_update;
}

void ScrollingImageFrameGenerator::CropSourceToScrolledImage(
    double scroll_factor) {
  int scroll_margin_x = current_source_frame_.buffer->width() - target_width_;
  int pixels_scrolled_x =
      static_cast<int>(scroll_margin_x * scroll_factor + 0.5);
  int scroll_margin_y = current_source_frame_.buffer->height() - target_height_;
  int pixels_scrolled_y =
      static_cast<int>(scroll_margin_y * scroll_factor + 0.5);

  rtc::scoped_refptr<I420BufferInterface> i420_buffer =
      current_source_frame_.buffer->ToI420();
  int offset_y =
      (i420_buffer->StrideY() * pixels_scrolled_y) + pixels_scrolled_x;
  int offset_u = (i420_buffer->StrideU() * (pixels_scrolled_y / 2)) +
                 (pixels_scrolled_x / 2);
  int offset_v = (i420_buffer->StrideV() * (pixels_scrolled_y / 2)) +
                 (pixels_scrolled_x / 2);

  VideoFrame::UpdateRect update_rect =
      current_source_frame_.update_rect->IsEmpty()
          ? VideoFrame::UpdateRect{0, 0, 0, 0}
          : VideoFrame::UpdateRect{0, 0, target_width_, target_height_};
  current_frame_ = VideoFrameData(
      WrapI420Buffer(target_width_, target_height_,
                     &i420_buffer->DataY()[offset_y], i420_buffer->StrideY(),
                     &i420_buffer->DataU()[offset_u], i420_buffer->StrideU(),
                     &i420_buffer->DataV()[offset_v], i420_buffer->StrideV(),
                     // To keep reference alive.
                     [i420_buffer] {}),
      update_rect);
}

}  // namespace test
}  // namespace webrtc
