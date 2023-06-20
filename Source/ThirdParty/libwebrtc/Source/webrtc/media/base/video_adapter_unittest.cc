/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/video_adapter.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "media/base/fake_frame_source.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/time_utils.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace cricket {
namespace {
const int kWidth = 1280;
const int kHeight = 720;
const int kDefaultFps = 30;

using ::testing::_;
using ::testing::Eq;
using ::testing::Pair;
using webrtc::Resolution;

rtc::VideoSinkWants BuildSinkWants(absl::optional<int> target_pixel_count,
                                   int max_pixel_count,
                                   int max_framerate_fps,
                                   int sink_alignment = 1) {
  rtc::VideoSinkWants wants;
  wants.target_pixel_count = target_pixel_count;
  wants.max_pixel_count = max_pixel_count;
  wants.max_framerate_fps = max_framerate_fps;
  wants.resolution_alignment = sink_alignment;
  wants.is_active = true;
  wants.aggregates.emplace(rtc::VideoSinkWants::Aggregates());
  wants.aggregates->any_active_without_requested_resolution = false;
  return wants;
}

rtc::VideoSinkWants BuildSinkWants(
    absl::optional<webrtc::Resolution> requested_resolution,
    bool any_active_without_requested_resolution) {
  rtc::VideoSinkWants wants;
  wants.max_framerate_fps = kDefaultFps;
  wants.resolution_alignment = 1;
  wants.is_active = true;
  if (requested_resolution) {
    wants.target_pixel_count = requested_resolution->PixelCount();
    wants.max_pixel_count = requested_resolution->PixelCount();
    wants.requested_resolution.emplace(rtc::VideoSinkWants::FrameSize(
        requested_resolution->width, requested_resolution->height));
  } else {
    wants.target_pixel_count = kWidth * kHeight;
    wants.max_pixel_count = kWidth * kHeight;
  }
  wants.aggregates.emplace(rtc::VideoSinkWants::Aggregates());
  wants.aggregates->any_active_without_requested_resolution =
      any_active_without_requested_resolution;
  return wants;
}

}  // namespace

class VideoAdapterTest : public ::testing::Test,
                         public ::testing::WithParamInterface<bool> {
 public:
  VideoAdapterTest() : VideoAdapterTest("", 1) {}
  explicit VideoAdapterTest(const std::string& field_trials,
                            int source_resolution_alignment)
      : override_field_trials_(field_trials),
        frame_source_(std::make_unique<FakeFrameSource>(
            kWidth,
            kHeight,
            VideoFormat::FpsToInterval(kDefaultFps) /
                rtc::kNumNanosecsPerMicrosec)),
        adapter_(source_resolution_alignment),
        adapter_wrapper_(std::make_unique<VideoAdapterWrapper>(&adapter_)),
        use_new_format_request_(GetParam()) {}

 protected:
  // Wrap a VideoAdapter and collect stats.
  class VideoAdapterWrapper {
   public:
    struct Stats {
      int captured_frames = 0;
      int dropped_frames = 0;
      bool last_adapt_was_no_op = false;

      int cropped_width = 0;
      int cropped_height = 0;
      int out_width = 0;
      int out_height = 0;
    };

    explicit VideoAdapterWrapper(VideoAdapter* adapter)
        : video_adapter_(adapter) {}

    void AdaptFrame(const webrtc::VideoFrame& frame) {
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
        stats_.cropped_width = cropped_width;
        stats_.cropped_height = cropped_height;
        stats_.out_width = out_width;
        stats_.out_height = out_height;
        stats_.last_adapt_was_no_op =
            (in_width == cropped_width && in_height == cropped_height &&
             in_width == out_width && in_height == out_height);
      } else {
        ++stats_.dropped_frames;
      }
      ++stats_.captured_frames;
    }

    Stats GetStats() const { return stats_; }

   private:
    VideoAdapter* video_adapter_;
    Stats stats_;
  };

  void VerifyAdaptedResolution(const VideoAdapterWrapper::Stats& stats,
                               int cropped_width,
                               int cropped_height,
                               int out_width,
                               int out_height) {
    EXPECT_EQ(cropped_width, stats.cropped_width);
    EXPECT_EQ(cropped_height, stats.cropped_height);
    EXPECT_EQ(out_width, stats.out_width);
    EXPECT_EQ(out_height, stats.out_height);
  }

  void OnOutputFormatRequest(int width,
                             int height,
                             const absl::optional<int>& fps) {
    if (use_new_format_request_) {
      absl::optional<std::pair<int, int>> target_aspect_ratio =
          std::make_pair(width, height);
      absl::optional<int> max_pixel_count = width * height;
      absl::optional<int> max_fps = fps;
      adapter_.OnOutputFormatRequest(target_aspect_ratio, max_pixel_count,
                                     max_fps);
      return;
    }
    adapter_.OnOutputFormatRequest(
        VideoFormat(width, height, fps ? VideoFormat::FpsToInterval(*fps) : 0,
                    cricket::FOURCC_I420));
  }

  // Return pair of <out resolution, cropping>
  std::pair<webrtc::Resolution, webrtc::Resolution> AdaptFrameResolution(
      webrtc::Resolution res) {
    webrtc::Resolution out;
    webrtc::Resolution cropped;
    timestamp_ns_ += 1000000000;
    EXPECT_TRUE(adapter_.AdaptFrameResolution(
        res.width, res.height, timestamp_ns_, &cropped.width, &cropped.height,
        &out.width, &out.height));
    return std::make_pair(out, cropped);
  }

  webrtc::test::ScopedFieldTrials override_field_trials_;
  const std::unique_ptr<FakeFrameSource> frame_source_;
  VideoAdapter adapter_;
  int64_t timestamp_ns_ = 0;
  int cropped_width_;
  int cropped_height_;
  int out_width_;
  int out_height_;
  const std::unique_ptr<VideoAdapterWrapper> adapter_wrapper_;
  const bool use_new_format_request_;
};

