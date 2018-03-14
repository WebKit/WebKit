/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "modules/video_coding/codecs/test/test_config.h"
#include "rtc_base/ptr_util.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {

// Codec settings.
const bool kResilienceOn = true;
const int kCifWidth = 352;
const int kCifHeight = 288;
#if !defined(WEBRTC_IOS)
const int kNumFramesShort = 100;
#endif
const int kNumFramesLong = 300;

const std::nullptr_t kNoVisualizationParams = nullptr;

}  // namespace

class VideoProcessorIntegrationTestLibvpx
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestLibvpx() {
    config_.filename = "foreman_cif";
    config_.input_filename = ResourcePath(config_.filename, "yuv");
    config_.output_filename =
        TempFilename(OutputPath(), "videoprocessor_integrationtest_libvpx");
    config_.num_frames = kNumFramesLong;
    // Only allow encoder/decoder to use single core, for predictability.
    config_.use_single_core = true;
    config_.hw_encoder = false;
    config_.hw_decoder = false;
    config_.encoded_frame_checker = &qp_frame_checker_;
  }

 private:
  // Verify that the QP parser returns the same QP as the encoder does.
  const class QpFrameChecker : public TestConfig::EncodedFrameChecker {
   public:
    void CheckEncodedFrame(webrtc::VideoCodecType codec,
                           const EncodedImage& encoded_frame) const override {
      int qp;
      if (codec == kVideoCodecVP8) {
        EXPECT_TRUE(
            vp8::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
      } else if (codec == kVideoCodecVP9) {
        EXPECT_TRUE(
            vp9::GetQp(encoded_frame._buffer, encoded_frame._length, &qp));
      } else {
        RTC_NOTREACHED();
      }
      EXPECT_EQ(encoded_frame.qp_, qp) << "Encoder QP != parsed bitstream QP.";
    }
  } qp_frame_checker_;
};

// Fails on iOS. See webrtc:4755.
#if !defined(WEBRTC_IOS)

#if !defined(RTC_DISABLE_VP9)
// VP9: Run with no packet loss and fixed bitrate. Quality should be very high.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTestLibvpx, Process0PercentPacketLossVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, false, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);
  config_.num_frames = kNumFramesShort;

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFramesShort + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {0, 40, 20, 10, 20, 0, 1}};

  QualityThresholds quality_thresholds(37.0, 36.0, 0.93, 0.92);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// VP9: Run with no packet loss, with varying bitrate (3 rate updates):
