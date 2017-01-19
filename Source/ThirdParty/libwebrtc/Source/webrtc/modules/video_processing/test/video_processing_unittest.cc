/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/test/video_processing_unittest.h"

#include <gflags/gflags.h>

#include <memory>
#include <string>

#include "webrtc/base/keep_ref_until_done.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

namespace {

// Define command line flag 'gen_files' (default value: false).
DEFINE_bool(gen_files, false, "Output files for visual inspection.");

}  // namespace

static void PreprocessFrameAndVerify(const VideoFrame& source,
                                     int target_width,
                                     int target_height,
                                     VideoProcessing* vpm,
                                     const VideoFrame** out_frame);
static rtc::scoped_refptr<VideoFrameBuffer> CropBuffer(
    rtc::scoped_refptr<VideoFrameBuffer> source_buffer,
    int offset_x,
    int offset_y,
    int cropped_width,
    int cropped_height);
// The |source_data| is cropped and scaled to |target_width| x |target_height|,
// and then scaled back to the expected cropped size. |expected_psnr| is used to
// verify basic quality, and is set to be ~0.1/0.05dB lower than actual PSNR
// verified under the same conditions.
static void TestSize(
    const VideoFrame& source_frame,
    const VideoFrameBuffer& cropped_source_buffer,
    int target_width,
    int target_height,
    double expected_psnr,
    VideoProcessing* vpm);
static void WriteProcessedFrameForVisualInspection(const VideoFrame& source,
                                                   const VideoFrame& processed);

VideoProcessingTest::VideoProcessingTest()
    : vp_(NULL),
      source_file_(NULL),
      width_(352),
      half_width_((width_ + 1) / 2),
      height_(288),
      size_y_(width_ * height_),
      size_uv_(half_width_ * ((height_ + 1) / 2)),
      frame_length_(CalcBufferSize(kI420, width_, height_)) {}

void VideoProcessingTest::SetUp() {
  vp_ = VideoProcessing::Create();
  ASSERT_TRUE(vp_ != NULL);

  const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  source_file_ = fopen(video_file.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL)
      << "Cannot read source file: " + video_file + "\n";
}

