/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_TIMING_H_
#define MODULES_VIDEO_CODING_TIMING_TIMING_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/timing/decode_time_percentile_filter.h"
#include "modules/video_coding/timing/timestamp_extrapolator.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class VCMTiming {
 public:
  struct VideoDelayTimings {
    static constexpr TimeDelta kDefaultRenderDelay = TimeDelta::Millis(10);

    void Reset();
    TimeDelta TargetDelay() const;
    TimeDelta StatsTargetDelay() const;
    // Returns whether the low-latency path should be used, i.e., frames should
    // be decoded and rendered as soon as possible.
    bool UseLowLatencyRendering() const;

    size_t num_decoded_frames = 0;
    // Pre-decode delay added to smooth out frame delay variation ("jitter")
    // caused by the network. The target delay will be no smaller than this
    // delay, thus it is called `minimum_delay`.
    TimeDelta minimum_delay = TimeDelta::Zero();
    // Estimated time needed to decode a video frame. Obtained as the 95th
    // percentile decode time over a recent time window.
    TimeDelta estimated_max_decode_time = TimeDelta::Zero();
    // Post-decode delay added to smooth out frame delay variation caused by
    // decoding and rendering. Set to a constant.
    TimeDelta render_delay = kDefaultRenderDelay;
    // Minimum total delay used when determining render time for a frame.
    // Obtained from API, `playout-delay` RTP header extension, or A/V sync.
    TimeDelta min_playout_delay = TimeDelta::Zero();
    // Maximum total delay used when determining render time for a frame.
    // Obtained from `playout-delay` RTP header extension.
    TimeDelta max_playout_delay = TimeDelta::Seconds(10);
    // Target total delay. Obtained from all the elements above.
    TimeDelta stats_target_delay = TimeDelta::Zero();
    // Current total delay. Obtained by smoothening the `target_delay`.
    TimeDelta current_delay = TimeDelta::Zero();
  };

  VCMTiming(Clock* clock, const FieldTrialsView& field_trials);
  virtual ~VCMTiming() = default;

  // Resets the timing to the initial state.
  void Reset();

  // Sets the amount of time needed to render an image. Defaults to 10 ms.
  void set_render_delay(TimeDelta render_delay);

  // Sets the minimum time the video must be delayed on the receiver to
  // get the desired jitter buffer level.
  void SetMinimumDelay(TimeDelta minimum_delay);

  // Sets/gets the minimum playout delay from capture to render.
  TimeDelta min_playout_delay() const;
  void set_min_playout_delay(TimeDelta min_playout_delay);

  // Sets the minimum and maximum playout delay from capture to render.
  void set_playout_delay(const VideoPlayoutDelay& playout_delay);

  // Increases or decreases the current delay to get closer to the target delay.
  // Given the actual decode time in ms and the render time in ms for a frame,
  // this function calculates how late the frame is and increases the delay
  // accordingly.
  void UpdateCurrentDelay(Timestamp render_time, Timestamp actual_decode_time);

  // Stops the decoder timer, should be called when the decoder returns a frame
  // or when the decoded frame callback is called.
  void StopDecodeTimer(TimeDelta decode_time, Timestamp now);

  // Used to report that a frame is passed to decoding. Updates the timestamp
  // filter which is used to map between timestamps and receiver system time.
  virtual void OnCompleteTemporalUnit(uint32_t rtp_timestamp, Timestamp now);

  // Returns the receiver system time when the frame with `rtp_timestamp`
  // should be rendered, assuming that the system time currently is `now`.
  Timestamp RenderTime(uint32_t rtp_timestamp, Timestamp now) const;

  // Returns the current target delay which is minimum delay + decode time +
  // render delay.
  TimeDelta TargetVideoDelay() const;

  // Returns current timing information.
  VideoDelayTimings GetTimings() const;

  void SetMaxCompositionDelayInFrames(
      std::optional<int> max_composition_delay_in_frames);

  VideoFrame::RenderParameters RenderParameters() const;

 private:
  mutable Mutex mutex_;
  Clock* const clock_;
  const std::unique_ptr<TimestampExtrapolator> ts_extrapolator_
      RTC_PT_GUARDED_BY(mutex_);
  std::unique_ptr<DecodeTimePercentileFilter> decode_time_filter_
      RTC_GUARDED_BY(mutex_) RTC_PT_GUARDED_BY(mutex_);

  // Holds the current video delay timings.
  VideoDelayTimings timings_ RTC_GUARDED_BY(mutex_);

  std::optional<int> max_composition_delay_in_frames_ RTC_GUARDED_BY(mutex_);
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_TIMING_H_
