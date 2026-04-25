/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_selector.h"

#include <algorithm>
#include <cstdint>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

constexpr int kVideoRtpTicksPerMs = kVideoPayloadTypeFrequency / 1000;

namespace {
bool CanNativelyHandleFormat(VideoFrameBuffer::Type type) {
  switch (type) {
    case VideoFrameBuffer::Type::kNV12:
    case VideoFrameBuffer::Type::kI420:
      return true;
    default:
      return false;
  }
}

}  // namespace

FrameSelector::FrameSelector(ScalabilityMode scalability_mode,
                             Timespan low_overhead_frame_span,
                             Timespan high_overhead_frame_span)
    : inter_layer_pred_mode_(
          ScalabilityModeToInterLayerPredMode(scalability_mode)),
      low_overhead_frame_span_(low_overhead_frame_span),
      high_overhead_frame_span_(high_overhead_frame_span),
      random_(TimeMicros()) {
  RTC_DCHECK_GE(low_overhead_frame_span.upper_bound,
                low_overhead_frame_span.lower_bound);
  RTC_DCHECK_GE(high_overhead_frame_span.upper_bound,
                high_overhead_frame_span.lower_bound);
}

bool FrameSelector::ShouldInstrumentFrame(const VideoFrame& raw_frame,
                                          const EncodedImage& encoded_frame) {
  int layer_id = std::max(encoded_frame.SpatialIndex().value_or(0),
                          encoded_frame.SimulcastIndex().value_or(0));
  if (encoded_frame.IsKey()) {
    // Always insrument keyframes. Clear any state related to this stream.
    if (inter_layer_pred_mode_ != InterLayerPredMode::kOff) {
      // When inter layer prediction is enabled, a keyframe clear all reference
      // buffers and so all layers must be reset.
      next_timestamp_cutoff_thresholds_.clear();
    } else {
      // For simulcast or S-mode SVC, keyframes happen independently per layer,
      // so clear only the state for this layer.
      next_timestamp_cutoff_thresholds_.erase(layer_id);
    }
  }

  Timestamp current_time = encoded_frame.CaptureTime();
  if (current_time.IsZero()) {
    // Use 90kHz RTP clock as capture time.
    current_time =
        Timestamp::Millis(encoded_frame.RtpTimestamp() / kVideoRtpTicksPerMs);
  }

  bool is_low_overhead =
      CanNativelyHandleFormat(raw_frame.video_frame_buffer()->type());
  const Timespan& span =
      is_low_overhead ? low_overhead_frame_span_ : high_overhead_frame_span_;

  bool select_frame;
  auto it = next_timestamp_cutoff_thresholds_.find(layer_id);
  if (it == next_timestamp_cutoff_thresholds_.end()) {
    // First frame for this layer (i.e., part of keyframe).
    select_frame = true;
  } else if (current_time >= it->second) {
    select_frame = true;
  } else {
    select_frame = false;
  }

  if (select_frame) {
    // Update threshold.
    // random uniform between lower and upper bound.
    int32_t lower = static_cast<int32_t>(span.lower_bound.us());
    int32_t upper = static_cast<int32_t>(span.upper_bound.us());
    int32_t random_delay_us = random_.Rand(lower, upper);
    Timestamp next_threshold =
        current_time + TimeDelta::Micros(random_delay_us);
    if (it != next_timestamp_cutoff_thresholds_.end()) {
      it->second = next_threshold;
    } else {
      next_timestamp_cutoff_thresholds_.emplace(layer_id, next_threshold);
    }
  }

  return select_frame;
}

}  // namespace webrtc