INSTANTIATE_TEST_SUITE_P(OnOutputFormatRequests,
                         VideoAdapterTest,
                         ::testing::Values(true, false));

// Do not adapt the frame rate or the resolution. Expect no frame drop, no
// cropping, and no resolution change.
TEST_P(VideoAdapterTest, AdaptNothing) {
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no frame drop and no resolution change.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, kWidth, kHeight, kWidth, kHeight);
  EXPECT_TRUE(stats.last_adapt_was_no_op);
}

TEST_P(VideoAdapterTest, AdaptZeroInterval) {
  OnOutputFormatRequest(kWidth, kHeight, absl::nullopt);
  for (int i = 0; i < 40; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no crash and that frames aren't dropped.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_GE(stats.captured_frames, 40);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, kWidth, kHeight, kWidth, kHeight);
}

// Adapt the frame rate to be half of the capture rate at the beginning. Expect
// the number of dropped frames to be half of the number the captured frames.
TEST_P(VideoAdapterTest, AdaptFramerateToHalf) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps / 2);

  // Capture 10 frames and verify that every other frame is dropped. The first
  // frame should not be dropped.
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());
  EXPECT_GE(adapter_wrapper_->GetStats().captured_frames, 10);
  EXPECT_EQ(5, adapter_wrapper_->GetStats().dropped_frames);
}

// Adapt the frame rate to be two thirds of the capture rate at the beginning.
// Expect the number of dropped frames to be one thirds of the number the
// captured frames.
TEST_P(VideoAdapterTest, AdaptFramerateToTwoThirds) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps * 2 / 3);

  // Capture 10 frames and verify that every third frame is dropped. The first
  // frame should not be dropped.
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());
  EXPECT_GE(adapter_wrapper_->GetStats().captured_frames, 10);
  EXPECT_EQ(3, adapter_wrapper_->GetStats().dropped_frames);
}

// Request frame rate twice as high as captured frame rate. Expect no frame
// drop.
TEST_P(VideoAdapterTest, AdaptFramerateHighLimit) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps * 2);

  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no frame drop.
  EXPECT_EQ(0, adapter_wrapper_->GetStats().dropped_frames);
}

// Adapt the frame rate to be half of the capture rate. No resolution limit set.
// Expect the number of dropped frames to be half of the number the captured
// frames.
TEST_P(VideoAdapterTest, AdaptFramerateToHalfWithNoPixelLimit) {
  adapter_.OnOutputFormatRequest(absl::nullopt, absl::nullopt, kDefaultFps / 2);

  // Capture 10 frames and verify that every other frame is dropped. The first
  // frame should not be dropped.
  int expected_dropped_frames = 0;
  for (int i = 0; i < 10; ++i) {
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());
    EXPECT_GE(adapter_wrapper_->GetStats().captured_frames, i + 1);
    if (i % 2 == 1)
      ++expected_dropped_frames;
    EXPECT_EQ(expected_dropped_frames,
              adapter_wrapper_->GetStats().dropped_frames);
    VerifyAdaptedResolution(adapter_wrapper_->GetStats(), kWidth, kHeight,
                            kWidth, kHeight);
  }
}

// Adapt the frame rate to be half of the capture rate after capturing no less
// than 10 frames. Expect no frame dropped before adaptation and frame dropped
// after adaptation.
TEST_P(VideoAdapterTest, AdaptFramerateOntheFly) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps);
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no frame drop before adaptation.
  EXPECT_EQ(0, adapter_wrapper_->GetStats().dropped_frames);

  // Adapt the frame rate.
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps / 2);
  for (int i = 0; i < 20; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify frame drop after adaptation.
  EXPECT_GT(adapter_wrapper_->GetStats().dropped_frames, 0);
}

