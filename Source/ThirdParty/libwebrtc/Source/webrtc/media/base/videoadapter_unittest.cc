/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>  // For INT_MAX

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/testutils.h"
#include "webrtc/media/base/videoadapter.h"

namespace cricket {

class VideoAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    capturer_.reset(new FakeVideoCapturer);
    capture_format_ = capturer_->GetSupportedFormats()->at(0);
    capture_format_.interval = VideoFormat::FpsToInterval(30);

    listener_.reset(new VideoCapturerListener(&adapter_));
    capturer_->AddOrUpdateSink(listener_.get(), rtc::VideoSinkWants());
  }

  virtual void TearDown() {
    // Explicitly disconnect the VideoCapturer before to avoid data races
    // (frames delivered to VideoCapturerListener while it's being destructed).
    capturer_->RemoveSink(listener_.get());
  }

 protected:
  class VideoCapturerListener
      : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    struct Stats {
      int captured_frames;
      int dropped_frames;
      bool last_adapt_was_no_op;

      int cropped_width;
      int cropped_height;
      int out_width;
      int out_height;
    };

    explicit VideoCapturerListener(VideoAdapter* adapter)
        : video_adapter_(adapter),
          cropped_width_(0),
          cropped_height_(0),
          out_width_(0),
          out_height_(0),
          captured_frames_(0),
          dropped_frames_(0),
          last_adapt_was_no_op_(false) {}

    void OnFrame(const webrtc::VideoFrame& frame) {
      rtc::CritScope lock(&crit_);
      const int in_width = frame.width();
      const int in_height = frame.height();
      int cropped_width;
      int cropped_height;
      int out_width;
      int out_height;
      if (video_adapter_->AdaptFrameResolution(
              in_width, in_height,
              frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec,
              &cropped_width, &cropped_height, &out_width, &out_height)) {
        cropped_width_ = cropped_width;
        cropped_height_ = cropped_height;
        out_width_ = out_width;
        out_height_ = out_height;
        last_adapt_was_no_op_ =
            (in_width == cropped_width && in_height == cropped_height &&
             in_width == out_width && in_height == out_height);
      } else {
        ++dropped_frames_;
      }
      ++captured_frames_;
    }

    Stats GetStats() {
      rtc::CritScope lock(&crit_);
      Stats stats;
      stats.captured_frames = captured_frames_;
      stats.dropped_frames = dropped_frames_;
      stats.last_adapt_was_no_op = last_adapt_was_no_op_;
      stats.cropped_width = cropped_width_;
      stats.cropped_height = cropped_height_;
      stats.out_width = out_width_;
      stats.out_height = out_height_;
      return stats;
    }

   private:
    rtc::CriticalSection crit_;
    VideoAdapter* video_adapter_;
    int cropped_width_;
    int cropped_height_;
    int out_width_;
    int out_height_;
    int captured_frames_;
    int dropped_frames_;
    bool last_adapt_was_no_op_;
  };


  void VerifyAdaptedResolution(const VideoCapturerListener::Stats& stats,
                               int cropped_width,
                               int cropped_height,
                               int out_width,
                               int out_height) {
    EXPECT_EQ(cropped_width, stats.cropped_width);
    EXPECT_EQ(cropped_height, stats.cropped_height);
    EXPECT_EQ(out_width, stats.out_width);
    EXPECT_EQ(out_height, stats.out_height);
  }

  std::unique_ptr<FakeVideoCapturer> capturer_;
  VideoAdapter adapter_;
  int cropped_width_;
  int cropped_height_;
  int out_width_;
  int out_height_;
  std::unique_ptr<VideoCapturerListener> listener_;
  VideoFormat capture_format_;
};

// Do not adapt the frame rate or the resolution. Expect no frame drop, no
// cropping, and no resolution change.
TEST_F(VideoAdapterTest, AdaptNothing) {
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop and no resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          capture_format_.width, capture_format_.height);
  EXPECT_TRUE(stats.last_adapt_was_no_op);
}

