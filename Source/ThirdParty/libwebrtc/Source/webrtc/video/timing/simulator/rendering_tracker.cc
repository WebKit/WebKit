/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rendering_tracker.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_frame.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/timing/timing.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/render/incoming_video_stream.h"
#include "video/task_queue_frame_decode_scheduler.h"
#include "video/timing/simulator/assembler.h"

namespace webrtc::video_timing_simulator {

namespace {

constexpr TimeDelta kMaxWaitForKeyframe = TimeDelta::Seconds(10);
constexpr TimeDelta kMaxWaitForFrame = TimeDelta::Seconds(10);

// TODO: b/423646186 - Consider adding some variability to the decode time, and
// update VCMTiming accordingly.
VideoFrame SimulateDecode(const EncodedFrame& encoded_frame) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(I420Buffer::Create(/*width=*/1, /*height=*/1))
      .set_timestamp_us(encoded_frame.RenderTimestamp()->us())
      .set_timestamp_rtp(encoded_frame.RtpTimestamp())
      // The `id` needs to be unwrapped by the consumer.
      .set_id(static_cast<uint16_t>(encoded_frame.Id()))
      .set_packet_infos(encoded_frame.PacketInfos())
      .build();
}

}  // namespace

RenderingTracker::RenderingTracker(const Environment& env,
                                   const Config& config,
                                   std::unique_ptr<VCMTiming> video_timing,
                                   RenderingTrackerEvents* absl_nonnull
                                       observer)
    : env_(env),
      config_(config),
      simulator_queue_(TaskQueueBase::Current()),
      video_timing_(std::move(video_timing)),
      video_stream_buffer_controller_(
          &env.clock(),
          simulator_queue_,
          video_timing_.get(),
          /*stats_proxy=*/this,
          /*receiver=*/this,
          kMaxWaitForKeyframe,
          kMaxWaitForFrame,
          std::make_unique<TaskQueueFrameDecodeScheduler>(&env_.clock(),
                                                          simulator_queue_),
          env_.field_trials()),
      incoming_video_stream_(
          std::make_unique<IncomingVideoStream>(env_,
                                                config_.render_delay.ms(),
                                                /*callback=*/this)),
      observer_(*observer),
      decoded_frame_id_cb_(nullptr) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Validation.
  RTC_DCHECK_NE(config.ssrc, 0u);
  RTC_DCHECK(config.render_delay.IsFinite());
  // Setup.
  ResetVideoStreamBufferControllerObserverStats();
  video_timing_->set_render_delay(config_.render_delay);
  video_stream_buffer_controller_.StartNextDecode(/*keyframe_required=*/true);
}

RenderingTracker::~RenderingTracker() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  video_stream_buffer_controller_.Stop();
}

void RenderingTracker::SetDecodedFrameIdCallback(
    DecodedFrameIdCallback* absl_nonnull decoded_frame_id_cb) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  decoded_frame_id_cb_ = decoded_frame_id_cb;
}

void RenderingTracker::OnAssembledFrame(
    std::unique_ptr<EncodedFrame> assembled_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  int64_t frame_id = assembled_frame->Id();
  bool is_keyframe = assembled_frame->is_keyframe();
  std::optional<int64_t> last_continuous_frame_id =
      video_stream_buffer_controller_.InsertFrame(std::move(assembled_frame));
  if (!last_continuous_frame_id.has_value()) {
    RTC_LOG(LS_WARNING) << "VideoStreamBufferController did not report "
                           "continuity for inserted frame_id="
                        << frame_id << ", is_keyframe=" << is_keyframe
                        << " on ssrc=" << config_.ssrc
                        << " (simulated_ts=" << env_.clock().CurrentTime()
                        << ")";
  }
}

