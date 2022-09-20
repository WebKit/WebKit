/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_buffer_controller.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/functional/bind_front.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/data_size.h"
#include "api/video/encoded_frame.h"
#include "api/video/frame_buffer.h"
#include "api/video/video_content_type.h"
#include "modules/video_coding/frame_helpers.h"
#include "modules/video_coding/timing/inter_frame_delay.h"
#include "modules/video_coding/timing/jitter_estimator.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread_annotations.h"
#include "video/frame_decode_timing.h"
#include "video/task_queue_frame_decode_scheduler.h"
#include "video/video_receive_stream_timeout_tracker.h"

namespace webrtc {

namespace {

// Max number of frames the buffer will hold.
static constexpr size_t kMaxFramesBuffered = 800;
// Max number of decoded frame info that will be saved.
static constexpr int kMaxFramesHistory = 1 << 13;

// Default value for the maximum decode queue size that is used when the
// low-latency renderer is used.
static constexpr size_t kZeroPlayoutDelayDefaultMaxDecodeQueueSize = 8;

struct FrameMetadata {
  explicit FrameMetadata(const EncodedFrame& frame)
      : is_last_spatial_layer(frame.is_last_spatial_layer),
        is_keyframe(frame.is_keyframe()),
        size(frame.size()),
        contentType(frame.contentType()),
        delayed_by_retransmission(frame.delayed_by_retransmission()),
        rtp_timestamp(frame.Timestamp()),
        receive_time(frame.ReceivedTimestamp()) {}

  const bool is_last_spatial_layer;
  const bool is_keyframe;
  const size_t size;
  const VideoContentType contentType;
  const bool delayed_by_retransmission;
  const uint32_t rtp_timestamp;
  const absl::optional<Timestamp> receive_time;
};

Timestamp ReceiveTime(const EncodedFrame& frame) {
  absl::optional<Timestamp> ts = frame.ReceivedTimestamp();
  RTC_DCHECK(ts.has_value()) << "Received frame must have a timestamp set!";
  return *ts;
}

enum class FrameBufferArm {
  kFrameBuffer3,
  kSyncDecode,
};

constexpr const char* kFrameBufferFieldTrial = "WebRTC-FrameBuffer3";

FrameBufferArm ParseFrameBufferFieldTrial(const FieldTrialsView& field_trials) {
  webrtc::FieldTrialEnum<FrameBufferArm> arm(
      "arm", FrameBufferArm::kFrameBuffer3,
      {
          {"FrameBuffer3", FrameBufferArm::kFrameBuffer3},
          {"SyncDecoding", FrameBufferArm::kSyncDecode},
      });
  ParseFieldTrial({&arm}, field_trials.Lookup(kFrameBufferFieldTrial));
  return arm.Get();
}

}  // namespace

std::unique_ptr<VideoStreamBufferController>
VideoStreamBufferController::CreateFromFieldTrial(
    Clock* clock,
    TaskQueueBase* worker_queue,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_proxy,
    FrameSchedulingReceiver* receiver,
    TimeDelta max_wait_for_keyframe,
    TimeDelta max_wait_for_frame,
    DecodeSynchronizer* decode_sync,
    const FieldTrialsView& field_trials) {
  switch (ParseFrameBufferFieldTrial(field_trials)) {
    case FrameBufferArm::kSyncDecode: {
      std::unique_ptr<FrameDecodeScheduler> scheduler;
      if (decode_sync) {
        scheduler = decode_sync->CreateSynchronizedFrameScheduler();
      } else {
        RTC_LOG(LS_ERROR) << "In FrameBuffer with sync decode trial, but "
                             "no DecodeSynchronizer was present!";
        // Crash in debug, but in production use the task queue scheduler.
        RTC_DCHECK_NOTREACHED();
        scheduler = std::make_unique<TaskQueueFrameDecodeScheduler>(
            clock, worker_queue);
      }
      return std::make_unique<VideoStreamBufferController>(
          clock, worker_queue, timing, stats_proxy, receiver,
          max_wait_for_keyframe, max_wait_for_frame, std::move(scheduler),
          field_trials);
    }
    case FrameBufferArm::kFrameBuffer3:
      ABSL_FALLTHROUGH_INTENDED;
    default: {
      auto scheduler =
          std::make_unique<TaskQueueFrameDecodeScheduler>(clock, worker_queue);
      return std::make_unique<VideoStreamBufferController>(
          clock, worker_queue, timing, stats_proxy, receiver,
          max_wait_for_keyframe, max_wait_for_frame, std::move(scheduler),
          field_trials);
    }
  }
}

VideoStreamBufferController::VideoStreamBufferController(
    Clock* clock,
    TaskQueueBase* worker_queue,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_proxy,
    FrameSchedulingReceiver* receiver,
    TimeDelta max_wait_for_keyframe,
    TimeDelta max_wait_for_frame,
    std::unique_ptr<FrameDecodeScheduler> frame_decode_scheduler,
    const FieldTrialsView& field_trials)
    : field_trials_(field_trials),
      clock_(clock),
      stats_proxy_(stats_proxy),
      receiver_(receiver),
      timing_(timing),
      frame_decode_scheduler_(std::move(frame_decode_scheduler)),
      jitter_estimator_(clock_, field_trials),
      buffer_(std::make_unique<FrameBuffer>(kMaxFramesBuffered,
                                            kMaxFramesHistory,
                                            field_trials)),
      decode_timing_(clock_, timing_),
      timeout_tracker_(
          clock_,
          worker_queue,
          VideoReceiveStreamTimeoutTracker::Timeouts{
              .max_wait_for_keyframe = max_wait_for_keyframe,
              .max_wait_for_frame = max_wait_for_frame},
          absl::bind_front(&VideoStreamBufferController::OnTimeout, this)),
      zero_playout_delay_max_decode_queue_size_(
          "max_decode_queue_size",
          kZeroPlayoutDelayDefaultMaxDecodeQueueSize) {
  RTC_DCHECK(stats_proxy_);
  RTC_DCHECK(receiver_);
  RTC_DCHECK(timing_);
  RTC_DCHECK(clock_);
  RTC_DCHECK(frame_decode_scheduler_);
  RTC_LOG(LS_WARNING) << "Using FrameBuffer3";

  ParseFieldTrial({&zero_playout_delay_max_decode_queue_size_},
                  field_trials.Lookup("WebRTC-ZeroPlayoutDelay"));
}

void VideoStreamBufferController::Stop() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  frame_decode_scheduler_->Stop();
  timeout_tracker_.Stop();
  decoder_ready_for_new_frame_ = false;
}