TEST_F(VideoAdapterTest, AdaptZeroInterval) {
  VideoFormat format = capturer_->GetSupportedFormats()->at(0);
  format.interval = 0;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no crash and that frames aren't dropped.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          capture_format_.width, capture_format_.height);
}

// Adapt the frame rate to be half of the capture rate at the beginning. Expect
// the number of dropped frames to be half of the number the captured frames.
TEST_F(VideoAdapterTest, AdaptFramerateToHalf) {
  VideoFormat request_format = capture_format_;
  request_format.interval *= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));

  // Capture 10 frames and verify that every other frame is dropped. The first
  // frame should not be dropped.
  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 1);
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 2);
  EXPECT_EQ(1, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 3);
  EXPECT_EQ(1, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 4);
  EXPECT_EQ(2, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 5);
  EXPECT_EQ(2, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 6);
  EXPECT_EQ(3, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 7);
  EXPECT_EQ(3, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 8);
  EXPECT_EQ(4, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 9);
  EXPECT_EQ(4, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 10);
  EXPECT_EQ(5, listener_->GetStats().dropped_frames);
}

// Adapt the frame rate to be two thirds of the capture rate at the beginning.
// Expect the number of dropped frames to be one thirds of the number the
// captured frames.
TEST_F(VideoAdapterTest, AdaptFramerateToTwoThirds) {
  VideoFormat request_format = capture_format_;
  request_format.interval = request_format.interval * 3 / 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));

  // Capture 10 frames and verify that every third frame is dropped. The first
  // frame should not be dropped.
  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 1);
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 2);
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 3);
  EXPECT_EQ(1, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 4);
  EXPECT_EQ(1, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 5);
  EXPECT_EQ(1, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 6);
  EXPECT_EQ(2, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 7);
  EXPECT_EQ(2, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 8);
  EXPECT_EQ(2, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 9);
  EXPECT_EQ(3, listener_->GetStats().dropped_frames);

  capturer_->CaptureFrame();
  EXPECT_GE(listener_->GetStats().captured_frames, 10);
  EXPECT_EQ(3, listener_->GetStats().dropped_frames);
}

// Request frame rate twice as high as captured frame rate. Expect no frame
// drop.
TEST_F(VideoAdapterTest, AdaptFramerateHighLimit) {
  VideoFormat request_format = capture_format_;
  request_format.interval /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop.
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);
}