void RenderingTracker::OnEncodedFrame(
    std::unique_ptr<EncodedFrame> encoded_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_CHECK(decoded_frame_id_cb_) << "Callback must be set before running";

  VideoFrame decoded_frame = SimulateDecode(*encoded_frame);

  // Verify expected callback order from the VideoStreamBufferController.
  // This check is currently true by construction, but it could change in
  // the future.
  RTC_DCHECK(vsbc_decodable_stats_.has_value());
  VideoStreamBufferControllerObserverDecodableStats vsbc_decodable_stats =
      vsbc_decodable_stats_.value_or(
          VideoStreamBufferControllerObserverDecodableStats());
  observer_.OnDecodedFrame(*encoded_frame, vsbc_frames_dropped_.value_or(0),
                           vsbc_decodable_stats.jitter_buffer_minimum_delay,
                           vsbc_decodable_stats.jitter_buffer_target_delay,
                           vsbc_decodable_stats.jitter_buffer_delay);
  decoded_frame_id_cb_->OnDecodedFrameId(encoded_frame->Id());
  encoded_frame.reset();  // Just to be explicit.

  // We need to "stop the decode timer", in order for `video_timing_` to know
  // that a frame was "decoded".
  // TODO: b/423646186 - Consider introducing a decode time delay model.
  // See `SimulateDecode()` below.
  video_timing_->StopDecodeTimer(/*decode_time=*/TimeDelta::Zero(),
                                 env_.clock().CurrentTime());

  // Send the "decoded" video frame for "rendering".
  // TODO: b/423646186 - Consider making this step configurable, since Chromium
  // disables "prerender smoothing".
  incoming_video_stream_->OnFrame(decoded_frame);

  // Get ready for the next decode.
  ResetVideoStreamBufferControllerObserverStats();
  video_stream_buffer_controller_.StartNextDecode(
      /*keyframe_required=*/false);
}

void RenderingTracker::OnDecodableFrameTimeout(TimeDelta wait_time) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Log as verbose because this timeout will periodically trigger when the
  // stream has been deregistered. Unfortunately there is no log event for the
  // deregistration, so the simulator will keep expecting packets forever.
  RTC_LOG(LS_VERBOSE) << "VideoStreamBufferController timed out after wait_ms="
                      << wait_time << " on ssrc=" << config_.ssrc
                      << " (simulated_ts=" << env_.clock().CurrentTime() << ")";
  // TODO: b/423646186 - Consider adding this as a callback event.
  video_stream_buffer_controller_.StartNextDecode(/*keyframe_required=*/true);
}

void RenderingTracker::OnDroppedFrames(uint32_t frames_dropped) {
  vsbc_frames_dropped_ = frames_dropped;
}

void RenderingTracker::OnDecodableFrame(TimeDelta jitter_buffer_delay,
                                        TimeDelta jitter_buffer_target_delay,
                                        TimeDelta jitter_buffer_minimum_delay) {
  vsbc_decodable_stats_ = VideoStreamBufferControllerObserverDecodableStats{
      .jitter_buffer_delay = jitter_buffer_delay,
      .jitter_buffer_target_delay = jitter_buffer_target_delay,
      .jitter_buffer_minimum_delay = jitter_buffer_minimum_delay};
}

void RenderingTracker::OnFrame(const VideoFrame& decoded_frame) {
  // `IncomingVideoStream` will call back on its own TaskQueue, so we copy
  // `decoded_frame` and post over to the `simulator_queue_` here...
  if (TaskQueueBase::Current() != simulator_queue_) {
    simulator_queue_->PostTask(SafeTask(
        safety_.flag(),
        [this, decoded_frame]() { observer_.OnRenderedFrame(decoded_frame); }));
    return;
  }
  // ...and in case that ever changes, we still call back here.
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  observer_.OnRenderedFrame(decoded_frame);
}

void RenderingTracker::ResetVideoStreamBufferControllerObserverStats() {
  vsbc_frames_dropped_.reset();
  vsbc_decodable_stats_.reset();
}

}  // namespace webrtc::video_timing_simulator
