/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/quality_scaler_resource.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/adaptation/resource.h"
#include "api/field_trials_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_image.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/checks.h"
#include "video/adaptation/video_stream_encoder_resource.h"
#include "video/video_stream_encoder_observer.h"

namespace webrtc {

// static
scoped_refptr<QualityScalerResource> QualityScalerResource::Create() {
  return make_ref_counted<QualityScalerResource>();
}

QualityScalerResource::QualityScalerResource()
    : VideoStreamEncoderResource("QualityScalerResource"),
      quality_scaler_(nullptr) {}

QualityScalerResource::~QualityScalerResource() {
  RTC_DCHECK(!quality_scaler_);
}

bool QualityScalerResource::is_started() const {
  RTC_DCHECK_RUN_ON(encoder_queue());
  return quality_scaler_.get();
}

void QualityScalerResource::StartCheckForOveruse(
    VideoEncoder::QpThresholds qp_thresholds,
    const FieldTrialsView& field_trials) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(!is_started());
  quality_scaler_ = std::make_unique<QualityScaler>(
      this, std::move(qp_thresholds), field_trials);
}

void QualityScalerResource::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(is_started());
  // Ensure we have no pending callbacks. This makes it safe to destroy the
  // QualityScaler and even task queues with tasks in-flight.
  quality_scaler_.reset();
}

void QualityScalerResource::SetQpThresholds(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(is_started());
  quality_scaler_->SetQpThresholds(std::move(qp_thresholds));
}

void QualityScalerResource::OnEncodeCompleted(const EncodedImage& encoded_image,
                                              int64_t time_sent_in_us) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  if (quality_scaler_ && encoded_image.qp_ >= 0) {
    quality_scaler_->ReportQp(encoded_image.qp_, time_sent_in_us);
  }
}

void QualityScalerResource::OnFrameDropped(
    VideoStreamEncoderObserver::DropReason reason) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  if (quality_scaler_) {
    if (reason == VideoStreamEncoderObserver::DropReason::kEncoder) {
      quality_scaler_->ReportDroppedFrameByEncoder();
    } else if (reason ==
               VideoStreamEncoderObserver::DropReason::kMediaOptimization) {
      quality_scaler_->ReportDroppedFrameByMediaOpt();
    }
  }
}

void QualityScalerResource::OnReportQpUsageHigh() {
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
}

void QualityScalerResource::OnReportQpUsageLow() {
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

}  // namespace webrtc
