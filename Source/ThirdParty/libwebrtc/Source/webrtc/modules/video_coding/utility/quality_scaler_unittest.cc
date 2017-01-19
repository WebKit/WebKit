/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/quality_scaler.h"

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
static const int kNumSeconds = 10;
static const int kWidth = 1920;
static const int kHeight = 1080;
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kNormalQp = 30;
static const int kLowQpThreshold = 18;
static const int kHighQp = 40;
static const int kDisabledBadQpThreshold = 64;
static const int kLowInitialBitrateKbps = 300;
// These values need to be in sync with corresponding constants
// in quality_scaler.cc
static const int kMeasureSecondsFastUpscale = 2;
static const int kMeasureSecondsUpscale = 5;
static const int kMeasureSecondsDownscale = 5;
static const int kMinDownscaleDimension = 140;
}  // namespace

class QualityScalerTest : public ::testing::Test {
 protected:
  enum ScaleDirection {
    kKeepScaleAtHighQp,
    kScaleDown,
    kScaleDownAboveHighQp,
    kScaleUp
  };

  QualityScalerTest() {
    input_frame_ = I420Buffer::Create(kWidth, kHeight);
    qs_.Init(kLowQpThreshold, kHighQp, 0, kWidth, kHeight, kFramerate);
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  }

  bool TriggerScale(ScaleDirection scale_direction) {
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    int initial_width = qs_.GetScaledResolution().width;
    for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
      switch (scale_direction) {
        case kScaleUp:
          qs_.ReportQP(kLowQp);
          break;
        case kScaleDown:
          qs_.ReportDroppedFrame();
          break;
        case kKeepScaleAtHighQp:
          qs_.ReportQP(kHighQp);
          break;
        case kScaleDownAboveHighQp:
          qs_.ReportQP(kHighQp + 1);
          break;
      }
      qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
      if (qs_.GetScaledResolution().width != initial_width)
        return true;
    }

    return false;
  }

  void ExpectOriginalFrame() {
    EXPECT_EQ(input_frame_, qs_.GetScaledBuffer(input_frame_))
        << "Using scaled frame instead of original input.";
  }

  void ExpectScaleUsingReportedResolution() {
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    rtc::scoped_refptr<VideoFrameBuffer> scaled_frame =
        qs_.GetScaledBuffer(input_frame_);
    EXPECT_EQ(res.width, scaled_frame->width());
    EXPECT_EQ(res.height, scaled_frame->height());
  }

  void ContinuouslyDownscalesByHalfDimensionsAndBackUp();

  void DoesNotDownscaleFrameDimensions(int width, int height);

  void DownscaleEndsAt(int input_width,
                       int input_height,
                       int end_width,
                       int end_height);

  QualityScaler qs_;
  rtc::scoped_refptr<VideoFrameBuffer> input_frame_;
};

TEST_F(QualityScalerTest, UsesOriginalFrameInitially) {
  ExpectOriginalFrame();
}

TEST_F(QualityScalerTest, ReportsOriginalResolutionInitially) {
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_EQ(input_frame_->width(), res.width);
  EXPECT_EQ(input_frame_->height(), res.height);
}

TEST_F(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  EXPECT_TRUE(TriggerScale(kScaleDown)) << "No downscale within " << kNumSeconds
                                        << " seconds.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_LT(res.width, input_frame_->width());
  EXPECT_LT(res.height, input_frame_->height());
}

TEST_F(QualityScalerTest, KeepsScaleAtHighQp) {
  EXPECT_FALSE(TriggerScale(kKeepScaleAtHighQp))
      << "Downscale at high threshold which should keep scale.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_EQ(res.width, input_frame_->width());
  EXPECT_EQ(res.height, input_frame_->height());
}

TEST_F(QualityScalerTest, DownscalesAboveHighQp) {
  EXPECT_TRUE(TriggerScale(kScaleDownAboveHighQp))
      << "No downscale within " << kNumSeconds << " seconds.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_LT(res.width, input_frame_->width());
  EXPECT_LT(res.height, input_frame_->height());
}

TEST_F(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 3; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.ReportDroppedFrame();
    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    if (qs_.GetScaledResolution().width < input_frame_->width())
      return;
  }

  FAIL() << "No downscale within " << kNumSeconds << " seconds.";
}

