/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/video_codec_stats.h"

namespace webrtc {
namespace test {

void VideoCodecStats::Stream::LogMetrics(
    MetricsLogger* logger,
    std::string test_case_name,
    std::map<std::string, std::string> metadata) const {
  logger->LogMetric("width", test_case_name, width, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("height", test_case_name, height, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric(
      "frame_size_bytes", test_case_name, frame_size_bytes, Unit::kBytes,
      webrtc::test::ImprovementDirection::kNeitherIsBetter, metadata);

  logger->LogMetric("keyframe", test_case_name, keyframe, Unit::kCount,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric("qp", test_case_name, qp, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric(
      "encode_time_ms", test_case_name, encode_time_ms, Unit::kMilliseconds,
      webrtc::test::ImprovementDirection::kSmallerIsBetter, metadata);

  logger->LogMetric(
      "decode_time_ms", test_case_name, decode_time_ms, Unit::kMilliseconds,
      webrtc::test::ImprovementDirection::kSmallerIsBetter, metadata);

  logger->LogMetric("target_bitrate_kbps", test_case_name, target_bitrate_kbps,
                    Unit::kKilobitsPerSecond,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("target_framerate_fps", test_case_name,
                    target_framerate_fps, Unit::kHertz,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("encoded_bitrate_kbps", test_case_name,
                    encoded_bitrate_kbps, Unit::kKilobitsPerSecond,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("encoded_framerate_fps", test_case_name,
                    encoded_framerate_fps, Unit::kHertz,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("bitrate_mismatch_pct", test_case_name,
                    bitrate_mismatch_pct, Unit::kPercent,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric("framerate_mismatch_pct", test_case_name,
                    framerate_mismatch_pct, Unit::kPercent,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric("transmission_time_ms", test_case_name,
                    transmission_time_ms, Unit::kMilliseconds,
                    webrtc::test::ImprovementDirection::kSmallerIsBetter,
                    metadata);

  logger->LogMetric("psnr_y_db", test_case_name, psnr.y, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("psnr_u_db", test_case_name, psnr.u, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);

  logger->LogMetric("psnr_v_db", test_case_name, psnr.v, Unit::kUnitless,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter,
                    metadata);
}

}  // namespace test
}  // namespace webrtc