// After the first timestamp, add a big offset to the timestamps. Expect that
// the adapter is conservative and resets to the new offset and does not drop
// any frame.
TEST_F(VideoAdapterTest, AdaptFramerateTimestampOffset) {
  const int64_t capture_interval = VideoFormat::FpsToInterval(30);
  adapter_.OnOutputFormatRequest(
      VideoFormat(640, 480, capture_interval, cricket::FOURCC_ANY));

  const int64_t first_timestamp = 0;
  adapter_.AdaptFrameResolution(640, 480, first_timestamp,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  const int64_t big_offset = -987654321LL * 1000;
  const int64_t second_timestamp = big_offset;
  adapter_.AdaptFrameResolution(640, 480, second_timestamp,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  const int64_t third_timestamp = big_offset + capture_interval;
  adapter_.AdaptFrameResolution(640, 480, third_timestamp,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);
}

// Request 30 fps and send 30 fps with jitter. Expect that no frame is dropped.
TEST_F(VideoAdapterTest, AdaptFramerateTimestampJitter) {
  const int64_t capture_interval = VideoFormat::FpsToInterval(30);
  adapter_.OnOutputFormatRequest(
      VideoFormat(640, 480, capture_interval, cricket::FOURCC_ANY));

  adapter_.AdaptFrameResolution(640, 480, capture_interval * 0 / 10,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  adapter_.AdaptFrameResolution(640, 480, capture_interval * 10 / 10 - 1,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  adapter_.AdaptFrameResolution(640, 480, capture_interval * 25 / 10,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  adapter_.AdaptFrameResolution(640, 480, capture_interval * 30 / 10,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  adapter_.AdaptFrameResolution(640, 480, capture_interval * 35 / 10,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);

  adapter_.AdaptFrameResolution(640, 480, capture_interval * 50 / 10,
                                &cropped_width_, &cropped_height_,
                                &out_width_, &out_height_);
  EXPECT_GT(out_width_, 0);
  EXPECT_GT(out_height_, 0);
}

// Adapt the frame rate to be half of the capture rate after capturing no less
// than 10 frames. Expect no frame dropped before adaptation and frame dropped
// after adaptation.
TEST_F(VideoAdapterTest, AdaptFramerateOntheFly) {
  VideoFormat request_format = capture_format_;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop before adaptation.
  EXPECT_EQ(0, listener_->GetStats().dropped_frames);

  // Adapat the frame rate.
  request_format.interval *= 2;
  adapter_.OnOutputFormatRequest(request_format);

  for (int i = 0; i < 20; ++i)
    capturer_->CaptureFrame();

  // Verify frame drop after adaptation.
  EXPECT_GT(listener_->GetStats().dropped_frames, 0);
}

// Set a very high output pixel resolution. Expect no cropping or resolution
// change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionHighLimit) {
  VideoFormat output_format = capture_format_;
  output_format.width *= 10;
  output_format.height *= 10;
  adapter_.OnOutputFormatRequest(output_format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));
  EXPECT_EQ(capture_format_.width, cropped_width_);
  EXPECT_EQ(capture_format_.height, cropped_height_);
  EXPECT_EQ(capture_format_.width, out_width_);
  EXPECT_EQ(capture_format_.height, out_height_);
}

// Adapt the frame resolution to be the same as capture resolution. Expect no
// cropping or resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionIdentical) {
  adapter_.OnOutputFormatRequest(capture_format_);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));
  EXPECT_EQ(capture_format_.width, cropped_width_);
  EXPECT_EQ(capture_format_.height, cropped_height_);
  EXPECT_EQ(capture_format_.width, out_width_);
  EXPECT_EQ(capture_format_.height, out_height_);
}

// Adapt the frame resolution to be a quarter of the capture resolution. Expect
// no cropping, but a resolution change.
TEST_F(VideoAdapterTest, AdaptFrameResolutionQuarter) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));
  EXPECT_EQ(capture_format_.width, cropped_width_);
  EXPECT_EQ(capture_format_.height, cropped_height_);
  EXPECT_EQ(request_format.width, out_width_);
  EXPECT_EQ(request_format.height, out_height_);
}

// Adapt the pixel resolution to 0. Expect frame drop.
TEST_F(VideoAdapterTest, AdaptFrameResolutionDrop) {
  VideoFormat output_format = capture_format_;
  output_format.width = 0;
  output_format.height = 0;
  adapter_.OnOutputFormatRequest(output_format);
  EXPECT_FALSE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));
}

// Adapt the frame resolution to be a quarter of the capture resolution at the
// beginning. Expect no cropping but a resolution change.
TEST_F(VideoAdapterTest, AdaptResolution) {
  VideoFormat request_format = capture_format_;
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no frame drop, no cropping, and resolution change.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, capture_format_.width, capture_format_.height,
                          request_format.width, request_format.height);
}