TEST_F(QualityScalerTest, DoesNotDownscaleOnNormalQp) {
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    ASSERT_EQ(input_frame_->width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 2; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    ASSERT_EQ(input_frame_->width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";

    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    ASSERT_EQ(input_frame_->width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";
  }
}

void QualityScalerTest::ContinuouslyDownscalesByHalfDimensionsAndBackUp() {
  const int initial_min_dimension =
      input_frame_->width() < input_frame_->height() ? input_frame_->width()
                                                     : input_frame_->height();
  int min_dimension = initial_min_dimension;
  int current_shift = 0;
  // Drop all frames to force-trigger downscaling.
  while (min_dimension >= 2 * kMinDownscaleDimension) {
    EXPECT_TRUE(TriggerScale(kScaleDown)) << "No downscale within "
                                          << kNumSeconds << " seconds.";
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    min_dimension = res.width < res.height ? res.width : res.height;
    ++current_shift;
    ASSERT_EQ(input_frame_->width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_->height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Make sure we can scale back with good-quality frames.
  while (min_dimension < initial_min_dimension) {
    EXPECT_TRUE(TriggerScale(kScaleUp)) << "No upscale within " << kNumSeconds
                                        << " seconds.";
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    min_dimension = res.width < res.height ? res.width : res.height;
    --current_shift;
    ASSERT_EQ(input_frame_->width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_->height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Verify we don't start upscaling after further low use.
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportQP(kLowQp);
    ExpectOriginalFrame();
  }
}

TEST_F(QualityScalerTest, ContinuouslyDownscalesByHalfDimensionsAndBackUp) {
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

TEST_F(QualityScalerTest,
       ContinuouslyDownscalesOddResolutionsByHalfDimensionsAndBackUp) {
  const int kOddWidth = 517;
  const int kOddHeight = 1239;
  input_frame_ = I420Buffer::Create(kOddWidth, kOddHeight);
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

void QualityScalerTest::DoesNotDownscaleFrameDimensions(int width, int height) {
  input_frame_ = I420Buffer::Create(width, height);

  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
    ASSERT_EQ(input_frame_->width(), qs_.GetScaledResolution().width)
        << "Unexpected scale of minimal-size frame.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1PxWidth) {
  DoesNotDownscaleFrameDimensions(1, kHeight);
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1PxHeight) {
  DoesNotDownscaleFrameDimensions(kWidth, 1);
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1Px) {
  DoesNotDownscaleFrameDimensions(1, 1);
}

TEST_F(QualityScalerTest, DoesNotDownscaleBelow2xDefaultMinDimensionsWidth) {
  DoesNotDownscaleFrameDimensions(
      2 * kMinDownscaleDimension - 1, 1000);
}

TEST_F(QualityScalerTest, DoesNotDownscaleBelow2xDefaultMinDimensionsHeight) {
  DoesNotDownscaleFrameDimensions(
      1000, 2 * kMinDownscaleDimension - 1);
}

TEST_F(QualityScalerTest, DownscaleToVgaOnLowInitialBitrate) {
  static const int kWidth720p = 1280;
  static const int kHeight720p = 720;
  static const int kInitialBitrateKbps = 300;
  input_frame_ = I420Buffer::Create(kWidth720p, kHeight720p);
  qs_.Init(kLowQpThreshold, kDisabledBadQpThreshold, kInitialBitrateKbps,
           kWidth720p, kHeight720p, kFramerate);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  int init_width = qs_.GetScaledResolution().width;
  int init_height = qs_.GetScaledResolution().height;
  EXPECT_EQ(640, init_width);
  EXPECT_EQ(360, init_height);
}

TEST_F(QualityScalerTest, DownscaleToQvgaOnLowerInitialBitrate) {
  static const int kWidth720p = 1280;
  static const int kHeight720p = 720;
  static const int kInitialBitrateKbps = 200;
  input_frame_ = I420Buffer::Create(kWidth720p, kHeight720p);
  qs_.Init(kLowQpThreshold, kDisabledBadQpThreshold, kInitialBitrateKbps,
           kWidth720p, kHeight720p, kFramerate);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  int init_width = qs_.GetScaledResolution().width;
  int init_height = qs_.GetScaledResolution().height;
  EXPECT_EQ(320, init_width);
  EXPECT_EQ(180, init_height);
}

TEST_F(QualityScalerTest, DownscaleAfterMeasuredSecondsThenSlowerBackUp) {
  qs_.Init(kLowQpThreshold, kHighQp, 0, kWidth, kHeight, kFramerate);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  QualityScaler::Resolution initial_res = qs_.GetScaledResolution();

  // Should not downscale if less than kMeasureSecondsDownscale seconds passed.
  for (int i = 0; i < kFramerate * kMeasureSecondsDownscale - 1; ++i) {
    qs_.ReportQP(kHighQp + 1);
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  }
  EXPECT_EQ(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_EQ(initial_res.height, qs_.GetScaledResolution().height);

  // Should downscale if more than kMeasureSecondsDownscale seconds passed (add
  // last frame).
  qs_.ReportQP(kHighQp + 1);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  EXPECT_GT(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_GT(initial_res.height, qs_.GetScaledResolution().height);

  // Should not upscale if less than kMeasureSecondsUpscale seconds passed since
  // we saw issues initially (have already gone down).
  for (int i = 0; i < kFramerate * kMeasureSecondsUpscale - 1; ++i) {
    qs_.ReportQP(kLowQp);
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  }
  EXPECT_GT(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_GT(initial_res.height, qs_.GetScaledResolution().height);

  // Should upscale (back to initial) if kMeasureSecondsUpscale seconds passed
  // (add last frame).
  qs_.ReportQP(kLowQp);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  EXPECT_EQ(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_EQ(initial_res.height, qs_.GetScaledResolution().height);
}

TEST_F(QualityScalerTest, UpscaleQuicklyInitiallyAfterMeasuredSeconds) {
  qs_.Init(kLowQpThreshold, kHighQp, kLowInitialBitrateKbps, kWidth, kHeight,
           kFramerate);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  QualityScaler::Resolution initial_res = qs_.GetScaledResolution();

  // Should not upscale if less than kMeasureSecondsFastUpscale seconds passed.
  for (int i = 0; i < kFramerate * kMeasureSecondsFastUpscale - 1; ++i) {
    qs_.ReportQP(kLowQp);
    qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  }
  EXPECT_EQ(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_EQ(initial_res.height, qs_.GetScaledResolution().height);

  // Should upscale if kMeasureSecondsFastUpscale seconds passed (add last
  // frame).
  qs_.ReportQP(kLowQp);
  qs_.OnEncodeFrame(input_frame_->width(), input_frame_->height());
  EXPECT_LT(initial_res.width, qs_.GetScaledResolution().width);
  EXPECT_LT(initial_res.height, qs_.GetScaledResolution().height);
}

void QualityScalerTest::DownscaleEndsAt(int input_width,
                                        int input_height,
                                        int end_width,
                                        int end_height) {
  // Create a frame with 2x expected end width/height to verify that we can
  // scale down to expected end width/height.
  input_frame_ = I420Buffer::Create(input_width, input_height);

  int last_width = input_width;
  int last_height = input_height;
  // Drop all frames to force-trigger downscaling.
  while (true) {
    TriggerScale(kScaleDown);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    if (last_width == res.width) {
      EXPECT_EQ(last_height, res.height);
      EXPECT_EQ(end_width, res.width);
      EXPECT_EQ(end_height, res.height);
      break;
    }
    last_width = res.width;
    last_height = res.height;
  }
}

TEST_F(QualityScalerTest, DownscalesTo320x180) {
  DownscaleEndsAt(640, 360, 320, 180);
}

TEST_F(QualityScalerTest, DownscalesTo180x320) {
  DownscaleEndsAt(360, 640, 180, 320);
}

TEST_F(QualityScalerTest, DownscalesFrom1280x720To320x180) {
  DownscaleEndsAt(1280, 720, 320, 180);
}

TEST_F(QualityScalerTest, DoesntDownscaleInitialQvga) {
  DownscaleEndsAt(320, 180, 320, 180);
}

}  // namespace webrtc