// low to high to medium. Check that quality and encoder response to the new
// target rate/per-frame bandwidth (for each rate update) is within limits.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTestLibvpx, ProcessNoLossChangeBitRateVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, false, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {200, 30, 100},  // target_kbps, input_fps, frame_index_rate_update
      {700, 30, 200},
      {500, 30, kNumFramesLong + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {{0, 35, 20, 20, 35, 0, 1},
                                                      {2, 0, 20, 20, 60, 0, 0},
                                                      {0, 0, 25, 20, 40, 0, 0}};

  QualityThresholds quality_thresholds(35.5, 30.0, 0.90, 0.85);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// VP9: Run with no packet loss, with an update (decrease) in frame rate.
// Lower frame rate means higher per-frame-bandwidth, so easier to encode.
// At the low bitrate in this test, this means better rate control after the
// update(s) to lower frame rate. So expect less frame drops, and max values
// for the rate control metrics can be lower. One key frame (first frame only).
// Note: quality after update should be higher but we currently compute quality
// metrics averaged over whole sequence run.
TEST_F(VideoProcessorIntegrationTestLibvpx,
       ProcessNoLossChangeFrameRateFrameDropVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, false, false, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {100, 24, 100},  // target_kbps, input_fps, frame_index_rate_update
      {100, 15, 200},
      {100, 10, kNumFramesLong + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {45, 50, 95, 15, 45, 0, 1},
      {20, 0, 50, 10, 30, 0, 0},
      {5, 0, 30, 5, 25, 0, 0}};

  QualityThresholds quality_thresholds(31.5, 18.0, 0.80, 0.43);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// VP9: Run with no packet loss and denoiser on. One key frame (first frame).
TEST_F(VideoProcessorIntegrationTestLibvpx, ProcessNoLossDenoiserOnVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, false, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);
  config_.num_frames = kNumFramesShort;

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFramesShort + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {0, 40, 20, 10, 20, 0, 1}};

  QualityThresholds quality_thresholds(36.8, 35.8, 0.92, 0.91);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// Run with no packet loss, at low bitrate.
// spatial_resize is on, for this low bitrate expect one resize in sequence.
// Resize happens on delta frame. Expect only one key frame (first frame).
TEST_F(VideoProcessorIntegrationTestLibvpx,
       DISABLED_ProcessNoLossSpatialResizeFrameDropVP9) {
  config_.SetCodecSettings(kVideoCodecVP9, 1, false, false, true, true,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {{50, 30, kNumFramesLong + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {228, 70, 160, 15, 80, 1, 1}};

  QualityThresholds quality_thresholds(24.0, 13.0, 0.65, 0.37);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// TODO(marpan): Add temporal layer test for VP9, once changes are in
// vp9 wrapper for this.

#endif  // !defined(RTC_DISABLE_VP9)

// VP8: Run with no packet loss and fixed bitrate. Quality should be very high.
// One key frame (first frame only) in sequence. Setting |key_frame_interval|
// to -1 below means no periodic key frames in test.
TEST_F(VideoProcessorIntegrationTestLibvpx, ProcessZeroPacketLoss) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, false, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);
  config_.num_frames = kNumFramesShort;

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFramesShort + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {0, 40, 20, 10, 15, 0, 1}};

  QualityThresholds quality_thresholds(34.95, 33.0, 0.90, 0.89);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

#endif  // !defined(WEBRTC_IOS)

// The tests below are currently disabled for Android. For ARM, the encoder
// uses |cpu_speed| = 12, as opposed to default |cpu_speed| <= 6 for x86,
// which leads to significantly different quality. The quality and rate control
// settings in the tests below are defined for encoder speed setting
// |cpu_speed| <= ~6. A number of settings would need to be significantly
// modified for the |cpu_speed| = 12 case. For now, keep the tests below
// disabled on Android. Some quality parameter in the above test has been
// adjusted to also pass for |cpu_speed| <= 12.

// VP8: Run with no packet loss, with varying bitrate (3 rate updates):
// low to high to medium. Check that quality and encoder response to the new
// target rate/per-frame bandwidth (for each rate update) is within limits.
// One key frame (first frame only) in sequence.
// Too slow to finish before timeout on iOS. See webrtc:4755.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_ProcessNoLossChangeBitRateVP8 \
  DISABLED_ProcessNoLossChangeBitRateVP8
#else
#define MAYBE_ProcessNoLossChangeBitRateVP8 ProcessNoLossChangeBitRateVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx,
       MAYBE_ProcessNoLossChangeBitRateVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, false, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {200, 30, 100},  // target_kbps, input_fps, frame_index_rate_update
      {800, 30, 200},
      {500, 30, kNumFramesLong + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {{0, 45, 20, 10, 15, 0, 1},
                                                      {0, 0, 25, 20, 10, 0, 0},
                                                      {0, 0, 25, 15, 10, 0, 0}};

  QualityThresholds quality_thresholds(34.0, 32.0, 0.85, 0.80);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// VP8: Run with no packet loss, with an update (decrease) in frame rate.
// Lower frame rate means higher per-frame-bandwidth, so easier to encode.
// At the bitrate in this test, this means better rate control after the
// update(s) to lower frame rate. So expect less frame drops, and max values
// for the rate control metrics can be lower. One key frame (first frame only).
// Note: quality after update should be higher but we currently compute quality
// metrics averaged over whole sequence run.
// Too slow to finish before timeout on iOS. See webrtc:4755.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_ProcessNoLossChangeFrameRateFrameDropVP8 \
  DISABLED_ProcessNoLossChangeFrameRateFrameDropVP8
#else
#define MAYBE_ProcessNoLossChangeFrameRateFrameDropVP8 \
  ProcessNoLossChangeFrameRateFrameDropVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx,
       MAYBE_ProcessNoLossChangeFrameRateFrameDropVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 1, false, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {
      {80, 24, 100},  // target_kbps, input_fps, frame_index_rate_update
      {80, 15, 200},
      {80, 10, kNumFramesLong + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {40, 20, 75, 15, 60, 0, 1},
      {10, 0, 25, 10, 35, 0, 0},
      {0, 0, 20, 10, 15, 0, 0}};

  QualityThresholds quality_thresholds(31.0, 22.0, 0.80, 0.65);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

// VP8: Run with no packet loss, with 3 temporal layers, with a rate update in
// the middle of the sequence. The max values for the frame size mismatch and
// encoding rate mismatch are applied to each layer.
// No dropped frames in this test, and internal spatial resizer is off.
// One key frame (first frame only) in sequence, so no spatial resizing.
// Too slow to finish before timeout on iOS. See webrtc:4755.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_ProcessNoLossTemporalLayersVP8 \
  DISABLED_ProcessNoLossTemporalLayersVP8
#else
#define MAYBE_ProcessNoLossTemporalLayersVP8 ProcessNoLossTemporalLayersVP8
#endif
TEST_F(VideoProcessorIntegrationTestLibvpx,
       MAYBE_ProcessNoLossTemporalLayersVP8) {
  config_.SetCodecSettings(kVideoCodecVP8, 3, false, true, true, false,
                           kResilienceOn, kCifWidth, kCifHeight);

  std::vector<RateProfile> rate_profiles = {{200, 30, 150},
                                            {400, 30, kNumFramesLong + 1}};

  std::vector<RateControlThresholds> rc_thresholds = {{0, 20, 30, 10, 10, 0, 1},
                                                      {0, 0, 30, 15, 10, 0, 0}};

  QualityThresholds quality_thresholds(32.5, 30.0, 0.85, 0.80);

  ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                              &quality_thresholds, nullptr,
                              kNoVisualizationParams);
}

}  // namespace test
}  // namespace webrtc