// Do not adapt the frame rate or the resolution. Expect no frame drop, no
// cropping, and no resolution change.
TEST_P(VideoAdapterTest, AdaptFramerateRequestMax) {
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt,
                                      std::numeric_limits<int>::max(),
                                      std::numeric_limits<int>::max()));

  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no frame drop and no resolution change.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, kWidth, kHeight, kWidth, kHeight);
  EXPECT_TRUE(stats.last_adapt_was_no_op);
}

TEST_P(VideoAdapterTest, AdaptFramerateRequestZero) {
  adapter_.OnSinkWants(
      BuildSinkWants(absl::nullopt, std::numeric_limits<int>::max(), 0));
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no crash and that frames aren't dropped.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(10, stats.dropped_frames);
}

// Adapt the frame rate to be half of the capture rate at the beginning. Expect
// the number of dropped frames to be half of the number the captured frames.
TEST_P(VideoAdapterTest, AdaptFramerateRequestHalf) {
  adapter_.OnSinkWants(BuildSinkWants(
      absl::nullopt, std::numeric_limits<int>::max(), kDefaultFps / 2));
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no crash and that frames aren't dropped.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(5, stats.dropped_frames);
  VerifyAdaptedResolution(stats, kWidth, kHeight, kWidth, kHeight);
}

// Set a very high output pixel resolution. Expect no cropping or resolution
// change.
TEST_P(VideoAdapterTest, AdaptFrameResolutionHighLimit) {
  OnOutputFormatRequest(kWidth * 10, kHeight * 10, kDefaultFps);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(kWidth, cropped_width_);
  EXPECT_EQ(kHeight, cropped_height_);
  EXPECT_EQ(kWidth, out_width_);
  EXPECT_EQ(kHeight, out_height_);
}

// Adapt the frame resolution to be the same as capture resolution. Expect no
// cropping or resolution change.
TEST_P(VideoAdapterTest, AdaptFrameResolutionIdentical) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(kWidth, cropped_width_);
  EXPECT_EQ(kHeight, cropped_height_);
  EXPECT_EQ(kWidth, out_width_);
  EXPECT_EQ(kHeight, out_height_);
}

// Adapt the frame resolution to be a quarter of the capture resolution. Expect
// no cropping, but a resolution change.
TEST_P(VideoAdapterTest, AdaptFrameResolutionQuarter) {
  OnOutputFormatRequest(kWidth / 2, kHeight / 2, kDefaultFps);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(kWidth, cropped_width_);
  EXPECT_EQ(kHeight, cropped_height_);
  EXPECT_EQ(kWidth / 2, out_width_);
  EXPECT_EQ(kHeight / 2, out_height_);
}

// Adapt the pixel resolution to 0. Expect frame drop.
TEST_P(VideoAdapterTest, AdaptFrameResolutionDrop) {
  OnOutputFormatRequest(kWidth * 0, kHeight * 0, kDefaultFps);
  EXPECT_FALSE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                             &cropped_width_, &cropped_height_,
                                             &out_width_, &out_height_));
}

// Adapt the frame resolution to be a quarter of the capture resolution at the
// beginning. Expect no cropping but a resolution change.
TEST_P(VideoAdapterTest, AdaptResolution) {
  OnOutputFormatRequest(kWidth / 2, kHeight / 2, kDefaultFps);
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no frame drop, no cropping, and resolution change.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_EQ(0, stats.dropped_frames);
  VerifyAdaptedResolution(stats, kWidth, kHeight, kWidth / 2, kHeight / 2);
}

// Adapt the frame resolution to be a quarter of the capture resolution after
// capturing no less than 10 frames. Expect no resolution change before
// adaptation and resolution change after adaptation.
TEST_P(VideoAdapterTest, AdaptResolutionOnTheFly) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps);
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no resolution change before adaptation.
  VerifyAdaptedResolution(adapter_wrapper_->GetStats(), kWidth, kHeight, kWidth,
                          kHeight);

  // Adapt the frame resolution.
  OnOutputFormatRequest(kWidth / 2, kHeight / 2, kDefaultFps);
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify resolution change after adaptation.
  VerifyAdaptedResolution(adapter_wrapper_->GetStats(), kWidth, kHeight,
                          kWidth / 2, kHeight / 2);
}

// Drop all frames for resolution 0x0.
TEST_P(VideoAdapterTest, DropAllFrames) {
  OnOutputFormatRequest(kWidth * 0, kHeight * 0, kDefaultFps);
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify all frames are dropped.
  VideoAdapterWrapper::Stats stats = adapter_wrapper_->GetStats();
  EXPECT_GE(stats.captured_frames, 10);
  EXPECT_EQ(stats.captured_frames, stats.dropped_frames);
}