void VideoProcessingTest::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
  delete vp_;
  vp_ = NULL;
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_PreprocessorLogic) {
#else
TEST_F(VideoProcessingTest, PreprocessorLogic) {
#endif
  // Disable temporal sampling (frame dropping).
  vp_->EnableTemporalDecimation(false);
  int resolution = 100;
  EXPECT_EQ(VPM_OK, vp_->SetTargetResolution(resolution, resolution, 15));
  EXPECT_EQ(VPM_OK, vp_->SetTargetResolution(resolution, resolution, 30));
  // Disable spatial sampling.
  vp_->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_EQ(VPM_OK, vp_->SetTargetResolution(resolution, resolution, 30));
  const VideoFrame* out_frame = NULL;
  // Set rescaling => output frame != NULL.
  vp_->SetInputFrameResampleMode(kFastRescaling);

  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      I420Buffer::Create(width_, height_, width_, half_width_, half_width_);

  // Clear video frame so DrMemory/Valgrind will allow reads of the buffer.
  buffer->InitializeData();
  VideoFrame video_frame(buffer, 0, 0, webrtc::kVideoRotation_0);

  PreprocessFrameAndVerify(video_frame, resolution, resolution, vp_,
                           &out_frame);
  // No rescaling=> output frame = NULL.
  vp_->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_TRUE(vp_->PreprocessFrame(video_frame) != nullptr);
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_Resampler) {
#else
TEST_F(VideoProcessingTest, Resampler) {
#endif
  enum { NumRuns = 1 };

  int64_t min_runtime = 0;
  int64_t total_runtime = 0;

  rewind(source_file_);
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read input file \n";

  // no temporal decimation
  vp_->EnableTemporalDecimation(false);

  // Reading test frame
  rtc::scoped_refptr<VideoFrameBuffer> video_buffer(
      test::ReadI420Buffer(width_, height_, source_file_));
  ASSERT_TRUE(video_buffer);

  for (uint32_t run_idx = 0; run_idx < NumRuns; run_idx++) {
    // Initiate test timer.
    const int64_t time_start = rtc::TimeNanos();

    // Init the sourceFrame with a timestamp.
    int64_t time_start_ms = time_start / rtc::kNumNanosecsPerMillisec;
    VideoFrame video_frame(video_buffer, time_start_ms * 90, time_start_ms,
                           webrtc::kVideoRotation_0);

    // Test scaling to different sizes: source is of |width|/|height| = 352/288.
    // Pure scaling:
    TestSize(video_frame, *video_buffer, width_ / 4, height_ / 4, 25.2, vp_);
    TestSize(video_frame, *video_buffer, width_ / 2, height_ / 2, 28.1, vp_);
    // No resampling:
    TestSize(video_frame, *video_buffer, width_, height_, -1, vp_);
    TestSize(video_frame, *video_buffer, 2 * width_, 2 * height_, 32.2, vp_);

    // Scaling and cropping. The cropped source frame is the largest center
    // aligned region that can be used from the source while preserving aspect
    // ratio.
    TestSize(video_frame, *CropBuffer(video_buffer, 0, 56, 352, 176),
             100, 50, 24.0, vp_);
    TestSize(video_frame, *CropBuffer(video_buffer, 0, 30, 352, 225),
             400, 256, 31.3, vp_);
    TestSize(video_frame, *CropBuffer(video_buffer, 68, 0, 216, 288),
             480, 640, 32.15, vp_);
    TestSize(video_frame, *CropBuffer(video_buffer, 0, 12, 352, 264),
             960, 720, 32.2, vp_);
    TestSize(video_frame, *CropBuffer(video_buffer, 0, 44, 352, 198),
             1280, 720, 32.15, vp_);

    // Upsampling to odd size.
    TestSize(video_frame, *CropBuffer(video_buffer, 0, 26, 352, 233),
             501, 333, 32.05, vp_);
    // Downsample to odd size.
    TestSize(video_frame, *CropBuffer(video_buffer, 0, 34, 352, 219),
             281, 175, 29.3, vp_);

    // Stop timer.
    const int64_t runtime =
        (rtc::TimeNanos() - time_start) / rtc::kNumNanosecsPerMicrosec;
    if (runtime < min_runtime || run_idx == 0) {
      min_runtime = runtime;
    }
    total_runtime += runtime;
  }

  printf("\nAverage run time = %d us / frame\n",
         static_cast<int>(total_runtime));
  printf("Min run time = %d us / frame\n\n", static_cast<int>(min_runtime));
}

void PreprocessFrameAndVerify(const VideoFrame& source,
                              int target_width,
                              int target_height,
                              VideoProcessing* vpm,
                              const VideoFrame** out_frame) {
  ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(target_width, target_height, 30));
  *out_frame = vpm->PreprocessFrame(source);
  EXPECT_TRUE(*out_frame != nullptr);

  // If no resizing is needed, expect the original frame.
  if (target_width == source.width() && target_height == source.height()) {
    EXPECT_EQ(&source, *out_frame);
    return;
  }

  // Verify the resampled frame.
  EXPECT_EQ(source.render_time_ms(), (*out_frame)->render_time_ms());
  EXPECT_EQ(source.timestamp(), (*out_frame)->timestamp());
  EXPECT_EQ(target_width, (*out_frame)->width());
  EXPECT_EQ(target_height, (*out_frame)->height());
}

rtc::scoped_refptr<VideoFrameBuffer> CropBuffer(
    rtc::scoped_refptr<VideoFrameBuffer> source_buffer,
    int offset_x,
    int offset_y,
    int cropped_width,
    int cropped_height) {
  // Force even.
  offset_x &= ~1;
  offset_y &= ~1;

  size_t y_start = offset_x + offset_y * source_buffer->StrideY();
  size_t u_start = (offset_x / 2) + (offset_y / 2) * source_buffer->StrideU();
  size_t v_start = (offset_x / 2) + (offset_y / 2) * source_buffer->StrideV();

  return rtc::scoped_refptr<VideoFrameBuffer>(
      new rtc::RefCountedObject<WrappedI420Buffer>(
          cropped_width, cropped_height, source_buffer->DataY() + y_start,
          source_buffer->StrideY(), source_buffer->DataU() + u_start,
          source_buffer->StrideU(), source_buffer->DataV() + v_start,
          source_buffer->StrideV(), rtc::KeepRefUntilDone(source_buffer)));
}

void TestSize(const VideoFrame& source_frame,
              const VideoFrameBuffer& cropped_source,
              int target_width,
              int target_height,
              double expected_psnr,
              VideoProcessing* vpm) {
  // Resample source_frame to out_frame.
  const VideoFrame* out_frame = NULL;
  vpm->SetInputFrameResampleMode(kBox);
  PreprocessFrameAndVerify(source_frame, target_width, target_height, vpm,
                           &out_frame);
  if (out_frame == NULL)
    return;
  WriteProcessedFrameForVisualInspection(source_frame, *out_frame);

  // Scale |resampled_source_frame| back to the source scale.
  VideoFrame resampled_source_frame(*out_frame);
  // Compute PSNR against the cropped source frame and check expectation.
  PreprocessFrameAndVerify(resampled_source_frame,
                           cropped_source.width(),
                           cropped_source.height(), vpm, &out_frame);
  WriteProcessedFrameForVisualInspection(resampled_source_frame, *out_frame);

  // Compute PSNR against the cropped source frame and check expectation.
  double psnr =
      I420PSNR(cropped_source, *out_frame->video_frame_buffer());
  EXPECT_GT(psnr, expected_psnr);
  printf(
      "PSNR: %f. PSNR is between source of size %d %d, and a modified "
      "source which is scaled down/up to: %d %d, and back to source size \n",
      psnr, source_frame.width(), source_frame.height(), target_width,
      target_height);
}

void WriteProcessedFrameForVisualInspection(const VideoFrame& source,
                                            const VideoFrame& processed) {
  // Skip if writing to files is not enabled.
  if (!FLAGS_gen_files)
    return;
  // Write the processed frame to file for visual inspection.
  std::ostringstream filename;
  filename << webrtc::test::OutputPath() << "Resampler_from_" << source.width()
           << "x" << source.height() << "_to_" << processed.width() << "x"
           << processed.height() << "_30Hz_P420.yuv";
  std::cout << "Watch " << filename.str() << " and verify that it is okay."
            << std::endl;
  FILE* stand_alone_file = fopen(filename.str().c_str(), "wb");
  if (PrintVideoFrame(processed, stand_alone_file) < 0)
    std::cerr << "Failed to write: " << filename.str() << std::endl;
  if (stand_alone_file)
    fclose(stand_alone_file);
}

}  // namespace webrtc
