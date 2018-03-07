/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_common.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"
#include "modules/video_coding/codecs/test/stats.h"
#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "test/gtest.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"
#include "test/testsupport/packet_reader.h"

namespace webrtc {
namespace test {

// Rates for the encoder and the frame number when to change profile.
struct RateProfile {
  int target_kbps;
  int input_fps;
  int frame_index_rate_update;
};

// Thresholds for the rate control metrics. The thresholds are defined for each
// rate update sequence. |max_num_frames_to_hit_target| is defined as number of
// frames, after a rate update is made to the encoder, for the encoder to reach
// |kMaxBitrateMismatchPercent| of new target rate.
struct RateControlThresholds {
  int max_num_dropped_frames;
  int max_key_framesize_mismatch_percent;
  int max_delta_framesize_mismatch_percent;
  int max_bitrate_mismatch_percent;
  int max_num_frames_to_hit_target;
  int num_spatial_resizes;
  int num_key_frames;
};

// Thresholds for the quality metrics.
struct QualityThresholds {
  QualityThresholds(double min_avg_psnr,
                    double min_min_psnr,
                    double min_avg_ssim,
                    double min_min_ssim)
      : min_avg_psnr(min_avg_psnr),
        min_min_psnr(min_min_psnr),
        min_avg_ssim(min_avg_ssim),
        min_min_ssim(min_min_ssim) {}
  double min_avg_psnr;
  double min_min_psnr;
  double min_avg_ssim;
  double min_min_ssim;
};

struct BitstreamThresholds {
  explicit BitstreamThresholds(size_t max_nalu_length)
      : max_nalu_length(max_nalu_length) {}
  size_t max_nalu_length;
};

// Should video files be saved persistently to disk for post-run visualization?
struct VisualizationParams {
  bool save_encoded_ivf;
  bool save_decoded_y4m;
};

// Integration test for video processor. Encodes+decodes a clip and
// writes it to the output directory. After completion, quality metrics
// (PSNR and SSIM) and rate control metrics are computed and compared to given
// thresholds, to verify that the quality and encoder response is acceptable.
// The rate control tests allow us to verify the behavior for changing bit rate,
// changing frame rate, frame dropping/spatial resize, and temporal layers.
// The thresholds for the rate control metrics are set to be fairly
// conservative, so failure should only happen when some significant regression
// or breakdown occurs.
class VideoProcessorIntegrationTest : public testing::Test {
 protected:
  // Verifies that all H.264 keyframes contain SPS/PPS/IDR NALUs.
  class H264KeyframeChecker : public TestConfig::EncodedFrameChecker {
   public:
    void CheckEncodedFrame(webrtc::VideoCodecType codec,
                           const EncodedImage& encoded_frame) const override;
  };

  VideoProcessorIntegrationTest();
  ~VideoProcessorIntegrationTest() override;

  void ProcessFramesAndMaybeVerify(
      const std::vector<RateProfile>& rate_profiles,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const QualityThresholds* quality_thresholds,
      const BitstreamThresholds* bs_thresholds,
      const VisualizationParams* visualization_params);

  // Config.
  TestConfig config_;

  // Can be used by all H.264 tests.
  const H264KeyframeChecker h264_keyframe_checker_;

 private:
  class CpuProcessTime;
  static const int kMaxNumTemporalLayers = 3;

  struct TestResults {
    int KeyFrameSizeMismatchPercent() const {
      if (num_key_frames == 0) {
        return -1;
      }
      return 100 * sum_key_framesize_mismatch / num_key_frames;
    }
    int DeltaFrameSizeMismatchPercent(int i) const {
      return 100 * sum_delta_framesize_mismatch_layer[i] / num_frames_layer[i];
    }
    int BitrateMismatchPercent(float target_kbps) const {
      return 100 * std::fabs(kbps - target_kbps) / target_kbps;
    }
    int BitrateMismatchPercent(int i, float target_kbps_layer) const {
      return 100 * std::fabs(kbps_layer[i] - target_kbps_layer) /
          target_kbps_layer;
    }
    int num_frames = 0;
    int num_frames_layer[kMaxNumTemporalLayers] = {0};
    int num_key_frames = 0;
    int num_frames_to_hit_target = 0;
    float sum_framesize_kbits = 0.0f;
    float sum_framesize_kbits_layer[kMaxNumTemporalLayers] = {0};
    float kbps = 0.0f;
    float kbps_layer[kMaxNumTemporalLayers] = {0};
    float sum_key_framesize_mismatch = 0.0f;
    float sum_delta_framesize_mismatch_layer[kMaxNumTemporalLayers] = {0};
  };

  struct TargetRates {
    int kbps;
    int fps;
    float kbps_layer[kMaxNumTemporalLayers];
    float fps_layer[kMaxNumTemporalLayers];
    float framesize_kbits_layer[kMaxNumTemporalLayers];
    float key_framesize_kbits_initial;
    float key_framesize_kbits;
  };

  struct QualityMetrics {
    int num_decoded_frames = 0;
    double total_psnr = 0.0;
    double total_ssim = 0.0;
    double min_psnr = std::numeric_limits<double>::max();
    double min_ssim = std::numeric_limits<double>::max();
  };

  void CreateEncoderAndDecoder();
  void DestroyEncoderAndDecoder();
  void SetUpAndInitObjects(rtc::TaskQueue* task_queue,
                           const int initial_bitrate_kbps,
                           const int initial_framerate_fps,
                           const VisualizationParams* visualization_params);
  void ReleaseAndCloseObjects(rtc::TaskQueue* task_queue);

  // Rate control metrics.
  void ResetRateControlMetrics(int rate_update_index,
                               const std::vector<RateProfile>& rate_profiles);
  void SetRatesPerTemporalLayer();
  void UpdateRateControlMetrics(int frame_number);
  void PrintRateControlMetrics(
      int rate_update_index,
      const std::vector<int>& num_dropped_frames,
      const std::vector<int>& num_spatial_resizes) const;
  void VerifyRateControlMetrics(
      int rate_update_index,
      const std::vector<RateControlThresholds>* rc_thresholds,
      const std::vector<int>& num_dropped_frames,
      const std::vector<int>& num_spatial_resizes) const;

  void VerifyBitstream(int frame_number,
                       const BitstreamThresholds& bs_thresholds);

  void UpdateQualityMetrics(int frame_number);
  void VerifyQualityMetrics(const QualityThresholds& quality_thresholds);

  void PrintSettings() const;

  // Codecs.
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;

  // Helper objects.
  std::unique_ptr<FrameReader> analysis_frame_reader_;
  std::unique_ptr<FrameWriter> analysis_frame_writer_;
  std::unique_ptr<IvfFileWriter> encoded_frame_writer_;
  std::unique_ptr<FrameWriter> decoded_frame_writer_;
  PacketReader packet_reader_;
  std::unique_ptr<PacketManipulator> packet_manipulator_;
  Stats stats_;
  std::unique_ptr<VideoProcessor> processor_;
  std::unique_ptr<CpuProcessTime> cpu_process_time_;

  // Quantities updated for every encoded frame.
  TestResults actual_;

  // Rates set for every encoder rate update.
  TargetRates target_;

  QualityMetrics quality_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_INTEGRATIONTEST_H_
