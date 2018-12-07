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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <memory>

#include "api/video/i010_buffer.h"
#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/frame_utils.h"

namespace webrtc {
namespace test {
namespace {

// Helper method for keeping a reference to passed pointers.
void KeepBufferRefs(rtc::scoped_refptr<webrtc::VideoFrameBuffer>,
                    rtc::scoped_refptr<webrtc::VideoFrameBuffer>) {}

// SquareGenerator is a FrameGenerator that draws a given amount of randomly
// sized and colored squares. Between each new generated frame, the squares
// are moved slightly towards the lower right corner.
class SquareGenerator : public FrameGenerator {
 public:
  SquareGenerator(int width, int height, OutputType type, int num_squares)
      : type_(type) {
    ChangeResolution(width, height);
    for (int i = 0; i < num_squares; ++i) {
      squares_.emplace_back(new Square(width, height, i + 1));
    }
  }

  void ChangeResolution(size_t width, size_t height) override {
    rtc::CritScope lock(&crit_);
    width_ = static_cast<int>(width);
    height_ = static_cast<int>(height);
    RTC_CHECK(width_ > 0);
    RTC_CHECK(height_ > 0);
  }

  rtc::scoped_refptr<I420Buffer> CreateI420Buffer(int width, int height) {
    rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(width, height));
    memset(buffer->MutableDataY(), 127, height * buffer->StrideY());
    memset(buffer->MutableDataU(), 127,
           buffer->ChromaHeight() * buffer->StrideU());
    memset(buffer->MutableDataV(), 127,
           buffer->ChromaHeight() * buffer->StrideV());
    return buffer;
  }

  VideoFrame* NextFrame() override {
    rtc::CritScope lock(&crit_);

    rtc::scoped_refptr<VideoFrameBuffer> buffer = nullptr;
    switch (type_) {
      case OutputType::I420:
      case OutputType::I010: {
        buffer = CreateI420Buffer(width_, height_);
        break;
      }
      case OutputType::I420A: {
        rtc::scoped_refptr<I420Buffer> yuv_buffer =
            CreateI420Buffer(width_, height_);
        rtc::scoped_refptr<I420Buffer> axx_buffer =
            CreateI420Buffer(width_, height_);
        buffer = WrapI420ABuffer(
            yuv_buffer->width(), yuv_buffer->height(), yuv_buffer->DataY(),
            yuv_buffer->StrideY(), yuv_buffer->DataU(), yuv_buffer->StrideU(),
            yuv_buffer->DataV(), yuv_buffer->StrideV(), axx_buffer->DataY(),
            axx_buffer->StrideY(),
            rtc::Bind(&KeepBufferRefs, yuv_buffer, axx_buffer));
        break;
      }
    }

    for (const auto& square : squares_)
      square->Draw(buffer);

    if (type_ == OutputType::I010) {
      buffer = I010Buffer::Copy(*buffer->ToI420());
    }

    frame_.reset(
        new VideoFrame(buffer, webrtc::kVideoRotation_0, 0 /* timestamp_us */));
    return frame_.get();
  }

 private:
  class Square {
   public:
    Square(int width, int height, int seed)
        : random_generator_(seed),
          x_(random_generator_.Rand(0, width)),
          y_(random_generator_.Rand(0, height)),
          length_(random_generator_.Rand(1, width > 4 ? width / 4 : 1)),
          yuv_y_(random_generator_.Rand(0, 255)),
          yuv_u_(random_generator_.Rand(0, 255)),
          yuv_v_(random_generator_.Rand(0, 255)),
          yuv_a_(random_generator_.Rand(0, 255)) {}

