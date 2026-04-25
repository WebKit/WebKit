/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RENDERING_TRACKER_H_
#define VIDEO_TIMING_SIMULATOR_RENDERING_TRACKER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/timing/timing.h"
#include "rtc_base/thread_annotations.h"
#include "video/timing/simulator/assembler.h"
#include "video/video_stream_buffer_controller.h"

namespace webrtc::video_timing_simulator {

// Callback for observer events. Implemented by the metadata collector.
class RenderingTrackerEvents {
 public:
  virtual ~RenderingTrackerEvents() = default;
  virtual void OnDecodedFrame(const EncodedFrame& decoded_frame,
                              int frames_dropped,
                              TimeDelta jitter_buffer_minimum_delay,
                              TimeDelta jitter_buffer_target_delay,
                              TimeDelta jitter_buffer_delay) = 0;
  virtual void OnRenderedFrame(const VideoFrame& rendered_frame) = 0;
};

// The `RenderingTracker` takes a sequence of assembled `EncodedFrame`s
// belonging to the same stream and produces a sequence of decoded and rendered
// `VideoFrame`s. This is done by calling the `VideoStreamBufferController` and
// passing the (fake) decoded frames through the `IncomingVideoStream`.
//
// The outputs of this class are interesting for evaluating the performance of
// the dejittering components of the video jitter buffer.
class RenderingTracker : public AssembledFrameCallback,
                         public FrameSchedulingReceiver,
                         public VideoStreamBufferControllerStatsObserver,
                         public VideoSinkInterface<VideoFrame> {
 public:
  // All members of the config should be explicitly set by the user.
  struct Config {
    uint32_t ssrc = 0;
    // Fixed render delay term added to the render timestamps.
    TimeDelta render_delay = TimeDelta::MinusInfinity();
  };

  RenderingTracker(const Environment& env,
                   const Config& config,
                   std::unique_ptr<VCMTiming> video_timing,
                   RenderingTrackerEvents* absl_nonnull observer);
  ~RenderingTracker() override;

  RenderingTracker(const RenderingTracker&) = delete;
  RenderingTracker& operator=(const RenderingTracker&) = delete;

  void SetDecodedFrameIdCallback(
      DecodedFrameIdCallback* absl_nonnull decoded_id_cb);

  // Inserts `assembled_frame` into the `VideoStreamBufferController` and logs
  // any rendered frames to the `observer_`.
  void OnAssembledFrame(std::unique_ptr<EncodedFrame> assembled_frame) override;

 private:
  struct VideoStreamBufferControllerObserverDecodableStats {
    TimeDelta jitter_buffer_delay = TimeDelta::Zero();
    TimeDelta jitter_buffer_target_delay = TimeDelta::Zero();
    TimeDelta jitter_buffer_minimum_delay = TimeDelta::Zero();
  };

  // Implements `FrameSchedulingReceiver`.
  void OnEncodedFrame(std::unique_ptr<EncodedFrame> encoded_frame) override;
  void OnDecodableFrameTimeout(TimeDelta wait_time) override;

  // Implements `VideoStreamBufferControllerStatsObserver`.
  void OnCompleteFrame(bool, size_t, VideoContentType) override {}
  void OnDroppedFrames(uint32_t frames_dropped) override;
  void OnDecodableFrame(TimeDelta jitter_buffer_delay,
                        TimeDelta jitter_buffer_target_delay,
                        TimeDelta jitter_buffer_minimum_delay) override;
  void OnFrameBufferTimingsUpdated(int, int, int, int, int, int) override {}
  void OnTimingFrameInfoUpdated(const TimingFrameInfo&) override {}

  // Implements `VideoSinkInterface<VideoFrame>`.
  void OnFrame(const VideoFrame& decoded_frame) override;

  void ResetVideoStreamBufferControllerObserverStats();

  // Environment.
  SequenceChecker sequence_checker_;
  const Environment env_;
  const Config config_;
  TaskQueueBase* const simulator_queue_;

  // Worker objects.
  std::unique_ptr<VCMTiming> video_timing_ RTC_GUARDED_BY(sequence_checker_);
  VideoStreamBufferController video_stream_buffer_controller_
      RTC_GUARDED_BY(sequence_checker_);
  std::unique_ptr<VideoSinkInterface<VideoFrame>> incoming_video_stream_
      RTC_GUARDED_BY(sequence_checker_);

  // Stats state. This is needed since the stats and the decodable frame are
  // provided by the VSBC on different callbacks, but we want to log the
  // the corresponding information simultaneously to our callback.
  std::optional<int> vsbc_frames_dropped_;
  std::optional<VideoStreamBufferControllerObserverDecodableStats>
      vsbc_decodable_stats_;

  // Outputs.
  RenderingTrackerEvents& observer_;
  DecodedFrameIdCallback* absl_nullable decoded_frame_id_cb_
      RTC_GUARDED_BY(sequence_checker_);

  // Task safety. By having this member be destroyed first, any outstanding
  // task referring to other members should not run.
  ScopedTaskSafety safety_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RENDERING_TRACKER_H_
