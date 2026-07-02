/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/bandwidth_quality_scaler_resource.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "api/adaptation/resource.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/utility/bandwidth_quality_scaler.h"
#include "rtc_base/checks.h"
#include "video/adaptation/video_stream_encoder_resource.h"

namespace webrtc {

// static
scoped_refptr<BandwidthQualityScalerResource>
BandwidthQualityScalerResource::Create() {
  return make_ref_counted<BandwidthQualityScalerResource>();
}

BandwidthQualityScalerResource::BandwidthQualityScalerResource()
    : VideoStreamEncoderResource("BandwidthQualityScalerResource"),
      bandwidth_quality_scaler_(nullptr) {}

BandwidthQualityScalerResource::~BandwidthQualityScalerResource() {
  RTC_DCHECK(!bandwidth_quality_scaler_);
}

bool BandwidthQualityScalerResource::is_started() const {
  RTC_DCHECK_RUN_ON(encoder_queue());
  return bandwidth_quality_scaler_.get();
}

void BandwidthQualityScalerResource::StartCheckForOveruse(
    const std::vector<VideoEncoder::ResolutionBitrateLimits>&
        resolution_bitrate_limits,
    VideoCodecType codec_type) {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(!is_started());
  bandwidth_quality_scaler_ = std::make_unique<BandwidthQualityScaler>(this);

  // If the configuration parameters more than one, we should define and
  // declare the function BandwidthQualityScaler::Initialize() and call it.
  bandwidth_quality_scaler_->SetResolutionBitrateLimits(
      resolution_bitrate_limits, codec_type);
}

void BandwidthQualityScalerResource::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(is_started());
  // Ensure we have no pending callbacks. This makes it safe to destroy the
  // BandwidthQualityScaler and even task queues with tasks in-flight.
  bandwidth_quality_scaler_.reset();
}

void BandwidthQualityScalerResource::OnReportUsageBandwidthHigh() {
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
}

void BandwidthQualityScalerResource::OnReportUsageBandwidthLow() {
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

void BandwidthQualityScalerResource::OnEncodeCompleted(
    const EncodedImage& encoded_image,
    int64_t time_sent_in_us,
    int64_t encoded_image_size_bytes) {
  RTC_DCHECK_RUN_ON(encoder_queue());

  if (bandwidth_quality_scaler_) {
    bandwidth_quality_scaler_->ReportEncodeInfo(
        encoded_image_size_bytes, time_sent_in_us / 1000,
        encoded_image._encodedWidth, encoded_image._encodedHeight);
  }
}

}  // namespace webrtc