TEST_P(VideoAdapterTest, TestOnOutputFormatRequest) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);

  // Format request 640x400.
  OnOutputFormatRequest(640, 400, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);

  // Request 1280x720, higher than input, but aspect 16:9. Expect cropping but
  // no scaling.
  OnOutputFormatRequest(1280, 720, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Request 0x0.
  OnOutputFormatRequest(0, 0, absl::nullopt);
  EXPECT_FALSE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                             &cropped_height_, &out_width_,
                                             &out_height_));

  // Request 320x200. Expect scaling, but no cropping.
  OnOutputFormatRequest(320, 200, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Request resolution close to 2/3 scale. Expect adapt down. Scaling to 2/3
  // is not optimized and not allowed, therefore 1/2 scaling will be used
  // instead.
  OnOutputFormatRequest(424, 265, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Request resolution of 3 / 8. Expect adapt down.
  OnOutputFormatRequest(640 * 3 / 8, 400 * 3 / 8, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640 * 3 / 8, out_width_);
  EXPECT_EQ(400 * 3 / 8, out_height_);

  // Switch back up. Expect adapt.
  OnOutputFormatRequest(320, 200, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(200, out_height_);

  // Format request 480x300.
  OnOutputFormatRequest(480, 300, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(300, out_height_);
}

TEST_P(VideoAdapterTest, TestViewRequestPlusCameraSwitch) {
  // Start at HD.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  // Format request for VGA.
  OnOutputFormatRequest(640, 360, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Now, the camera reopens at VGA.
  // Both the frame and the output format should be 640x360.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // And another view request comes in for 640x360, which should have no
  // real impact.
  OnOutputFormatRequest(640, 360, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_P(VideoAdapterTest, TestVgaWidth) {
  // Requested output format is 640x360.
  OnOutputFormatRequest(640, 360, absl::nullopt);

  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  // Expect cropping.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // But if frames come in at 640x360, we shouldn't adapt them down.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_P(VideoAdapterTest, TestOnResolutionRequestInSmallSteps) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  // Adapt down one step.
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 1280 * 720 - 1,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(960, out_width_);
  EXPECT_EQ(540, out_height_);

  // Adapt down one step more.
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 960 * 540 - 1,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt down one step more.
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 640 * 360 - 1,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt up one step.
  adapter_.OnSinkWants(
      BuildSinkWants(640 * 360, 960 * 540, std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt up one step more.
  adapter_.OnSinkWants(
      BuildSinkWants(960 * 540, 1280 * 720, std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(960, out_width_);
  EXPECT_EQ(540, out_height_);

  // Adapt up one step more.
  adapter_.OnSinkWants(
      BuildSinkWants(1280 * 720, 1920 * 1080, std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_P(VideoAdapterTest, TestOnResolutionRequestMaxZero) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnSinkWants(
      BuildSinkWants(absl::nullopt, 0, std::numeric_limits<int>::max()));
  EXPECT_FALSE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                             &cropped_height_, &out_width_,
                                             &out_height_));
}

TEST_P(VideoAdapterTest, TestOnResolutionRequestInLargeSteps) {
  // Large step down.
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 640 * 360 - 1,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Large step up.
  adapter_.OnSinkWants(
      BuildSinkWants(1280 * 720, 1920 * 1080, std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_P(VideoAdapterTest, TestOnOutputFormatRequestCapsMaxResolution) {
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 640 * 360 - 1,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  OnOutputFormatRequest(640, 360, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 960 * 720,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_P(VideoAdapterTest, TestOnResolutionRequestReset) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 640 * 360 - 1,
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt,
                                      std::numeric_limits<int>::max(),
                                      std::numeric_limits<int>::max()));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_P(VideoAdapterTest, TestOnOutputFormatRequestResolutionReset) {
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);

  adapter_.OnOutputFormatRequest(absl::nullopt, 640 * 360 - 1, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  adapter_.OnOutputFormatRequest(absl::nullopt, absl::nullopt, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(1280, 720, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(1280, cropped_width_);
  EXPECT_EQ(720, cropped_height_);
  EXPECT_EQ(1280, out_width_);
  EXPECT_EQ(720, out_height_);
}

TEST_P(VideoAdapterTest, TestOnOutputFormatRequestFpsReset) {
  OnOutputFormatRequest(kWidth, kHeight, kDefaultFps / 2);
  for (int i = 0; i < 10; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify frame drop.
  const int dropped_frames = adapter_wrapper_->GetStats().dropped_frames;
  EXPECT_GT(dropped_frames, 0);

  // Reset frame rate.
  OnOutputFormatRequest(kWidth, kHeight, absl::nullopt);
  for (int i = 0; i < 20; ++i)
    adapter_wrapper_->AdaptFrame(frame_source_->GetFrame());

  // Verify no frame drop after reset.
  EXPECT_EQ(dropped_frames, adapter_wrapper_->GetStats().dropped_frames);
}

TEST_P(VideoAdapterTest, RequestAspectRatio) {
  // Request aspect ratio 320/180 (16:9), smaller than input, but no resolution
  // limit. Expect cropping but no scaling.
  adapter_.OnOutputFormatRequest(std::make_pair(320, 180), absl::nullopt,
                                 absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  adapter_.OnOutputFormatRequest(std::make_pair(1280, 720), 1280 * 720 - 1,
                                 absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(2592, 1944, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(2592, cropped_width_);
  EXPECT_EQ(1458, cropped_height_);
  EXPECT_EQ(1152, out_width_);
  EXPECT_EQ(648, out_height_);
}

TEST_P(VideoAdapterTest, RequestAspectRatioWithDifferentOrientation) {
  // Request 720x1280, higher than input, but aspect 16:9. Orientation should
  // not matter, expect cropping but no scaling.
  OnOutputFormatRequest(720, 1280, absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_P(VideoAdapterTest, InvalidAspectRatioIgnored) {
  // Request aspect ratio 320/0. Expect no cropping.
  adapter_.OnOutputFormatRequest(std::make_pair(320, 0), absl::nullopt,
                                 absl::nullopt);
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 400, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(400, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(400, out_height_);
}

TEST_P(VideoAdapterTest, TestCroppingWithResolutionRequest) {
  // Ask for 640x360 (16:9 aspect).
  OnOutputFormatRequest(640, 360, absl::nullopt);
  // Send 640x480 (4:3 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Adapt down one step.
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 640 * 360 - 1,
                                      std::numeric_limits<int>::max()));
  // Expect cropping to 16:9 format and 3/4 scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt down one step more.
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 480 * 270 - 1,
                                      std::numeric_limits<int>::max()));
  // Expect cropping to 16:9 format and 1/2 scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(320, out_width_);
  EXPECT_EQ(180, out_height_);

  // Adapt up one step.
  adapter_.OnSinkWants(
      BuildSinkWants(480 * 270, 640 * 360, std::numeric_limits<int>::max()));
  // Expect cropping to 16:9 format and 3/4 scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(480, out_width_);
  EXPECT_EQ(270, out_height_);

  // Adapt up one step more.
  adapter_.OnSinkWants(
      BuildSinkWants(640 * 360, 960 * 540, std::numeric_limits<int>::max()));
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);

  // Try to adapt up one step more.
  adapter_.OnSinkWants(
      BuildSinkWants(960 * 540, 1280 * 720, std::numeric_limits<int>::max()));
  // Expect cropping to 16:9 format and no scaling.
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(360, cropped_height_);
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

TEST_P(VideoAdapterTest, TestCroppingOddResolution) {
  // Ask for 640x360 (16:9 aspect), with 3/16 scaling.
  OnOutputFormatRequest(640, 360, absl::nullopt);
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt,
                                      640 * 360 * 3 / 16 * 3 / 16,
                                      std::numeric_limits<int>::max()));

  // Send 640x480 (4:3 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 480, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));

  // Instead of getting the exact aspect ratio with cropped resolution 640x360,
  // the resolution should be adjusted to get a perfect scale factor instead.
  EXPECT_EQ(640, cropped_width_);
  EXPECT_EQ(368, cropped_height_);
  EXPECT_EQ(120, out_width_);
  EXPECT_EQ(69, out_height_);
}

TEST_P(VideoAdapterTest, TestAdaptToVerySmallResolution) {
  // Ask for 1920x1080 (16:9 aspect), with 1/16 scaling.
  const int w = 1920;
  const int h = 1080;
  OnOutputFormatRequest(w, h, absl::nullopt);
  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, w * h * 1 / 16 * 1 / 16,
                                      std::numeric_limits<int>::max()));

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
  adapter_.OnSinkWants(BuildSinkWants(w * h * 3 / 32 * 3 / 32,
                                      w * h * 1 / 8 * 1 / 8,
                                      std::numeric_limits<int>::max()));

  // Send 1920x1080 (16:9 aspect).
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      w, h, 0, &cropped_width_, &cropped_height_, &out_width_, &out_height_));

  EXPECT_EQ(160, out_width_);
  EXPECT_EQ(90, out_height_);
}

TEST_P(VideoAdapterTest, AdaptFrameResolutionDropWithResolutionRequest) {
  OnOutputFormatRequest(0, 0, kDefaultFps);
  EXPECT_FALSE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                             &cropped_width_, &cropped_height_,
                                             &out_width_, &out_height_));

  adapter_.OnSinkWants(BuildSinkWants(960 * 540,
                                      std::numeric_limits<int>::max(),
                                      std::numeric_limits<int>::max()));

  // Still expect all frames to be dropped
  EXPECT_FALSE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                             &cropped_width_, &cropped_height_,
                                             &out_width_, &out_height_));

  adapter_.OnSinkWants(BuildSinkWants(absl::nullopt, 640 * 480 - 1,
                                      std::numeric_limits<int>::max()));

  // Still expect all frames to be dropped
  EXPECT_FALSE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                             &cropped_width_, &cropped_height_,
                                             &out_width_, &out_height_));
}

// Test that we will adapt to max given a target pixel count close to max.
TEST_P(VideoAdapterTest, TestAdaptToMax) {
  OnOutputFormatRequest(640, 360, kDefaultFps);
  adapter_.OnSinkWants(BuildSinkWants(640 * 360 - 1 /* target */,
                                      std::numeric_limits<int>::max(),
                                      std::numeric_limits<int>::max()));

  EXPECT_TRUE(adapter_.AdaptFrameResolution(640, 360, 0, &cropped_width_,
                                            &cropped_height_, &out_width_,
                                            &out_height_));
  EXPECT_EQ(640, out_width_);
  EXPECT_EQ(360, out_height_);
}

// Test adjusting to 16:9 in landscape, and 9:16 in portrait.
TEST(VideoAdapterTestMultipleOrientation, TestNormal) {
  VideoAdapter video_adapter;
  video_adapter.OnOutputFormatRequest(std::make_pair(640, 360), 640 * 360,
                                      std::make_pair(360, 640), 360 * 640, 30);

  int cropped_width;
  int cropped_height;
  int out_width;
  int out_height;
  EXPECT_TRUE(video_adapter.AdaptFrameResolution(
      /* in_width= */ 640, /* in_height= */ 480, /* in_timestamp_ns= */ 0,
      &cropped_width, &cropped_height, &out_width, &out_height));
  EXPECT_EQ(640, cropped_width);
  EXPECT_EQ(360, cropped_height);
  EXPECT_EQ(640, out_width);
  EXPECT_EQ(360, out_height);

  EXPECT_TRUE(video_adapter.AdaptFrameResolution(
      /* in_width= */ 480, /* in_height= */ 640,
      /* in_timestamp_ns= */ rtc::kNumNanosecsPerSec / 30, &cropped_width,
      &cropped_height, &out_width, &out_height));
  EXPECT_EQ(360, cropped_width);
  EXPECT_EQ(640, cropped_height);
  EXPECT_EQ(360, out_width);
  EXPECT_EQ(640, out_height);
}

// Force output to be 9:16, even for landscape input.
TEST(VideoAdapterTestMultipleOrientation, TestForcePortrait) {
  VideoAdapter video_adapter;
  video_adapter.OnOutputFormatRequest(std::make_pair(360, 640), 640 * 360,
                                      std::make_pair(360, 640), 360 * 640, 30);

  int cropped_width;
  int cropped_height;
  int out_width;
  int out_height;
  EXPECT_TRUE(video_adapter.AdaptFrameResolution(
      /* in_width= */ 640, /* in_height= */ 480, /* in_timestamp_ns= */ 0,
      &cropped_width, &cropped_height, &out_width, &out_height));
  EXPECT_EQ(270, cropped_width);
  EXPECT_EQ(480, cropped_height);
  EXPECT_EQ(270, out_width);
  EXPECT_EQ(480, out_height);

  EXPECT_TRUE(video_adapter.AdaptFrameResolution(
      /* in_width= */ 480, /* in_height= */ 640,
      /* in_timestamp_ns= */ rtc::kNumNanosecsPerSec / 30, &cropped_width,
      &cropped_height, &out_width, &out_height));
  EXPECT_EQ(360, cropped_width);
  EXPECT_EQ(640, cropped_height);
  EXPECT_EQ(360, out_width);
  EXPECT_EQ(640, out_height);
}

TEST_P(VideoAdapterTest, AdaptResolutionInStepsFirst3_4) {
  const int kWidth = 1280;
  const int kHeight = 720;
  OnOutputFormatRequest(kWidth, kHeight, absl::nullopt);  // 16:9 aspect.

  // Scale factors: 3/4, 2/3, 3/4, 2/3, ...
  // Scale        : 3/4, 1/2, 3/8, 1/4, 3/16, 1/8.
  const int kExpectedWidths[] = {960, 640, 480, 320, 240, 160};
  const int kExpectedHeights[] = {540, 360, 270, 180, 135, 90};

  int request_width = kWidth;
  int request_height = kHeight;

  for (size_t i = 0; i < arraysize(kExpectedWidths); ++i) {
    // Adapt down one step.
    adapter_.OnSinkWants(BuildSinkWants(absl::nullopt,
                                        request_width * request_height - 1,
                                        std::numeric_limits<int>::max()));
    EXPECT_TRUE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                              &cropped_width_, &cropped_height_,
                                              &out_width_, &out_height_));
    EXPECT_EQ(kExpectedWidths[i], out_width_);
    EXPECT_EQ(kExpectedHeights[i], out_height_);
    request_width = out_width_;
    request_height = out_height_;
  }
}

TEST_P(VideoAdapterTest, AdaptResolutionInStepsFirst2_3) {
  const int kWidth = 1920;
  const int kHeight = 1080;
  OnOutputFormatRequest(kWidth, kHeight, absl::nullopt);  // 16:9 aspect.

  // Scale factors: 2/3, 3/4, 2/3, 3/4, ...
  // Scale:         2/3, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12.
  const int kExpectedWidths[] = {1280, 960, 640, 480, 320, 240, 160};
  const int kExpectedHeights[] = {720, 540, 360, 270, 180, 135, 90};

  int request_width = kWidth;
  int request_height = kHeight;

  for (size_t i = 0; i < arraysize(kExpectedWidths); ++i) {
    // Adapt down one step.
    adapter_.OnSinkWants(BuildSinkWants(absl::nullopt,
                                        request_width * request_height - 1,
                                        std::numeric_limits<int>::max()));
    EXPECT_TRUE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                              &cropped_width_, &cropped_height_,
                                              &out_width_, &out_height_));
    EXPECT_EQ(kExpectedWidths[i], out_width_);
    EXPECT_EQ(kExpectedHeights[i], out_height_);
    request_width = out_width_;
    request_height = out_height_;
  }
}

TEST_P(VideoAdapterTest, AdaptResolutionInStepsFirst2x2_3) {
  const int kWidth = 1440;
  const int kHeight = 1080;
  OnOutputFormatRequest(kWidth, kHeight, absl::nullopt);  // 4:3 aspect.

  // Scale factors: 2/3, 2/3, 3/4, 2/3, 3/4, ...
  // Scale        : 2/3, 4/9, 1/3, 2/9, 1/6, 1/9, 1/12, 1/18, 1/24, 1/36.
  const int kExpectedWidths[] = {960, 640, 480, 320, 240, 160, 120, 80, 60, 40};
  const int kExpectedHeights[] = {720, 480, 360, 240, 180, 120, 90, 60, 45, 30};

  int request_width = kWidth;
  int request_height = kHeight;

  for (size_t i = 0; i < arraysize(kExpectedWidths); ++i) {
    // Adapt down one step.
    adapter_.OnSinkWants(BuildSinkWants(absl::nullopt,
                                        request_width * request_height - 1,
                                        std::numeric_limits<int>::max()));
    EXPECT_TRUE(adapter_.AdaptFrameResolution(kWidth, kHeight, 0,
                                              &cropped_width_, &cropped_height_,
                                              &out_width_, &out_height_));
    EXPECT_EQ(kExpectedWidths[i], out_width_);
    EXPECT_EQ(kExpectedHeights[i], out_height_);
    request_width = out_width_;
    request_height = out_height_;
  }
}

TEST_P(VideoAdapterTest, AdaptResolutionWithSinkAlignment) {
  constexpr int kSourceWidth = 1280;
  constexpr int kSourceHeight = 720;
  constexpr int kSourceFramerate = 30;
  constexpr int kRequestedWidth = 480;
  constexpr int kRequestedHeight = 270;
  constexpr int kRequestedFramerate = 30;

  OnOutputFormatRequest(kRequestedWidth, kRequestedHeight, kRequestedFramerate);

  int frame_num = 1;
  for (const int sink_alignment : {2, 3, 4, 5}) {
    adapter_.OnSinkWants(
        BuildSinkWants(absl::nullopt, std::numeric_limits<int>::max(),
                       std::numeric_limits<int>::max(), sink_alignment));
    EXPECT_TRUE(adapter_.AdaptFrameResolution(
        kSourceWidth, kSourceHeight,
        frame_num * rtc::kNumNanosecsPerSec / kSourceFramerate, &cropped_width_,
        &cropped_height_, &out_width_, &out_height_));
    EXPECT_EQ(out_width_ % sink_alignment, 0);
    EXPECT_EQ(out_height_ % sink_alignment, 0);

    ++frame_num;
  }
}

// Verify the cases the OnOutputFormatRequest is ignored and
// requested_resolution is used instead.
TEST_P(VideoAdapterTest, UseRequestedResolutionInsteadOfOnOutputFormatRequest) {
  {
    // Both new and old API active => Use OnOutputFormatRequest
    OnOutputFormatRequest(640, 360, kDefaultFps);
    adapter_.OnSinkWants(
        BuildSinkWants(Resolution{.width = 960, .height = 540},
                       /* any_active_without_requested_resolution= */ true));

    EXPECT_THAT(
        AdaptFrameResolution(/* input frame */ {.width = 1280, .height = 720})
            .first,
        Eq(Resolution{.width = 640, .height = 360}));
  }
  {
    // New API active, old API inactive, ignore OnOutputFormatRequest and use
    // requested_resolution.
    OnOutputFormatRequest(640, 360, kDefaultFps);
    adapter_.OnSinkWants(
        BuildSinkWants(Resolution{.width = 960, .height = 540},
                       /* any_active_without_requested_resolution= */ false));

    EXPECT_THAT(
        AdaptFrameResolution(/* input frame */ {.width = 1280, .height = 720})
            .first,
        Eq(Resolution{.width = 960, .height = 540}));
  }

  {
    // New API inactive, old API inactive, use OnOutputFormatRequest.
    OnOutputFormatRequest(640, 360, kDefaultFps);
    adapter_.OnSinkWants(
        BuildSinkWants(absl::nullopt,
                       /* any_active_without_requested_resolution= */ false));

    EXPECT_THAT(
        AdaptFrameResolution(/* input frame */ {.width = 1280, .height = 720})
            .first,
        Eq(Resolution{.width = 640, .height = 360}));
  }

  {
    // New API active, old API inactive, remember OnOutputFormatRequest.
    OnOutputFormatRequest(640, 360, kDefaultFps);
    adapter_.OnSinkWants(
        BuildSinkWants(Resolution{.width = 960, .height = 540},
                       /* any_active_without_requested_resolution= */ false));

    EXPECT_THAT(
        AdaptFrameResolution(/* input frame */ {.width = 1280, .height = 720})
            .first,
        Eq(Resolution{.width = 960, .height = 540}));

    // This is ignored since there is not any active NOT using
    // requested_resolution.
    OnOutputFormatRequest(320, 180, kDefaultFps);

    EXPECT_THAT(
        AdaptFrameResolution(/* input frame */ {.width = 1280, .height = 720})
            .first,
        Eq(Resolution{.width = 960, .height = 540}));

    // Disable new API => fallback to last OnOutputFormatRequest.
    adapter_.OnSinkWants(
        BuildSinkWants(absl::nullopt,
                       /* any_active_without_requested_resolution= */ false));

    EXPECT_THAT(
        AdaptFrameResolution(/* input frame */ {.width = 1280, .height = 720})
            .first,
        Eq(Resolution{.width = 320, .height = 180}));
  }
}

class VideoAdapterWithSourceAlignmentTest : public VideoAdapterTest {
 protected:
  static constexpr int kSourceResolutionAlignment = 7;

  VideoAdapterWithSourceAlignmentTest()
      : VideoAdapterTest(/*field_trials=*/"", kSourceResolutionAlignment) {}
};

TEST_P(VideoAdapterWithSourceAlignmentTest, AdaptResolution) {
  constexpr int kSourceWidth = 1280;
  constexpr int kSourceHeight = 720;
  constexpr int kRequestedWidth = 480;
  constexpr int kRequestedHeight = 270;
  constexpr int kRequestedFramerate = 30;

  OnOutputFormatRequest(kRequestedWidth, kRequestedHeight, kRequestedFramerate);

  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      kSourceWidth, kSourceHeight, /*in_timestamp_ns=*/0, &cropped_width_,
      &cropped_height_, &out_width_, &out_height_));
  EXPECT_EQ(out_width_ % kSourceResolutionAlignment, 0);
  EXPECT_EQ(out_height_ % kSourceResolutionAlignment, 0);
}

