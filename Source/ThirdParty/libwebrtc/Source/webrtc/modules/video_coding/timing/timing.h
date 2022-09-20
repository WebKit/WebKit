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

#include <memory>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/video/video_frame.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/timing/codec_timer.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time/timestamp_extrapolator.h"

namespace webrtc {

class Clock;
class TimestampExtrapolator;

class VCMTiming {
 public:
  static constexpr auto kDefaultRenderDelay = TimeDelta::Millis(10);
  static constexpr auto kDelayMaxChangeMsPerS = 100;

  VCMTiming(Clock* clock, const FieldTrialsView& field_trials);
  virtual ~VCMTiming() = default;

  // Resets the timing to the initial state.
  void Reset();

  // Set the amount of time needed to render an image. Defaults to 10 ms.
  void set_render_delay(TimeDelta render_delay);

  // Set the minimum time the video must be delayed on the receiver to
  // get the desired jitter buffer level.
  void SetJitterDelay(TimeDelta required_delay);

  // Set/get the minimum playout delay from capture to render.
  TimeDelta min_playout_delay() const;
  void set_min_playout_delay(TimeDelta min_playout_delay);

  // Set/get the maximum playout delay from capture to render in ms.
  void set_max_playout_delay(TimeDelta max_playout_delay);

  // Increases or decreases the current delay to get closer to the target delay.
  // Calculates how long it has been since the previous call to this function,
  // and increases/decreases the delay in proportion to the time difference.
  void UpdateCurrentDelay(uint32_t frame_timestamp);

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
  void IncomingTimestamp(uint32_t rtp_timestamp, Timestamp last_packet_time);

  // Returns the receiver system time when the frame with timestamp
  // `frame_timestamp` should be rendered, assuming that the system time
  // currently is `now`.
  virtual Timestamp RenderTime(uint32_t frame_timestamp, Timestamp now) const;

  // Returns the maximum time in ms that we can wait for a frame to become
  // complete before we must pass it to the decoder. render_time==0 indicates
  // that the frames should be processed as quickly as possible, with possibly
  // only a small delay added to make sure that the decoder is not overloaded.
  // In this case, the parameter too_many_frames_queued is used to signal that
  // the decode queue is full and that the frame should be decoded as soon as
  // possible.
  virtual TimeDelta MaxWaitingTime(Timestamp render_time,
                                   Timestamp now,
                                   bool too_many_frames_queued) const;

  // Returns the current target delay which is required delay + decode time +
  // render delay.
  TimeDelta TargetVideoDelay() const;

  // Return current timing information. Returns true if the first frame has been
  // decoded, false otherwise.
  struct VideoDelayTimings {
    TimeDelta max_decode_duration;
    TimeDelta current_delay;
    TimeDelta target_delay;
    TimeDelta jitter_buffer_delay;
    TimeDelta min_playout_delay;
    TimeDelta max_playout_delay;
    TimeDelta render_delay;
    size_t num_decoded_frames;
  };
  VideoDelayTimings GetTimings() const;

  void SetTimingFrameInfo(const TimingFrameInfo& info);
  absl::optional<TimingFrameInfo> GetTimingFrameInfo();

  void SetMaxCompositionDelayInFrames(
      absl::optional<int> max_composition_delay_in_frames);

  VideoFrame::RenderParameters RenderParameters() const;

  // Updates the last time a frame was scheduled for decoding.
  void SetLastDecodeScheduledTimestamp(Timestamp last_decode_scheduled);

 protected:
  TimeDelta RequiredDecodeTime() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Timestamp RenderTimeInternal(uint32_t frame_timestamp, Timestamp now) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  TimeDelta TargetDelayInternal() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool UseLowLatencyRendering() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

 private:
  mutable Mutex mutex_;
  Clock* const clock_;
  const std::unique_ptr<TimestampExtrapolator> ts_extrapolator_
      RTC_PT_GUARDED_BY(mutex_);
  std::unique_ptr<CodecTimer> codec_timer_ RTC_GUARDED_BY(mutex_)
      RTC_PT_GUARDED_BY(mutex_);
  TimeDelta render_delay_ RTC_GUARDED_BY(mutex_);
  // Best-effort playout delay range for frames from capture to render.
  // The receiver tries to keep the delay between `min_playout_delay_ms_`
  // and `max_playout_delay_ms_` taking the network jitter into account.
  // A special case is where min_playout_delay_ms_ = max_playout_delay_ms_ = 0,
  // in which case the receiver tries to play the frames as they arrive.
  TimeDelta min_playout_delay_ RTC_GUARDED_BY(mutex_);
  TimeDelta max_playout_delay_ RTC_GUARDED_BY(mutex_);
  TimeDelta jitter_delay_ RTC_GUARDED_BY(mutex_);
  TimeDelta current_delay_ RTC_GUARDED_BY(mutex_);
  uint32_t prev_frame_timestamp_ RTC_GUARDED_BY(mutex_);
  absl::optional<TimingFrameInfo> timing_frame_info_ RTC_GUARDED_BY(mutex_);
  size_t num_decoded_frames_ RTC_GUARDED_BY(mutex_);
  absl::optional<int> max_composition_delay_in_frames_ RTC_GUARDED_BY(mutex_);
  // Set by the field trial WebRTC-ZeroPlayoutDelay. The parameter min_pacing
  // determines the minimum delay between frames scheduled for decoding that is
  // used when min playout delay=0 and max playout delay>=0.
  FieldTrialParameter<TimeDelta> zero_playout_delay_min_pacing_
      RTC_GUARDED_BY(mutex_);
  // Timestamp at which the last frame was scheduled to be sent to the decoder.
  // Used only when the RTP header extension playout delay is set to min=0 ms
  // which is indicated by a render time set to 0.
  Timestamp last_decode_scheduled_ RTC_GUARDED_BY(mutex_);
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_TIMING_H_