// Adapt the frame resolution to be a quarter of the capture resolution after
// capturing no less than 10 frames. Expect no resolution change before
// adaptation and resolution change after adaptation.
TEST_F(VideoAdapterTest, AdaptResolutionOnTheFly) {
  VideoFormat request_format = capture_format_;
  adapter_.OnOutputFormatRequest(request_format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify no resolution change before adaptation.
  VerifyAdaptedResolution(listener_->GetStats(),
                          capture_format_.width, capture_format_.height,
                          request_format.width, request_format.height);

  // Adapt the frame resolution.
  request_format.width /= 2;
  request_format.height /= 2;
  adapter_.OnOutputFormatRequest(request_format);
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify resolution change after adaptation.
  VerifyAdaptedResolution(listener_->GetStats(),
                          capture_format_.width, capture_format_.height,
                          request_format.width, request_format.height);
}

// Drop all frames.
TEST_F(VideoAdapterTest, DropAllFrames) {
  VideoFormat format;  // with resolution 0x0.
  adapter_.OnOutputFormatRequest(format);
  EXPECT_EQ(CS_RUNNING, capturer_->Start(capture_format_));
  for (int i = 0; i < 10; ++i)
    capturer_->CaptureFrame();

  // Verify all frames are dropped.
  VideoCapturerListener::Stats stats = listener_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(stats.captured_frames, stats.dropped_frames);
}

TEST_F(VideoAdapterTest, TestOnOutputFormatRequest) {
  VideoFormat format(640, 400, 0, 0);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);

  // Format request 640x400.
  format.height = 400;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);

  // Request 1280x720, higher than input, but aspect 16:9. Expect cropping but
  // no scaling.
  format.width = 1280;
  format.height = 720;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Request 0x0.
  format.width = 0;
  format.height = 0;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_FALSE(adapter_.AdaptFrameResolution(640, 400, 0,
                                             &cropped_width_, &cropped_height_,
                                             &out_width_, &out_height_));

  // Request 320x200. Expect scaling, but no cropping.
  format.width = 320;
  format.height = 200;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Request resolution close to 2/3 scale. Expect adapt down. Scaling to 2/3
  // is not optimized and not allowed, therefore 1/2 scaling will be used
  // instead.
  format.width = 424;
  format.height = 265;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Request resolution of 3 / 8. Expect adapt down.
  format.width = 640 * 3 / 8;
  format.height = 400 * 3 / 8;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640 * 3 / 8, out_width_);
  EXPECT_EQ(400 * 3 / 8, out_height_);

  // Switch back up. Expect adapt.
  format.width = 320;
  format.height = 200;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Format request 480x300.
  format.width = 480;
  format.height = 300;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(300, out_height_);
}

TEST_F(VideoAdapterTest, TestViewRequestPlusCameraSwitch) {
  // Start at HD.
  VideoFormat format(1280, 720, 0, 0);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  // Format request for VGA.
  format.width = 640;
  format.height = 360;
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Now, the camera reopens at VGA.
  // Both the frame and the output format should be 640x360.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // And another view request comes in for 640x360, which should have no
  // real impact.
  adapter_.OnOutputFormatRequest(format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestVGAWidth) {
  // Reqeuested Output format is 640x360.
  VideoFormat format(640, 360, 0, FOURCC_I420);
  adapter_.OnOutputFormatRequest(format);

  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  // Expect cropping.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // But if frames come in at 640x360, we shouldn't adapt them down.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestInSmallSteps) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  // Adapt down one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(1280 * 720 - 1));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(960, out_width_);
  EXPECT_EQ(540, out_height_);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 540 - 1));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360 - 1));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt up one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360),
                               rtc::Optional<int>(960 * 540));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(960 * 540),
                               rtc::Optional<int>(1280 * 720));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(960, out_width_);
  EXPECT_EQ(540, out_height_);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(1280 * 720),
                               rtc::Optional<int>(1920 * 1080));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestMaxZero) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(), rtc::Optional<int>(0));
  EXPECT_FALSE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                             &cropped_width_, &cropped_height_,
                                             &out_width_, &out_height_));
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestInLargeSteps) {
  // Large step down.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360 - 1));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Large step up.
  adapter_.OnResolutionRequest(rtc::Optional<int>(1280 * 720),
                               rtc::Optional<int>(1920 * 1080));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_F(VideoAdapterTest, TestOnOutputFormatRequestCapsMaxResolution) {
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360 - 1));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  VideoFormat new_format(640, 360, 0, FOURCC_I420);
  adapter_.OnOutputFormatRequest(new_format);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(960 * 720));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestOnResolutionRequestReset) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360 - 1));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnResolutionRequest(rtc::Optional<int>(), rtc::Optional<int>());
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_F(VideoAdapterTest, TestCroppingWithResolutionRequest) {
  // Ask for 640x360 (16:9 aspect).
  adapter_.OnOutputFormatRequest(VideoFormat(640, 360, 0, FOURCC_I420));
  // Send 640x480 (4:3 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt down one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360 - 1));
  // Expect cropping to 16:9 format and 3/4 scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt down one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(480 * 270 - 1));
  // Expect cropping to 16:9 format and 1/2 scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(180, out_height_);

  // Adapt up one step.
  adapter_.OnResolutionRequest(rtc::Optional<int>(480 * 270),
                               rtc::Optional<int>(640 * 360));
  // Expect cropping to 16:9 format and 3/4 scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360),
                               rtc::Optional<int>(960 * 540));
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Try to adapt up one step more.
  adapter_.OnResolutionRequest(rtc::Optional<int>(960 * 540),
                               rtc::Optional<int>(1280 * 720));
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_F(VideoAdapterTest, TestCroppingOddResolution) {
  // Ask for 640x360 (16:9 aspect), with 3/16 scaling.
  adapter_.OnOutputFormatRequest(
      VideoFormat(640, 360, 0, FOURCC_I420));
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 360 * 3 / 16 * 3 / 16));

  // Send 640x480 (4:3 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0,
                                            &cropped_width_, &cropped_height_,
                                            &out_width_, &out_height_));

  // Instead of getting the exact aspect ratio with cropped resolution 640x360,
  // the resolution should be adjusted to get a perfect scale factor instead.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(368, cropped_height_);
  EXPECT_EQ(120, out_width_);
  EXPECT_EQ(69, out_height_);
}