TEST_P(VideoAdapterWithSourceAlignmentTest, AdaptResolutionWithSinkAlignment) {
  constexpr int kSourceWidth = 1280;
  constexpr int kSourceHeight = 720;
  // 7 and 8 neither divide 480 nor 270.
  constexpr int kRequestedWidth = 480;
  constexpr int kRequestedHeight = 270;
  constexpr int kRequestedFramerate = 30;
  constexpr int kSinkResolutionAlignment = 8;

  OnOutputFormatRequest(kRequestedWidth, kRequestedHeight, kRequestedFramerate);

  adapter_.OnSinkWants(BuildSinkWants(
      absl::nullopt, std::numeric_limits<int>::max(),
      std::numeric_limits<int>::max(), kSinkResolutionAlignment));
  EXPECT_TRUE(adapter_.AdaptFrameResolution(
      kSourceWidth, kSourceHeight, /*in_timestamp_ns=*/0, &cropped_width_,
      &cropped_height_, &out_width_, &out_height_));
  EXPECT_EQ(out_width_ % kSourceResolutionAlignment, 0);
  EXPECT_EQ(out_height_ % kSourceResolutionAlignment, 0);
  EXPECT_EQ(out_width_ % kSinkResolutionAlignment, 0);
  EXPECT_EQ(out_height_ % kSinkResolutionAlignment, 0);
}

INSTANTIATE_TEST_SUITE_P(OnOutputFormatRequests,
                         VideoAdapterWithSourceAlignmentTest,
                         ::testing::Values(true, false));

}  // namespace cricket