void VideoStreamBufferController::SetProtectionMode(
    VCMVideoProtection protection_mode) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  protection_mode_ = protection_mode;
}

void VideoStreamBufferController::Clear() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  stats_proxy_->OnDroppedFrames(buffer_->CurrentSize());
  buffer_ = std::make_unique<FrameBuffer>(kMaxFramesBuffered, kMaxFramesHistory,
                                          field_trials_);
  frame_decode_scheduler_->CancelOutstanding();
}

absl::optional<int64_t> VideoStreamBufferController::InsertFrame(
    std::unique_ptr<EncodedFrame> frame) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  FrameMetadata metadata(*frame);
  int complete_units = buffer_->GetTotalNumberOfContinuousTemporalUnits();
  if (buffer_->InsertFrame(std::move(frame))) {
    RTC_DCHECK(metadata.receive_time) << "Frame receive time must be set!";
    if (!metadata.delayed_by_retransmission && metadata.receive_time)
      timing_->IncomingTimestamp(metadata.rtp_timestamp,
                                 *metadata.receive_time);
    if (complete_units < buffer_->GetTotalNumberOfContinuousTemporalUnits()) {
      stats_proxy_->OnCompleteFrame(metadata.is_keyframe, metadata.size,
                                    metadata.contentType);
      MaybeScheduleFrameForRelease();
    }
  }

  return buffer_->LastContinuousFrameId();
}

void VideoStreamBufferController::UpdateRtt(int64_t max_rtt_ms) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  jitter_estimator_.UpdateRtt(TimeDelta::Millis(max_rtt_ms));
}

void VideoStreamBufferController::SetMaxWaits(TimeDelta max_wait_for_keyframe,
                                              TimeDelta max_wait_for_frame) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  timeout_tracker_.SetTimeouts({.max_wait_for_keyframe = max_wait_for_keyframe,
                                .max_wait_for_frame = max_wait_for_frame});
}