TEST_F(VideoAdapterTest, TestAdaptToVerySmallResolution) {
  // Ask for 1920x1080 (16:9 aspect), with 1/16 scaling.
  const int w = 1920;
  const int h = 1080;
  adapter_.OnOutputFormatRequest(VideoFormat(w, h, 0, FOURCC_I420));
  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(w * h * 1 / 16 * 1 / 16));

  // Send 1920x1080 (16:9 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      w, h, 0, &cropped_width_, &cropped_height_, &out_width_, &out_height_));

  // Instead of getting the exact aspect ratio with cropped resolution 1920x1080
  // the resolution should be adjusted to get a perfect scale factor instead.
  EXPECT_EQ(1920, cropped_width_);
  EXPECT_EQ(1072, cropped_height_);
  EXPECT_EQ(120, out_width_);
  EXPECT_EQ(67, out_height_);

  // Adapt back up one step to 3/32.
  adapter_.OnResolutionRequest(rtc::Optional<int>(w * h * 3 / 32 * 3 / 32),
                               rtc::Optional<int>(w * h * 1 / 8 * 1 / 8));

  // Send 1920x1080 (16:9 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      w, h, 0, &cropped_width_, &cropped_height_, &out_width_, &out_height_));

  EXPECT_EQ(180, out_width_);
  EXPECT_EQ(99, out_height_);
}

TEST_F(VideoAdapterTest, AdaptFrameResolutionDropWithResolutionRequest) {
  VideoFormat output_format = capture_format_;
  output_format.width = 0;
  output_format.height = 0;
  adapter_.OnOutputFormatRequest(output_format);
  EXPECT_FALSE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));

  adapter_.OnResolutionRequest(rtc::Optional<int>(960 * 540),
                               rtc::Optional<int>());

  // Still expect all frames to be dropped
  EXPECT_FALSE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));

  adapter_.OnResolutionRequest(rtc::Optional<int>(),
                               rtc::Optional<int>(640 * 480 - 1));

  // Still expect all frames to be dropped
  EXPECT_FALSE(adapter_.AdaptFrameResolution(
      capture_format_.width, capture_format_.height, 0,
      &cropped_width_, &cropped_height_,
      &out_width_, &out_height_));
}

// Test that we will adapt to max given a target pixel count close to max.
TEST_F(VideoAdapterTest, TestAdaptToMax) {
  adapter_.OnOutputFormatRequest(VideoFormat(640, 360, 0, FOURCC_I420));
  adapter_.OnResolutionRequest(rtc::Optional<int>(640 * 360 - 1) /* target */,
                               rtc::Optional<int>());

  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

}  // namespace cricket
