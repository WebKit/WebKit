/*
 *  Copyright (c) 2008 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <memory>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/media/base/fakevideorenderer.h"
#include "webrtc/media/base/testutils.h"
#include "webrtc/media/base/videocapturer.h"

using cricket::FakeVideoCapturer;

namespace {

const int kMsCallbackWait = 500;
// For HD only the height matters.
const int kMinHdHeight = 720;

}  // namespace

class VideoCapturerTest
    : public sigslot::has_slots<>,
      public testing::Test {
 public:
  VideoCapturerTest()
      : capture_state_(cricket::CS_STOPPED), num_state_changes_(0) {
    InitCapturer(false);
  }

 protected:
  void InitCapturer(bool is_screencast) {
    capturer_ = std::unique_ptr<FakeVideoCapturer>(
        new FakeVideoCapturer(is_screencast));
    capturer_->SignalStateChange.connect(this,
                                         &VideoCapturerTest::OnStateChange);
    capturer_->AddOrUpdateSink(&renderer_, rtc::VideoSinkWants());
  }
  void InitScreencast() { InitCapturer(true); }

  void OnStateChange(cricket::VideoCapturer*,
                     cricket::CaptureState capture_state) {
    capture_state_ = capture_state;
    ++num_state_changes_;
  }
  cricket::CaptureState capture_state() { return capture_state_; }
  int num_state_changes() { return num_state_changes_; }

  std::unique_ptr<cricket::FakeVideoCapturer> capturer_;
  cricket::CaptureState capture_state_;
  int num_state_changes_;
  cricket::FakeVideoRenderer renderer_;
  bool expects_rotation_applied_;
};

TEST_F(VideoCapturerTest, CaptureState) {
  EXPECT_TRUE(capturer_->enable_video_adapter());
  EXPECT_EQ(cricket::CS_RUNNING, capturer_->Start(cricket::VideoFormat(
      640,
      480,
      cricket::VideoFormat::FpsToInterval(30),
      cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_->IsRunning());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, capture_state(), kMsCallbackWait);
  EXPECT_EQ(1, num_state_changes());
  capturer_->Stop();
  EXPECT_EQ_WAIT(cricket::CS_STOPPED, capture_state(), kMsCallbackWait);
  EXPECT_EQ(2, num_state_changes());
  capturer_->Stop();
  rtc::Thread::Current()->ProcessMessages(100);
  EXPECT_EQ(2, num_state_changes());
}

TEST_F(VideoCapturerTest, ScreencastScaledOddWidth) {
  InitScreencast();

  int kWidth = 1281;
  int kHeight = 720;

  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(kWidth, kHeight,
                                         cricket::VideoFormat::FpsToInterval(5),
                                         cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(formats);

  EXPECT_EQ(cricket::CS_RUNNING,
            capturer_->Start(cricket::VideoFormat(
                kWidth, kHeight, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_->IsRunning());
  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(1, renderer_.num_rendered_frames());
  EXPECT_EQ(kWidth, renderer_.width());
  EXPECT_EQ(kHeight, renderer_.height());
}

TEST_F(VideoCapturerTest, TestRotationAppliedBySource) {
  int kWidth = 800;
  int kHeight = 400;
  int frame_count = 0;

  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(kWidth, kHeight,
                                         cricket::VideoFormat::FpsToInterval(5),
                                         cricket::FOURCC_I420));

  capturer_->ResetSupportedFormats(formats);
  rtc::VideoSinkWants wants;
  // |capturer_| should compensate rotation.
  wants.rotation_applied = true;
  capturer_->AddOrUpdateSink(&renderer_, wants);

  // capturer_ should compensate rotation as default.
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer_->Start(cricket::VideoFormat(
                kWidth, kHeight, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_->IsRunning());
  EXPECT_EQ(0, renderer_.num_rendered_frames());

  // If the frame's rotation is compensated anywhere in the pipeline based on
  // the rotation information, the renderer should be given the right dimension
  // such that the frame could be rendered.

  capturer_->SetRotation(webrtc::kVideoRotation_90);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  // Swapped width and height
  EXPECT_EQ(kWidth, renderer_.height());
  EXPECT_EQ(kHeight, renderer_.width());
  EXPECT_EQ(webrtc::kVideoRotation_0, renderer_.rotation());

  capturer_->SetRotation(webrtc::kVideoRotation_270);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  // Swapped width and height
  EXPECT_EQ(kWidth, renderer_.height());
  EXPECT_EQ(kHeight, renderer_.width());
  EXPECT_EQ(webrtc::kVideoRotation_0, renderer_.rotation());

  capturer_->SetRotation(webrtc::kVideoRotation_180);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  // Back to normal width and height
  EXPECT_EQ(kWidth, renderer_.width());
  EXPECT_EQ(kHeight, renderer_.height());
  EXPECT_EQ(webrtc::kVideoRotation_0, renderer_.rotation());
}

TEST_F(VideoCapturerTest, TestRotationAppliedBySinkByDefault) {
  int kWidth = 800;
  int kHeight = 400;

  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(kWidth, kHeight,
                                         cricket::VideoFormat::FpsToInterval(5),
                                         cricket::FOURCC_I420));

  capturer_->ResetSupportedFormats(formats);

  EXPECT_EQ(cricket::CS_RUNNING,
            capturer_->Start(cricket::VideoFormat(
                kWidth, kHeight, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_->IsRunning());
  EXPECT_EQ(0, renderer_.num_rendered_frames());

  // If the frame's rotation is compensated anywhere in the pipeline, the frame
  // won't have its original dimension out from capturer. Since the renderer
  // here has the same dimension as the capturer, it will skip that frame as the
  // resolution won't match anymore.

  int frame_count = 0;
  capturer_->SetRotation(webrtc::kVideoRotation_0);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  EXPECT_EQ(capturer_->GetRotation(), renderer_.rotation());

  capturer_->SetRotation(webrtc::kVideoRotation_90);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  EXPECT_EQ(capturer_->GetRotation(), renderer_.rotation());

  capturer_->SetRotation(webrtc::kVideoRotation_180);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  EXPECT_EQ(capturer_->GetRotation(), renderer_.rotation());

  capturer_->SetRotation(webrtc::kVideoRotation_270);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  EXPECT_EQ(capturer_->GetRotation(), renderer_.rotation());
}

TEST_F(VideoCapturerTest, TestRotationAppliedBySourceWhenDifferentWants) {
  int kWidth = 800;
  int kHeight = 400;

  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(kWidth, kHeight,
                                         cricket::VideoFormat::FpsToInterval(5),
                                         cricket::FOURCC_I420));

  capturer_->ResetSupportedFormats(formats);
  rtc::VideoSinkWants wants;
  // capturer_ should not compensate rotation.
  wants.rotation_applied = false;
  capturer_->AddOrUpdateSink(&renderer_, wants);

  EXPECT_EQ(cricket::CS_RUNNING,
            capturer_->Start(cricket::VideoFormat(
                kWidth, kHeight, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_->IsRunning());
  EXPECT_EQ(0, renderer_.num_rendered_frames());

  int frame_count = 0;
  capturer_->SetRotation(webrtc::kVideoRotation_90);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  EXPECT_EQ(capturer_->GetRotation(), renderer_.rotation());

  // Add another sink that wants frames to be rotated.
  cricket::FakeVideoRenderer renderer2;
  wants.rotation_applied = true;
  capturer_->AddOrUpdateSink(&renderer2, wants);

  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(++frame_count, renderer_.num_rendered_frames());
  EXPECT_EQ(1, renderer2.num_rendered_frames());
  EXPECT_EQ(webrtc::kVideoRotation_0, renderer_.rotation());
  EXPECT_EQ(webrtc::kVideoRotation_0, renderer2.rotation());
}

// TODO(nisse): This test doesn't quite fit here. It tests two things:
// Aggregation of VideoSinkWants, which is the responsibility of
// VideoBroadcaster, and translation of VideoSinkWants to actual
// resolution, which is the responsibility of the VideoAdapter.
TEST_F(VideoCapturerTest, SinkWantsMaxPixelAndMaxPixelCountStepUp) {
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer_->Start(cricket::VideoFormat(
                1280, 720, cricket::VideoFormat::FpsToInterval(30),
                cricket::FOURCC_I420)));
  EXPECT_TRUE(capturer_->IsRunning());

  EXPECT_EQ(0, renderer_.num_rendered_frames());
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(1, renderer_.num_rendered_frames());
  EXPECT_EQ(1280, renderer_.width());
  EXPECT_EQ(720, renderer_.height());

  // Request a lower resolution. The output resolution will have a resolution
  // with less than or equal to |wants.max_pixel_count| depending on how the
  // capturer can scale the input frame size.
  rtc::VideoSinkWants wants;
  wants.max_pixel_count = 1280 * 720 * 3 / 5;
  capturer_->AddOrUpdateSink(&renderer_, wants);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(2, renderer_.num_rendered_frames());
  EXPECT_EQ(960, renderer_.width());
  EXPECT_EQ(540, renderer_.height());

  // Request a lower resolution.
  wants.max_pixel_count = (renderer_.width() * renderer_.height() * 3) / 5;
  capturer_->AddOrUpdateSink(&renderer_, wants);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(3, renderer_.num_rendered_frames());
  EXPECT_EQ(640, renderer_.width());
  EXPECT_EQ(360, renderer_.height());

  // Adding a new renderer should not affect resolution.
  cricket::FakeVideoRenderer renderer2;
  capturer_->AddOrUpdateSink(&renderer2, rtc::VideoSinkWants());
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(4, renderer_.num_rendered_frames());
  EXPECT_EQ(640, renderer_.width());
  EXPECT_EQ(360, renderer_.height());
  EXPECT_EQ(1, renderer2.num_rendered_frames());
  EXPECT_EQ(640, renderer2.width());
  EXPECT_EQ(360, renderer2.height());

  // Request higher resolution.
  wants.target_pixel_count.emplace((wants.max_pixel_count * 5) / 3);
  wants.max_pixel_count = wants.max_pixel_count * 4;
  capturer_->AddOrUpdateSink(&renderer_, wants);
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(5, renderer_.num_rendered_frames());
  EXPECT_EQ(960, renderer_.width());
  EXPECT_EQ(540, renderer_.height());
  EXPECT_EQ(2, renderer2.num_rendered_frames());
  EXPECT_EQ(960, renderer2.width());
  EXPECT_EQ(540, renderer2.height());

  // Updating with no wants should not affect resolution.
  capturer_->AddOrUpdateSink(&renderer2, rtc::VideoSinkWants());
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(6, renderer_.num_rendered_frames());
  EXPECT_EQ(960, renderer_.width());
  EXPECT_EQ(540, renderer_.height());
  EXPECT_EQ(3, renderer2.num_rendered_frames());
  EXPECT_EQ(960, renderer2.width());
  EXPECT_EQ(540, renderer2.height());

  // But resetting the wants should reset the resolution to what the camera is
  // opened with.
  capturer_->AddOrUpdateSink(&renderer_, rtc::VideoSinkWants());
  EXPECT_TRUE(capturer_->CaptureFrame());
  EXPECT_EQ(7, renderer_.num_rendered_frames());
  EXPECT_EQ(1280, renderer_.width());
  EXPECT_EQ(720, renderer_.height());
  EXPECT_EQ(4, renderer2.num_rendered_frames());
  EXPECT_EQ(1280, renderer2.width());
  EXPECT_EQ(720, renderer2.height());
}

TEST_F(VideoCapturerTest, TestFourccMatch) {
  cricket::VideoFormat desired(640, 480,
                               cricket::VideoFormat::FpsToInterval(30),
                               cricket::FOURCC_ANY);
  cricket::VideoFormat best;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.fourcc = cricket::FOURCC_MJPG;
  EXPECT_FALSE(capturer_->GetBestCaptureFormat(desired, &best));

  desired.fourcc = cricket::FOURCC_I420;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
}

TEST_F(VideoCapturerTest, TestResolutionMatch) {
  cricket::VideoFormat desired(1920, 1080,
                               cricket::VideoFormat::FpsToInterval(30),
                               cricket::FOURCC_ANY);
  cricket::VideoFormat best;
  // Ask for 1920x1080. Get HD 1280x720 which is the highest.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(1280, best.width);
  EXPECT_EQ(720, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 360;
  desired.height = 250;
  // Ask for a little higher than QVGA. Get QVGA.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 480;
  desired.height = 270;
  // Ask for HVGA. Get VGA.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 320;
  desired.height = 240;
  // Ask for QVGA. Get QVGA.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 80;
  desired.height = 60;
  // Ask for lower than QQVGA. Get QQVGA, which is the lowest.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(160, best.width);
  EXPECT_EQ(120, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);
}

TEST_F(VideoCapturerTest, TestHDResolutionMatch) {
  // Add some HD formats typical of a mediocre HD webcam.
  std::vector<cricket::VideoFormat> formats;
  formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(960, 544,
      cricket::VideoFormat::FpsToInterval(24), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(15), cricket::FOURCC_I420));
  formats.push_back(cricket::VideoFormat(2592, 1944,
      cricket::VideoFormat::FpsToInterval(7), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(formats);

  cricket::VideoFormat desired(960, 720,
                               cricket::VideoFormat::FpsToInterval(30),
                               cricket::FOURCC_ANY);
  cricket::VideoFormat best;
  // Ask for 960x720 30 fps. Get qHD 24 fps
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(960, best.width);
  EXPECT_EQ(544, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(24), best.interval);

  desired.width = 960;
  desired.height = 544;
  desired.interval = cricket::VideoFormat::FpsToInterval(30);
  // Ask for qHD 30 fps. Get qHD 24 fps
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(960, best.width);
  EXPECT_EQ(544, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(24), best.interval);

  desired.width = 360;
  desired.height = 250;
  desired.interval = cricket::VideoFormat::FpsToInterval(30);
  // Ask for a little higher than QVGA. Get QVGA.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 480;
  desired.height = 270;
  // Ask for HVGA. Get VGA.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 320;
  desired.height = 240;
  // Ask for QVGA. Get QVGA.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 160;
  desired.height = 120;
  // Ask for lower than QVGA. Get QVGA, which is the lowest.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(320, best.width);
  EXPECT_EQ(240, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 1280;
  desired.height = 720;
  // Ask for HD. 720p fps is too low. Get VGA which has 30 fps.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  desired.width = 1280;
  desired.height = 720;
  desired.interval = cricket::VideoFormat::FpsToInterval(15);
  // Ask for HD 15 fps. Fps matches. Get HD
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(1280, best.width);
  EXPECT_EQ(720, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), best.interval);

  desired.width = 1920;
  desired.height = 1080;
  desired.interval = cricket::VideoFormat::FpsToInterval(30);
  // Ask for 1080p. Fps of HD formats is too low. Get VGA which can do 30 fps.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);
}

// Some cameras support 320x240 and 320x640. Verify we choose 320x240.
TEST_F(VideoCapturerTest, TestStrangeFormats) {
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 640,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats;
  required_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  required_formats.push_back(cricket::VideoFormat(320, 200,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  required_formats.push_back(cricket::VideoFormat(320, 180,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  cricket::VideoFormat best;
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
  }

  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(320, 640,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
  }
}

// Some cameras only have very low fps. Verify we choose something sensible.
TEST_F(VideoCapturerTest, TestPoorFpsFormats) {
  // all formats are low framerate
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(10), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(7), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(2), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats;
  required_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  cricket::VideoFormat best;
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // Increase framerate of 320x240. Expect low fps VGA avoided.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(7), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(2), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
  }
}

// Some cameras support same size with different frame rates. Verify we choose
// the frame rate properly.
TEST_F(VideoCapturerTest, TestSameSizeDifferentFpsFormats) {
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(10), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(20), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(320, 240,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats = supported_formats;
  cricket::VideoFormat best;
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(320, best.width);
    EXPECT_EQ(240, best.height);
    EXPECT_EQ(required_formats[i].interval, best.interval);
  }
}

// Some cameras support the correct resolution but at a lower fps than
// we'd like. This tests we get the expected resolution and fps.
TEST_F(VideoCapturerTest, TestFpsFormats) {
  // We have VGA but low fps. Choose VGA, not HD
  std::vector<cricket::VideoFormat> supported_formats;
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(15), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats;
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ANY));
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(20), cricket::FOURCC_ANY));
  required_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(10), cricket::FOURCC_ANY));
  cricket::VideoFormat best;

  // Expect 30 fps to choose 30 fps format.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[0], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(400, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  // Expect 20 fps to choose 30 fps format.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[1], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(400, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(30), best.interval);

  // Expect 10 fps to choose 15 fps format and set fps to 15.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[2], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), best.interval);

  // We have VGA 60 fps and 15 fps. Choose best fps.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_MJPG));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(15), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  // Expect 30 fps to choose 60 fps format and will set best fps to 60.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[0], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(60), best.interval);

  // Expect 20 fps to choose 60 fps format, and will set best fps to 60.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[1], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(60), best.interval);

  // Expect 10 fps to choose 15 fps.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[2], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(480, best.height);
  EXPECT_EQ(cricket::VideoFormat::FpsToInterval(15), best.interval);
}

TEST_F(VideoCapturerTest, TestRequest16x10_9) {
  std::vector<cricket::VideoFormat> supported_formats;
  // We do not support HD, expect 4x3 for 4x3, 16x10, and 16x9 requests.
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  std::vector<cricket::VideoFormat> required_formats = supported_formats;
  cricket::VideoFormat best;
  // Expect 4x3, 16x10, and 16x9 requests are respected.
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // We do not support 16x9 HD, expect 4x3 for 4x3, 16x10, and 16x9 requests.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(960, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  // Expect 4x3, 16x10, and 16x9 requests are respected.
  for (size_t i = 0; i < required_formats.size(); ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // We support 16x9HD, Expect 4x3, 16x10, and 16x9 requests are respected.
  supported_formats.clear();
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 480,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 400,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(640, 360,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);

  // Expect 4x3 for 4x3 and 16x10 requests.
  for (size_t i = 0; i < required_formats.size() - 1; ++i) {
    EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[i], &best));
    EXPECT_EQ(required_formats[i].width, best.width);
    EXPECT_EQ(required_formats[i].height, best.height);
  }

  // Expect 16x9 for 16x9 request.
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(required_formats[2], &best));
  EXPECT_EQ(640, best.width);
  EXPECT_EQ(360, best.height);
}

bool HdFormatInList(const std::vector<cricket::VideoFormat>& formats) {
  for (std::vector<cricket::VideoFormat>::const_iterator found =
           formats.begin(); found != formats.end(); ++found) {
    if (found->height >= kMinHdHeight) {
      return true;
    }
  }
  return false;
}

TEST_F(VideoCapturerTest, Whitelist) {
  // The definition of HD only applies to the height. Set the HD width to the
  // smallest legal number to document this fact in this test.
  const int kMinHdWidth = 1;
  cricket::VideoFormat hd_format(kMinHdWidth,
                                 kMinHdHeight,
                                 cricket::VideoFormat::FpsToInterval(30),
                                 cricket::FOURCC_I420);
  cricket::VideoFormat vga_format(640, 480,
                                  cricket::VideoFormat::FpsToInterval(30),
                                  cricket::FOURCC_I420);
  std::vector<cricket::VideoFormat> formats = *capturer_->GetSupportedFormats();
  formats.push_back(hd_format);

  // Enable whitelist. Expect HD not in list.
  capturer_->set_enable_camera_list(true);
  capturer_->ResetSupportedFormats(formats);
  EXPECT_TRUE(HdFormatInList(*capturer_->GetSupportedFormats()));
  capturer_->ConstrainSupportedFormats(vga_format);
  EXPECT_FALSE(HdFormatInList(*capturer_->GetSupportedFormats()));

  // Disable whitelist. Expect HD in list.
  capturer_->set_enable_camera_list(false);
  capturer_->ResetSupportedFormats(formats);
  EXPECT_TRUE(HdFormatInList(*capturer_->GetSupportedFormats()));
  capturer_->ConstrainSupportedFormats(vga_format);
  EXPECT_TRUE(HdFormatInList(*capturer_->GetSupportedFormats()));
}

TEST_F(VideoCapturerTest, BlacklistAllFormats) {
  cricket::VideoFormat vga_format(640, 480,
                                  cricket::VideoFormat::FpsToInterval(30),
                                  cricket::FOURCC_I420);
  std::vector<cricket::VideoFormat> supported_formats;
  // Mock a device that only supports HD formats.
  supported_formats.push_back(cricket::VideoFormat(1280, 720,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  supported_formats.push_back(cricket::VideoFormat(1920, 1080,
      cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
  capturer_->ResetSupportedFormats(supported_formats);
  EXPECT_EQ(2u, capturer_->GetSupportedFormats()->size());
  // Now, enable the list, which would exclude both formats. However, since
  // only HD formats are available, we refuse to filter at all, so we don't
  // break this camera.
  capturer_->set_enable_camera_list(true);
  capturer_->ConstrainSupportedFormats(vga_format);
  EXPECT_EQ(2u, capturer_->GetSupportedFormats()->size());
  // To make sure it's not just the camera list being broken, add in VGA and
  // try again. This time, only the VGA format should be there.
  supported_formats.push_back(vga_format);
  capturer_->ResetSupportedFormats(supported_formats);
  ASSERT_EQ(1u, capturer_->GetSupportedFormats()->size());
  EXPECT_EQ(vga_format.height, capturer_->GetSupportedFormats()->at(0).height);
}