    void Draw(const rtc::scoped_refptr<VideoFrameBuffer>& frame_buffer) {
      RTC_DCHECK(frame_buffer->type() == VideoFrameBuffer::Type::kI420 ||
                 frame_buffer->type() == VideoFrameBuffer::Type::kI420A);
      rtc::scoped_refptr<I420BufferInterface> buffer = frame_buffer->ToI420();
      x_ = (x_ + random_generator_.Rand(0, 4)) % (buffer->width() - length_);
      y_ = (y_ + random_generator_.Rand(0, 4)) % (buffer->height() - length_);
      for (int y = y_; y < y_ + length_; ++y) {
        uint8_t* pos_y = (const_cast<uint8_t*>(buffer->DataY()) + x_ +
                          y * buffer->StrideY());
        memset(pos_y, yuv_y_, length_);
      }

      for (int y = y_; y < y_ + length_; y = y + 2) {
        uint8_t* pos_u = (const_cast<uint8_t*>(buffer->DataU()) + x_ / 2 +
                          y / 2 * buffer->StrideU());
        memset(pos_u, yuv_u_, length_ / 2);
        uint8_t* pos_v = (const_cast<uint8_t*>(buffer->DataV()) + x_ / 2 +
                          y / 2 * buffer->StrideV());
        memset(pos_v, yuv_v_, length_ / 2);
      }

      if (frame_buffer->type() == VideoFrameBuffer::Type::kI420)
        return;

      // Optionally draw on alpha plane if given.
      const webrtc::I420ABufferInterface* yuva_buffer =
          frame_buffer->GetI420A();
      for (int y = y_; y < y_ + length_; ++y) {
        uint8_t* pos_y = (const_cast<uint8_t*>(yuva_buffer->DataA()) + x_ +
                          y * yuva_buffer->StrideA());
        memset(pos_y, yuv_a_, length_);
      }
    }

   private:
    Random random_generator_;
    int x_;
    int y_;
    const int length_;
    const uint8_t yuv_y_;
    const uint8_t yuv_u_;
    const uint8_t yuv_v_;
    const uint8_t yuv_a_;
  };

  rtc::CriticalSection crit_;
  const OutputType type_;
  int width_ RTC_GUARDED_BY(&crit_);
  int height_ RTC_GUARDED_BY(&crit_);
  std::vector<std::unique_ptr<Square>> squares_ RTC_GUARDED_BY(&crit_);
  std::unique_ptr<VideoFrame> frame_ RTC_GUARDED_BY(&crit_);
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

  virtual ~YuvFileGenerator() {
    for (FILE* file : files_)
      fclose(file);
  }

