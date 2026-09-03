/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/timing.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "api/field_trials_view.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/timing/video_jitter_timing_interface.h"
#include "api/video/video_frame.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/timing/decode_time_percentile_filter.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"
#include "video/timing/default_video_jitter_timing.h"

namespace webrtc {
namespace {

// Maximum `max_playout_delay` for a stream to use low-latency rendering.
constexpr TimeDelta kLowLatencyStreamMaxPlayoutDelayThreshold =
    TimeDelta::Millis(500);

void CheckDelaysValid(TimeDelta min_delay, TimeDelta max_delay) {
  if (min_delay > max_delay) {
    RTC_LOG(LS_ERROR)
        << "Playout delays set incorrectly: min playout delay (" << min_delay
        << ") > max playout delay (" << max_delay
        << "). This is undefined behaviour. Application writers should "
           "ensure that the min delay is always less than or equals max "
           "delay. If trying to use the playout delay header extensions "
           "described in "
           "https://webrtc.googlesource.com/src/+/refs/heads/main/docs/"
           "native-code/rtp-hdrext/playout-delay/, be careful that a playout "
           "delay hint or A/V sync settings may have caused this conflict.";
  }
}

}  // namespace

void VCMTiming::VideoDelayTimings::Reset() {
  minimum_delay = TimeDelta::Zero();
  estimated_max_decode_time = TimeDelta::Zero();
  render_delay = kDefaultRenderDelay;
  min_playout_delay = TimeDelta::Zero();
  stats_target_delay = TimeDelta::Zero();
  current_delay = TimeDelta::Zero();
}

TimeDelta VCMTiming::VideoDelayTimings::TargetDelay() const {
  return std::max(min_playout_delay,
                  minimum_delay + estimated_max_decode_time + render_delay);
}

TimeDelta VCMTiming::VideoDelayTimings::StatsTargetDelay() const {
  TimeDelta delay = TargetDelay() - (estimated_max_decode_time + render_delay);
  return std::max(TimeDelta::Zero(), delay);
}

bool VCMTiming::VideoDelayTimings::UseLowLatencyRendering() const {
  return min_playout_delay.IsZero() &&
         max_playout_delay <= kLowLatencyStreamMaxPlayoutDelayThreshold;
}

VCMTiming::VCMTiming(Clock* clock,
                     const FieldTrialsView& field_trials,
                     TimeDelta render_delay)
    : VCMTiming(clock, field_trials, render_delay, nullptr) {}

VCMTiming::VCMTiming(
    Clock* clock,
    const FieldTrialsView& field_trials,
    TimeDelta render_delay,
    std::unique_ptr<VideoJitterTimingInterface> video_jitter_timing)
    : video_jitter_timing_(
          video_jitter_timing
              ? std::move(video_jitter_timing)
              : std::make_unique<DefaultVideoJitterTiming>(clock,
                                                           field_trials)),
      decode_time_filter_(std::make_unique<DecodeTimePercentileFilter>()),
      timings_({.render_delay = render_delay}) {}

void VCMTiming::Reset() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  video_jitter_timing_->Reset();

  MutexLock lock(&mutex_);
  decode_time_filter_ = std::make_unique<DecodeTimePercentileFilter>();
  timings_.Reset();
}

TimeDelta VCMTiming::min_playout_delay() const {
  MutexLock lock(&mutex_);
  return timings_.min_playout_delay;
}

void VCMTiming::set_min_playout_delay(TimeDelta min_playout_delay) {
  MutexLock lock(&mutex_);
  if (timings_.min_playout_delay != min_playout_delay) {
    CheckDelaysValid(min_playout_delay, timings_.max_playout_delay);
    timings_.min_playout_delay = min_playout_delay;
  }
}

void VCMTiming::set_playout_delay(const VideoPlayoutDelay& playout_delay) {
  MutexLock lock(&mutex_);
  // No need to call `CheckDelaysValid` as the same invariant (min <= max)
  // is guaranteed by the `VideoPlayoutDelay` type.
  timings_.min_playout_delay = playout_delay.min();
  timings_.max_playout_delay = playout_delay.max();
}

