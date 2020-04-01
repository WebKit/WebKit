/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/encode_usage_resource.h"

#include <limits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

EncodeUsageResource::EncodeUsageResource(
    std::unique_ptr<OveruseFrameDetector> overuse_detector)
    : overuse_detector_(std::move(overuse_detector)),
      is_started_(false),
      target_frame_rate_(absl::nullopt) {
  RTC_DCHECK(overuse_detector_);
}

void EncodeUsageResource::StartCheckForOveruse(CpuOveruseOptions options) {
  RTC_DCHECK(!is_started_);
  overuse_detector_->StartCheckForOveruse(TaskQueueBase::Current(),
                                          std::move(options), this);
  is_started_ = true;
  overuse_detector_->OnTargetFramerateUpdated(TargetFrameRateAsInt());
}

void EncodeUsageResource::StopCheckForOveruse() {
  overuse_detector_->StopCheckForOveruse();
  is_started_ = false;
}

void EncodeUsageResource::SetTargetFrameRate(
    absl::optional<double> target_frame_rate) {
  if (target_frame_rate == target_frame_rate_)
    return;
  target_frame_rate_ = target_frame_rate;
  if (is_started_)
    overuse_detector_->OnTargetFramerateUpdated(TargetFrameRateAsInt());
}

void EncodeUsageResource::OnEncodeStarted(const VideoFrame& cropped_frame,
                                          int64_t time_when_first_seen_us) {
  // TODO(hbos): Rename FrameCaptured() to something more appropriate (e.g.
  // "OnEncodeStarted"?) or revise usage.
  overuse_detector_->FrameCaptured(cropped_frame, time_when_first_seen_us);
}

void EncodeUsageResource::OnEncodeCompleted(
    uint32_t timestamp,
    int64_t time_sent_in_us,
    int64_t capture_time_us,
    absl::optional<int> encode_duration_us) {
  // TODO(hbos): Rename FrameSent() to something more appropriate (e.g.
  // "OnEncodeCompleted"?).
  overuse_detector_->FrameSent(timestamp, time_sent_in_us, capture_time_us,
                               encode_duration_us);
}

void EncodeUsageResource::AdaptUp(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kCpu);
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

bool EncodeUsageResource::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kCpu);
  return OnResourceUsageStateMeasured(ResourceUsageState::kOveruse) !=
         ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
}

int EncodeUsageResource::TargetFrameRateAsInt() {
  return target_frame_rate_.has_value()
             ? static_cast<int>(target_frame_rate_.value())
             : std::numeric_limits<int>::max();
}

}  // namespace webrtc