  VideoFrame* NextFrame() override {
    if (current_display_count_ == 0)
      ReadNextFrame();
    if (++current_display_count_ >= frame_display_count_)
      current_display_count_ = 0;

    temp_frame_.reset(new VideoFrame(
        last_read_buffer_, webrtc::kVideoRotation_0, 0 /* timestamp_us */));
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

// SlideGenerator works similarly to YuvFileGenerator but it fills the frames
// with randomly sized and colored squares instead of reading their content
// from files.
class SlideGenerator : public FrameGenerator {
 public:
  SlideGenerator(int width, int height, int frame_repeat_count)
      : width_(width),
        height_(height),
        frame_display_count_(frame_repeat_count),
        current_display_count_(0),
        random_generator_(1234) {
    RTC_DCHECK_GT(width, 0);
    RTC_DCHECK_GT(height, 0);
    RTC_DCHECK_GT(frame_repeat_count, 0);
  }

  VideoFrame* NextFrame() override {
    if (current_display_count_ == 0)
      GenerateNewFrame();
    if (++current_display_count_ >= frame_display_count_)
      current_display_count_ = 0;

    frame_.reset(new VideoFrame(buffer_, webrtc::kVideoRotation_0,
                                0 /* timestamp_us */));
    return frame_.get();
  }

  // Generates some randomly sized and colored squares scattered
  // over the frame.
  void GenerateNewFrame() {
    // The squares should have a varying order of magnitude in order
    // to simulate variation in the slides' complexity.
    const int kSquareNum =  1 << (4 + (random_generator_.Rand(0, 3) * 2));

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
        uint8_t* pos_y =
            (buffer_->MutableDataY() + x + yy * buffer_->StrideY());
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

 private:
  const int width_;
  const int height_;
  const int frame_display_count_;
  int current_display_count_;
  Random random_generator_;
  rtc::scoped_refptr<I420Buffer> buffer_;
  std::unique_ptr<VideoFrame> frame_;
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
    RTC_DCHECK_GT(num_frames_, 0);
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

    return current_frame_ ? &*current_frame_ : nullptr;
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

    rtc::scoped_refptr<I420BufferInterface> i420_buffer =
        current_source_frame_->video_frame_buffer()->ToI420();
    int offset_y =
        (i420_buffer->StrideY() * pixels_scrolled_y) + pixels_scrolled_x;
    int offset_u = (i420_buffer->StrideU() * (pixels_scrolled_y / 2)) +
                   (pixels_scrolled_x / 2);
    int offset_v = (i420_buffer->StrideV() * (pixels_scrolled_y / 2)) +
                   (pixels_scrolled_x / 2);

    current_frame_ = webrtc::VideoFrame(
        WrapI420Buffer(target_width_, target_height_,
                       &i420_buffer->DataY()[offset_y], i420_buffer->StrideY(),
                       &i420_buffer->DataU()[offset_u], i420_buffer->StrideU(),
                       &i420_buffer->DataV()[offset_v], i420_buffer->StrideV(),
                       KeepRefUntilDone(i420_buffer)),
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
  absl::optional<VideoFrame> current_frame_;
  YuvFileGenerator file_generator_;
};

}  // namespace

FrameForwarder::FrameForwarder() : sink_(nullptr) {}
FrameForwarder::~FrameForwarder() {}

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

std::unique_ptr<FrameGenerator> FrameGenerator::CreateSquareGenerator(
    int width,
    int height,
    absl::optional<OutputType> type,
    absl::optional<int> num_squares) {
  return std::unique_ptr<FrameGenerator>(
      new SquareGenerator(width, height, type.value_or(OutputType::I420),
                          num_squares.value_or(10)));
}

std::unique_ptr<FrameGenerator> FrameGenerator::CreateSlideGenerator(
    int width, int height, int frame_repeat_count) {
  return std::unique_ptr<FrameGenerator>(new SlideGenerator(
      width, height, frame_repeat_count));
}

std::unique_ptr<FrameGenerator> FrameGenerator::CreateFromYuvFile(
    std::vector<std::string> filenames,
    size_t width,
    size_t height,
    int frame_repeat_count) {
  RTC_DCHECK(!filenames.empty());
  std::vector<FILE*> files;
  for (const std::string& filename : filenames) {
    FILE* file = fopen(filename.c_str(), "rb");
    RTC_DCHECK(file != nullptr) << "Failed to open: '" << filename << "'\n";
    files.push_back(file);
  }

  return std::unique_ptr<FrameGenerator>(
      new YuvFileGenerator(files, width, height, frame_repeat_count));
}

std::unique_ptr<FrameGenerator>
FrameGenerator::CreateScrollingInputFromYuvFiles(
    Clock* clock,
    std::vector<std::string> filenames,
    size_t source_width,
    size_t source_height,
    size_t target_width,
    size_t target_height,
    int64_t scroll_time_ms,
    int64_t pause_time_ms) {
  RTC_DCHECK(!filenames.empty());
  std::vector<FILE*> files;
  for (const std::string& filename : filenames) {
    FILE* file = fopen(filename.c_str(), "rb");
    RTC_DCHECK(file != nullptr);
    files.push_back(file);
  }

  return std::unique_ptr<FrameGenerator>(new ScrollingImageFrameGenerator(
      clock, files, source_width, source_height, target_width, target_height,
      scroll_time_ms, pause_time_ms));
}

}  // namespace test
}  // namespace webrtc