void VideoStreamBufferController::StartNextDecode(bool keyframe_required) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  if (!timeout_tracker_.Running())
    timeout_tracker_.Start(keyframe_required);
  keyframe_required_ = keyframe_required;
  if (keyframe_required_) {
    timeout_tracker_.SetWaitingForKeyframe();
  }
  decoder_ready_for_new_frame_ = true;
  MaybeScheduleFrameForRelease();
}

int VideoStreamBufferController::Size() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  return buffer_->CurrentSize();
}

void VideoStreamBufferController::OnFrameReady(
    absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames,
    Timestamp render_time) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  RTC_DCHECK(!frames.empty());

  timeout_tracker_.OnEncodedFrameReleased();

  Timestamp now = clock_->CurrentTime();
  bool superframe_delayed_by_retransmission = false;
  DataSize superframe_size = DataSize::Zero();
  const EncodedFrame& first_frame = *frames.front();
  Timestamp receive_time = ReceiveTime(first_frame);

  if (first_frame.is_keyframe())
    keyframe_required_ = false;

  // Gracefully handle bad RTP timestamps and render time issues.
  if (FrameHasBadRenderTiming(render_time, now, timing_->TargetVideoDelay())) {
    jitter_estimator_.Reset();
    timing_->Reset();
    render_time = timing_->RenderTime(first_frame.Timestamp(), now);
  }

  for (std::unique_ptr<EncodedFrame>& frame : frames) {
    frame->SetRenderTime(render_time.ms());

    superframe_delayed_by_retransmission |= frame->delayed_by_retransmission();
    receive_time = std::max(receive_time, ReceiveTime(*frame));
    superframe_size += DataSize::Bytes(frame->size());
  }

  if (!superframe_delayed_by_retransmission) {
    auto frame_delay = inter_frame_delay_.CalculateDelay(
        first_frame.Timestamp(), receive_time);
    if (frame_delay) {
      jitter_estimator_.UpdateEstimate(*frame_delay, superframe_size);
    }

    float rtt_mult = protection_mode_ == kProtectionNackFEC ? 0.0 : 1.0;
    absl::optional<TimeDelta> rtt_mult_add_cap_ms = absl::nullopt;
    if (rtt_mult_settings_.has_value()) {
      rtt_mult = rtt_mult_settings_->rtt_mult_setting;
      rtt_mult_add_cap_ms =
          TimeDelta::Millis(rtt_mult_settings_->rtt_mult_add_cap_ms);
    }
    timing_->SetJitterDelay(
        jitter_estimator_.GetJitterEstimate(rtt_mult, rtt_mult_add_cap_ms));
    timing_->UpdateCurrentDelay(render_time, now);
  } else if (RttMultExperiment::RttMultEnabled()) {
    jitter_estimator_.FrameNacked();
  }

  // Update stats.
  UpdateDroppedFrames();
  UpdateJitterDelay();
  UpdateTimingFrameInfo();

  std::unique_ptr<EncodedFrame> frame =
      CombineAndDeleteFrames(std::move(frames));

  timing_->SetLastDecodeScheduledTimestamp(now);

  decoder_ready_for_new_frame_ = false;
  receiver_->OnEncodedFrame(std::move(frame));
}

void VideoStreamBufferController::OnTimeout(TimeDelta delay) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);

  // Stop sending timeouts until receiver starts waiting for a new frame.
  timeout_tracker_.Stop();

  // If the stream is paused then ignore the timeout.
  if (!decoder_ready_for_new_frame_) {
    return;
  }
  decoder_ready_for_new_frame_ = false;
  receiver_->OnDecodableFrameTimeout(delay);
}

void VideoStreamBufferController::FrameReadyForDecode(uint32_t rtp_timestamp,
                                                      Timestamp render_time) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  auto frames = buffer_->ExtractNextDecodableTemporalUnit();
  RTC_DCHECK(frames[0]->Timestamp() == rtp_timestamp)
      << "Frame buffer's next decodable frame was not the one sent for "
         "extraction rtp="
      << rtp_timestamp << " extracted rtp=" << frames[0]->Timestamp();
  OnFrameReady(std::move(frames), render_time);
}