void VCMTiming::SetMinimumDelay(TimeDelta minimum_delay) {
  MutexLock lock(&mutex_);
  if (minimum_delay != timings_.minimum_delay) {
    timings_.minimum_delay = minimum_delay;
    // When in initial state, set current delay to minimum delay.
    if (timings_.current_delay.IsZero()) {
      timings_.current_delay = timings_.minimum_delay;
    }
  }
}

void VCMTiming::OnContinuousTemporalUnits(
    std::span<const uint32_t> rtp_timestamps,
    Timestamp now) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  std::optional<TimeDelta> minimum_delay =
      video_jitter_timing_->OnContinuousTemporalUnits(rtp_timestamps, now);
  if (minimum_delay.has_value()) {
    SetMinimumDelay(*minimum_delay);
  }
}

void VCMTiming::OnDecodableTemporalUnit(
    const VideoJitterTimingInterface::TemporalUnitInfo& info) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  std::optional<TimeDelta> minimum_delay =
      video_jitter_timing_->OnDecodableTemporalUnit(info);
  if (minimum_delay.has_value()) {
    SetMinimumDelay(*minimum_delay);
  }
}

void VCMTiming::OnNetworkUpdate(
    const VideoJitterTimingInterface::NetworkInfo& info) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  video_jitter_timing_->OnNetworkUpdate(info);
}

void VCMTiming::UpdateCurrentDelay(Timestamp render_time,
                                   Timestamp actual_decode_time) {
  MutexLock lock(&mutex_);
  TimeDelta target_delay = timings_.TargetDelay();
  TimeDelta delayed = (actual_decode_time - render_time) +
                      timings_.estimated_max_decode_time +
                      timings_.render_delay;

  // Only consider `delayed` as negative by more than a few microseconds.
  if (delayed.ms() < 0) {
    return;
  }
  if (timings_.current_delay + delayed <= target_delay) {
    timings_.current_delay += delayed;
  } else {
    timings_.current_delay = target_delay;
  }
}

void VCMTiming::UpdateDecodeTimeEstimate(TimeDelta decode_time, Timestamp now) {
  MutexLock lock(&mutex_);
  RTC_DCHECK_GE(decode_time, TimeDelta::Zero());
  decode_time_filter_->AddSample(decode_time.ms(), now.ms());
  ++timings_.num_decoded_frames;
  timings_.estimated_max_decode_time =
      TimeDelta::Millis(decode_time_filter_->GetPercentileMs());
}

void VCMTiming::OnCompleteFrame(
    const VideoJitterTimingInterface::FrameInfo& info) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  video_jitter_timing_->OnCompleteFrame(info);
}

Timestamp VCMTiming::RenderTime(uint32_t rtp_timestamp, Timestamp now) const {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  VideoDelayTimings timings = GetTimings();
  if (timings.UseLowLatencyRendering()) {
    // Render as soon as possible.
    return Timestamp::Zero();
  }
  std::optional<Timestamp> local_time =
      video_jitter_timing_->LocalTime(rtp_timestamp);
  if (!local_time.has_value()) {
    return now;
  }
  return *local_time + std::clamp(timings.current_delay,
                                  timings.min_playout_delay,
                                  timings.max_playout_delay);
}

TimeDelta VCMTiming::TargetVideoDelay() const {
  MutexLock lock(&mutex_);
  return timings_.TargetDelay();
}

VCMTiming::VideoDelayTimings VCMTiming::GetTimings() const {
  MutexLock lock(&mutex_);
  VideoDelayTimings timings = timings_;
  timings.stats_target_delay = timings_.StatsTargetDelay();
  return timings;
}

VideoFrame::RenderParameters VCMTiming::RenderParameters() const {
  MutexLock lock(&mutex_);
  return {.use_low_latency_rendering = timings_.UseLowLatencyRendering(),
          .max_composition_delay_in_frames = max_composition_delay_in_frames_};
}

void VCMTiming::SetMaxCompositionDelayInFrames(
    std::optional<int> max_composition_delay_in_frames) {
  MutexLock lock(&mutex_);
  max_composition_delay_in_frames_ = max_composition_delay_in_frames;
}

}  // namespace webrtc
