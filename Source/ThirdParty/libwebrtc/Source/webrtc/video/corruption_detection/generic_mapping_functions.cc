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

#include <cmath>

#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr int kLumaThreshold = 5;
constexpr int kChromaThresholdVp8 = 6;
constexpr int kChromaThresholdVp9 = 4;
constexpr int kChromaThresholdAv1 = 4;
constexpr int kChromaThresholdH264 = 2;

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
  switch (codec_type) {
    case VideoCodecType::kVideoCodecVP8:
      return ExponentialFunction(0.006, 0.01857465, -4.26470513, qp);
    case VideoCodecType::kVideoCodecVP9:
      return RationalFunction(1, -257, 0.3, qp);
    case VideoCodecType::kVideoCodecAV1:
      return RationalFunction(0.69, -256, 0.42, qp);
    case VideoCodecType::kVideoCodecH264:
      return ExponentialFunction(0.016, 0.13976962, -1.40179328, qp);
    default:
      RTC_FATAL() << "Codec type " << CodecTypeToPayloadString(codec_type)
                  << " is not supported.";
  }
}

}  // namespace

FilterSettings GetCorruptionFilterSettings(int qp, VideoCodecType codec_type) {
  return FilterSettings{.std_dev = MapQpToOptimalStdDev(qp, codec_type),
                        .luma_error_threshold = LumaThreshold(codec_type),
                        .chroma_error_threshold = ChromaThreshold(codec_type)};
}

}  // namespace webrtc