void VideoStreamBufferController::UpdateDroppedFrames()
    RTC_RUN_ON(&worker_sequence_checker_) {
  const int dropped_frames = buffer_->GetTotalNumberOfDroppedFrames() -
                             frames_dropped_before_last_new_frame_;
  if (dropped_frames > 0)
    stats_proxy_->OnDroppedFrames(dropped_frames);
  frames_dropped_before_last_new_frame_ =
      buffer_->GetTotalNumberOfDroppedFrames();
}

void VideoStreamBufferController::UpdateJitterDelay() {
  auto timings = timing_->GetTimings();
  if (timings.num_decoded_frames) {
    stats_proxy_->OnFrameBufferTimingsUpdated(
        timings.max_decode_duration.ms(), timings.current_delay.ms(),
        timings.target_delay.ms(), timings.jitter_buffer_delay.ms(),
        timings.min_playout_delay.ms(), timings.render_delay.ms());
  }
}

void VideoStreamBufferController::UpdateTimingFrameInfo() {
  absl::optional<TimingFrameInfo> info = timing_->GetTimingFrameInfo();
  if (info)
    stats_proxy_->OnTimingFrameInfoUpdated(*info);
}

bool VideoStreamBufferController::IsTooManyFramesQueued() const
    RTC_RUN_ON(&worker_sequence_checker_) {
  return buffer_->CurrentSize() > zero_playout_delay_max_decode_queue_size_;
}

void VideoStreamBufferController::ForceKeyFrameReleaseImmediately()
    RTC_RUN_ON(&worker_sequence_checker_) {
  RTC_DCHECK(keyframe_required_);
  // Iterate through the frame buffer until there is a complete keyframe and
  // release this right away.
  while (buffer_->DecodableTemporalUnitsInfo()) {
    auto next_frame = buffer_->ExtractNextDecodableTemporalUnit();
    if (next_frame.empty()) {
      RTC_DCHECK_NOTREACHED()
          << "Frame buffer should always return at least 1 frame.";
      continue;
    }
    // Found keyframe - decode right away.
    if (next_frame.front()->is_keyframe()) {
      auto render_time = timing_->RenderTime(next_frame.front()->Timestamp(),
                                             clock_->CurrentTime());
      OnFrameReady(std::move(next_frame), render_time);
      return;
    }
  }
}

void VideoStreamBufferController::MaybeScheduleFrameForRelease()
    RTC_RUN_ON(&worker_sequence_checker_) {
  auto decodable_tu_info = buffer_->DecodableTemporalUnitsInfo();
  if (!decoder_ready_for_new_frame_ || !decodable_tu_info) {
    return;
  }

  if (keyframe_required_) {
    return ForceKeyFrameReleaseImmediately();
  }

  // If already scheduled then abort.
  if (frame_decode_scheduler_->ScheduledRtpTimestamp() ==
      decodable_tu_info->next_rtp_timestamp) {
    return;
  }

  TimeDelta max_wait = timeout_tracker_.TimeUntilTimeout();
  // Ensures the frame is scheduled for decode before the stream times out.
  // This is otherwise a race condition.
  max_wait = std::max(max_wait - TimeDelta::Millis(1), TimeDelta::Zero());
  absl::optional<FrameDecodeTiming::FrameSchedule> schedule;
  while (decodable_tu_info) {
    schedule = decode_timing_.OnFrameBufferUpdated(
        decodable_tu_info->next_rtp_timestamp,
        decodable_tu_info->last_rtp_timestamp, max_wait,
        IsTooManyFramesQueued());
    if (schedule) {
      // Don't schedule if already waiting for the same frame.
      if (frame_decode_scheduler_->ScheduledRtpTimestamp() !=
          decodable_tu_info->next_rtp_timestamp) {
        frame_decode_scheduler_->CancelOutstanding();
        frame_decode_scheduler_->ScheduleFrame(
            decodable_tu_info->next_rtp_timestamp, *schedule,
            absl::bind_front(&VideoStreamBufferController::FrameReadyForDecode,
                             this));
      }
      return;
    }
    // If no schedule for current rtp, drop and try again.
    buffer_->DropNextDecodableTemporalUnit();
    decodable_tu_info = buffer_->DecodableTemporalUnitsInfo();
  }
}

}  // namespace webrtc
