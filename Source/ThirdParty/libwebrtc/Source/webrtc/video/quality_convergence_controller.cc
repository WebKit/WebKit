/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/quality_convergence_controller.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace {
// TODO(https://crbug.com/328598314): Remove default values once HW encoders
// correctly report the minimum QP value. These thresholds correspond to the
// default configurations used for the software encoders.
constexpr int kVp8DefaultStaticQpThreshold = 15;
constexpr int kVp9DefaultStaticQpThreshold = 32;
constexpr int kAv1DefaultStaticQpThreshold = 40;

int GetDefaultStaticQpThreshold(VideoCodecType codec) {
  switch (codec) {
    case kVideoCodecVP8:
      return kVp8DefaultStaticQpThreshold;
    case kVideoCodecVP9:
      return kVp9DefaultStaticQpThreshold;
    case kVideoCodecAV1:
      return kAv1DefaultStaticQpThreshold;
    case kVideoCodecGeneric:
    case kVideoCodecH264:
    case kVideoCodecH265:
      // -1 will effectively disable the static QP threshold since QP values are
      // always >= 0.
      return -1;
  }
}
}  // namespace

void QualityConvergenceController::Initialize(
    int number_of_layers,
    absl::optional<int> static_qp_threshold,
    VideoCodecType codec,
    const FieldTrialsView& trials) {
  RTC_CHECK(number_of_layers > 0);
  number_of_layers_ = number_of_layers;
  convergence_monitors_.clear();

  int qp_threshold =
      static_qp_threshold.value_or(GetDefaultStaticQpThreshold(codec));
  for (int i = 0; i < number_of_layers_; ++i) {
    convergence_monitors_.push_back(
        QualityConvergenceMonitor::Create(qp_threshold, codec, trials));
  }
  initialized_ = true;
}

bool QualityConvergenceController::AddSampleAndCheckTargetQuality(
    int layer_index,
    int qp,
    bool is_refresh_frame) {
  RTC_CHECK(initialized_);
  if (layer_index < 0 || layer_index >= number_of_layers_) {
    return false;
  }

  convergence_monitors_[layer_index]->AddSample(qp, is_refresh_frame);
  return convergence_monitors_[layer_index]->AtTargetQuality();
}

}  // namespace webrtc
