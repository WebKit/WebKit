/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/render_delay_controller_metrics.h"

#include <algorithm>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

enum class DelayReliabilityCategory {
  kNone,
  kPoor,
  kMedium,
  kGood,
  kExcellent,
  kNumCategories
};
enum class DelayChangesCategory {
  kNone,
  kFew,
  kSeveral,
  kMany,
  kConstant,
  kNumCategories
};

}  // namespace

void RenderDelayControllerMetrics::Update(rtc::Optional<size_t> delay_samples,
                                          size_t buffer_delay_blocks) {
  ++call_counter_;

  if (!initial_update) {
    if (delay_samples) {
      ++reliable_delay_estimate_counter_;
      size_t delay_blocks = (*delay_samples) / kBlockSize;

      if (delay_blocks != delay_blocks_) {
        ++delay_change_counter_;
        delay_blocks_ = delay_blocks;
      }
    }
  } else if (++initial_call_counter_ == 5 * kNumBlocksPerSecond) {
    initial_update = false;
  }

  if (call_counter_ == kMetricsReportingIntervalBlocks) {
    int value_to_report = static_cast<int>(delay_blocks_);
    value_to_report = std::min(124, value_to_report);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.EchoCanceller.EchoPathDelay",
                                value_to_report, 0, 124, 125);

    value_to_report = static_cast<int>(buffer_delay_blocks);
    value_to_report = std::min(124, value_to_report);
    RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.EchoCanceller.BufferDelay",
                                value_to_report, 0, 124, 125);

    DelayReliabilityCategory delay_reliability;
    if (reliable_delay_estimate_counter_ == 0) {
      delay_reliability = DelayReliabilityCategory::kNone;
    } else if (reliable_delay_estimate_counter_ > (call_counter_ >> 1)) {
      delay_reliability = DelayReliabilityCategory::kExcellent;
    } else if (reliable_delay_estimate_counter_ > 100) {
      delay_reliability = DelayReliabilityCategory::kGood;
    } else if (reliable_delay_estimate_counter_ > 10) {
      delay_reliability = DelayReliabilityCategory::kMedium;
    } else {
      delay_reliability = DelayReliabilityCategory::kPoor;
    }
    RTC_HISTOGRAM_ENUMERATION(
        "WebRTC.Audio.EchoCanceller.ReliableDelayEstimates",
        static_cast<int>(delay_reliability),
        static_cast<int>(DelayReliabilityCategory::kNumCategories));

    DelayChangesCategory delay_changes;
    if (delay_change_counter_ == 0) {
      delay_changes = DelayChangesCategory::kNone;
    } else if (delay_change_counter_ > 10) {
      delay_changes = DelayChangesCategory::kConstant;
    } else if (delay_change_counter_ > 5) {
      delay_changes = DelayChangesCategory::kMany;
    } else if (delay_change_counter_ > 2) {
      delay_changes = DelayChangesCategory::kSeveral;
    } else {
      delay_changes = DelayChangesCategory::kFew;
    }
    RTC_HISTOGRAM_ENUMERATION(
        "WebRTC.Audio.EchoCanceller.DelayChanges",
        static_cast<int>(delay_changes),
        static_cast<int>(DelayChangesCategory::kNumCategories));

    metrics_reported_ = true;
    call_counter_ = 0;
    ResetMetrics();
  } else {
    metrics_reported_ = false;
  }
}

void RenderDelayControllerMetrics::ResetMetrics() {
  delay_change_counter_ = 0;
  reliable_delay_estimate_counter_ = 0;
}

}  // namespace webrtc
