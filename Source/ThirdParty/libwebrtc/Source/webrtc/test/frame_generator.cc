/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/frame_generator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/frame_utils.h"

namespace webrtc {
namespace test {
namespace {

class ChromaGenerator : public FrameGenerator {
 public:
  ChromaGenerator(size_t width, size_t height) : angle_(0.0) {
    ChangeResolution(width, height);
  }

  void ChangeResolution(size_t width, size_t height) override {
    rtc::CritScope lock(&crit_);
    width_ = width;
    height_ = height;
    RTC_CHECK(width_ > 0);
    RTC_CHECK(height_ > 0);
    half_width_ = (width_ + 1) / 2;
    y_size_ = width_ * height_;
    uv_size_ = half_width_ * ((height_ + 1) / 2);
  }

  VideoFrame* NextFrame() override {
    rtc::CritScope lock(&crit_);
    angle_ += 30.0;
    uint8_t u = fabs(sin(angle_)) * 0xFF;
    uint8_t v = fabs(cos(angle_)) * 0xFF;

    // Ensure stride == width.
    rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(
        static_cast<int>(width_), static_cast<int>(height_),
        static_cast<int>(width_), static_cast<int>(half_width_),
        static_cast<int>(half_width_)));

    memset(buffer->MutableDataY(), 0x80, y_size_);
    memset(buffer->MutableDataU(), u, uv_size_);
    memset(buffer->MutableDataV(), v, uv_size_);

    frame_.reset(new VideoFrame(buffer, 0, 0, webrtc::kVideoRotation_0));
    return frame_.get();
  }

 private:
  rtc::CriticalSection crit_;
  double angle_ GUARDED_BY(&crit_);
  size_t width_ GUARDED_BY(&crit_);
  size_t height_ GUARDED_BY(&crit_);
  size_t half_width_ GUARDED_BY(&crit_);
  size_t y_size_ GUARDED_BY(&crit_);
  size_t uv_size_ GUARDED_BY(&crit_);
  std::unique_ptr<VideoFrame> frame_ GUARDED_BY(&crit_);
};

class YuvFileGenerator : public FrameGenerator {
 public:
  YuvFileGenerator(std::vector<FILE*> files,
                   size_t width,
                   size_t height,
                   int frame_repeat_count)
      : file_index_(0),
        files_(files),
        width_(width),
        height_(height),
        frame_size_(CalcBufferSize(kI420,
                                   static_cast<int>(width_),
                                   static_cast<int>(height_))),
        frame_buffer_(new uint8_t[frame_size_]),
        frame_display_count_(frame_repeat_count),
        current_display_count_(0) {
    assert(width > 0);
    assert(height > 0);
    assert(frame_repeat_count > 0);
  }

  virtual ~YuvFileGenerator() {
    for (FILE* file : files_)
      fclose(file);
  }

  VideoFrame* NextFrame() override {
    if (current_display_count_ == 0)
      ReadNextFrame();
    if (++current_display_count_ >= frame_display_count_)
      current_display_count_ = 0;

    temp_frame_.reset(
        new VideoFrame(last_read_buffer_, 0, 0, webrtc::kVideoRotation_0));
    return temp_frame_.get();
  }

  void ReadNextFrame() {
    last_read_buffer_ =
        test::ReadI420Buffer(static_cast<int>(width_),
                             static_cast<int>(height_),
                             files_[file_index_]);
    if (!last_read_buffer_) {
      // No more frames to read in this file, rewind and move to next file.
      rewind(files_[file_index_]);
      file_index_ = (file_index_ + 1) % files_.size();
      last_read_buffer_ =
          test::ReadI420Buffer(static_cast<int>(width_),
                               static_cast<int>(height_),
                               files_[file_index_]);
      RTC_CHECK(last_read_buffer_);
    }
  }

