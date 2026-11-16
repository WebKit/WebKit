/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/generic_mapping_functions.h"

#include <algorithm>
#include <cmath>

#include "api/video/corruption_detection/corruption_detection_filter_settings.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr int kLumaThreshold = 5;
constexpr int kChromaThresholdVp8 = 6;
constexpr int kChromaThresholdVp9 = 4;
constexpr int kChromaThresholdAv1 = 4;
constexpr int kChromaThresholdH264 = 2;
constexpr int kChromaThresholdH265 = 4;

int LumaThreshold(VideoCodecType codec_type) {
  return kLumaThreshold;
}

int ChromaThreshold(VideoCodecType codec_type) {
  switch (codec_type) {
    case VideoCodecType::kVideoCodecVP8:
      return kChromaThresholdVp8;
    case VideoCodecType::kVideoCodecVP9:
      return kChromaThresholdVp9;
    case VideoCodecType::kVideoCodecAV1:
      return kChromaThresholdAv1;
    case VideoCodecType::kVideoCodecH264:
      return kChromaThresholdH264;
    case VideoCodecType::kVideoCodecH265:
      return kChromaThresholdH265;
    default:
      RTC_FATAL() << "Codec type " << CodecTypeToPayloadString(codec_type)
                  << " is not supported.";
  }
}

double ExponentialFunction(double a, double b, double c, int qp) {
  return a * std::exp(b * qp - c);
}

double RationalFunction(double a, double b, double c, int qp) {
  return (-a * qp) / (qp + b) + c;
}

// Maps QP to the optimal standard deviation for the Gausian kernel.
// Observe that the values below can be changed unnoticed.
double MapQpToOptimalStdDev(int qp, VideoCodecType codec_type) {
  double std_dev;
  switch (codec_type) {
    case VideoCodecType::kVideoCodecVP8:
      std_dev = ExponentialFunction(0.006, 0.01857465, -4.26470513, qp);
      break;
    case VideoCodecType::kVideoCodecVP9:
      std_dev = RationalFunction(1, -257, 0.3, qp);
      break;
    case VideoCodecType::kVideoCodecAV1:
      std_dev = RationalFunction(0.69, -256, 0.42, qp);
      break;
    case VideoCodecType::kVideoCodecH264:
      std_dev = ExponentialFunction(0.016, 0.13976962, -1.40179328, qp);
      break;
    case VideoCodecType::kVideoCodecH265:
      // Observe that these values are currently only tuned for software libx265
      // in "preset ultrafast -tune zerolatency" mode.
      std_dev = RationalFunction(1.6, -52, 0.1, qp);
      break;
    default:
      RTC_FATAL() << "Codec type " << CodecTypeToPayloadString(codec_type)
                  << " is not supported.";
  }
  if (std_dev < 0.0 || std_dev > 40.0) {
    RTC_LOG(LS_VERBOSE) << "Generic frame instrumentation settings generated "
                           "incorrect std_dev value for codec "
                        << CodecTypeToPayloadString(codec_type) << " and QP "
                        << qp << ": " << std_dev
                        << ". Capping to legal bound [0, 40]";
    std_dev = std::min(std::max(std_dev, 0.0), 40.0);
  }
  return std_dev;
}

}  // namespace

CorruptionDetectionFilterSettings GetCorruptionFilterSettings(
    int qp,
    VideoCodecType codec_type) {
  return CorruptionDetectionFilterSettings{
      .std_dev = MapQpToOptimalStdDev(qp, codec_type),
      .luma_error_threshold = LumaThreshold(codec_type),
      .chroma_error_threshold = ChromaThreshold(codec_type)};
}

}  // namespace webrtc