 private:
  size_t file_index_;
  const std::vector<FILE*> files_;
  const size_t width_;
  const size_t height_;
  const size_t frame_size_;
  const std::unique_ptr<uint8_t[]> frame_buffer_;
  const int frame_display_count_;
  int current_display_count_;
  rtc::scoped_refptr<I420Buffer> last_read_buffer_;
  std::unique_ptr<VideoFrame> temp_frame_;
};

class ScrollingImageFrameGenerator : public FrameGenerator {
 public:
  ScrollingImageFrameGenerator(Clock* clock,
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
        current_source_frame_(nullptr),
        file_generator_(files, source_width, source_height, 1) {
    RTC_DCHECK(clock_ != nullptr);
    RTC_DCHECK_GT(num_frames_, 0u);
    RTC_DCHECK_GE(source_height, target_height);
    RTC_DCHECK_GE(source_width, target_width);
    RTC_DCHECK_GE(scroll_time_ms, 0);
    RTC_DCHECK_GE(pause_time_ms, 0);
    RTC_DCHECK_GT(scroll_time_ms + pause_time_ms, 0);
  }

  virtual ~ScrollingImageFrameGenerator() {}

  VideoFrame* NextFrame() override {
    const int64_t kFrameDisplayTime = scroll_time_ + pause_time_;
    const int64_t now = clock_->TimeInMilliseconds();
    int64_t ms_since_start = now - start_time_;

    size_t frame_num = (ms_since_start / kFrameDisplayTime) % num_frames_;
    UpdateSourceFrame(frame_num);

    double scroll_factor;
    int64_t time_into_frame = ms_since_start % kFrameDisplayTime;
    if (time_into_frame < scroll_time_) {
      scroll_factor = static_cast<double>(time_into_frame) / scroll_time_;
    } else {
      scroll_factor = 1.0;
    }
    CropSourceToScrolledImage(scroll_factor);

    return &current_frame_;
  }

  void UpdateSourceFrame(size_t frame_num) {
    while (current_frame_num_ != frame_num) {
      current_source_frame_ = file_generator_.NextFrame();
      current_frame_num_ = (current_frame_num_ + 1) % num_frames_;
    }
    RTC_DCHECK(current_source_frame_ != nullptr);
  }

  void CropSourceToScrolledImage(double scroll_factor) {
    int scroll_margin_x = current_source_frame_->width() - target_width_;
    int pixels_scrolled_x =
        static_cast<int>(scroll_margin_x * scroll_factor + 0.5);
    int scroll_margin_y = current_source_frame_->height() - target_height_;
    int pixels_scrolled_y =
        static_cast<int>(scroll_margin_y * scroll_factor + 0.5);

    int offset_y = (current_source_frame_->video_frame_buffer()->StrideY() *
                    pixels_scrolled_y) +
                   pixels_scrolled_x;
    int offset_u = (current_source_frame_->video_frame_buffer()->StrideU() *
                    (pixels_scrolled_y / 2)) +
                   (pixels_scrolled_x / 2);
    int offset_v = (current_source_frame_->video_frame_buffer()->StrideV() *
                    (pixels_scrolled_y / 2)) +
                   (pixels_scrolled_x / 2);

    rtc::scoped_refptr<VideoFrameBuffer> frame_buffer(
        current_source_frame_->video_frame_buffer());
    current_frame_ = webrtc::VideoFrame(
        new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
            target_width_, target_height_,
            &frame_buffer->DataY()[offset_y], frame_buffer->StrideY(),
            &frame_buffer->DataU()[offset_u], frame_buffer->StrideU(),
            &frame_buffer->DataV()[offset_v], frame_buffer->StrideV(),
            KeepRefUntilDone(frame_buffer)),
        kVideoRotation_0, 0);
  }

  Clock* const clock_;
  const int64_t start_time_;
  const int64_t scroll_time_;
  const int64_t pause_time_;
  const size_t num_frames_;
  const int target_width_;
  const int target_height_;

  size_t current_frame_num_;
  VideoFrame* current_source_frame_;
  VideoFrame current_frame_;
  YuvFileGenerator file_generator_;
};

}  // namespace

FrameForwarder::FrameForwarder() : sink_(nullptr) {}

void FrameForwarder::IncomingCapturedFrame(const VideoFrame& video_frame) {
  rtc::CritScope lock(&crit_);
  if (sink_)
    sink_->OnFrame(video_frame);
}

void FrameForwarder::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                                     const rtc::VideoSinkWants& wants) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK(!sink_ || sink_ == sink);
  sink_ = sink;
  sink_wants_ = wants;
}

void FrameForwarder::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK_EQ(sink, sink_);
  sink_ = nullptr;
}

rtc::VideoSinkWants FrameForwarder::sink_wants() const {
  rtc::CritScope lock(&crit_);
  return sink_wants_;
}

bool FrameForwarder::has_sinks() const {
  rtc::CritScope lock(&crit_);
  return sink_ != nullptr;
}

FrameGenerator* FrameGenerator::CreateChromaGenerator(size_t width,
                                                      size_t height) {
  return new ChromaGenerator(width, height);
}

FrameGenerator* FrameGenerator::CreateFromYuvFile(
    std::vector<std::string> filenames,
    size_t width,
    size_t height,
    int frame_repeat_count) {
  assert(!filenames.empty());
  std::vector<FILE*> files;
  for (const std::string& filename : filenames) {
    FILE* file = fopen(filename.c_str(), "rb");
    RTC_DCHECK(file != nullptr);
    files.push_back(file);
  }

  return new YuvFileGenerator(files, width, height, frame_repeat_count);
}

FrameGenerator* FrameGenerator::CreateScrollingInputFromYuvFiles(
    Clock* clock,
    std::vector<std::string> filenames,
    size_t source_width,
    size_t source_height,
    size_t target_width,
    size_t target_height,
    int64_t scroll_time_ms,
    int64_t pause_time_ms) {
  assert(!filenames.empty());
  std::vector<FILE*> files;
  for (const std::string& filename : filenames) {
    FILE* file = fopen(filename.c_str(), "rb");
    RTC_DCHECK(file != nullptr);
    files.push_back(file);
  }

  return new ScrollingImageFrameGenerator(
      clock, files, source_width, source_height, target_width, target_height,
      scroll_time_ms, pause_time_ms);
}

}  // namespace test
}  // namespace webrtc
