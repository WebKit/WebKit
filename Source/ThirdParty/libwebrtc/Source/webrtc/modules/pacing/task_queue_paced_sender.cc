/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/task_queue_paced_sender.h"

#include <algorithm>
#include <utility>
#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {
// If no calls to MaybeProcessPackets() happen, make sure we update stats
// at least every |kMaxTimeBetweenStatsUpdates| as long as the pacer isn't
// completely drained.
constexpr TimeDelta kMaxTimeBetweenStatsUpdates = TimeDelta::Millis(33);
// Don't call UpdateStats() more than |kMinTimeBetweenStatsUpdates| apart,
// for performance reasons.
constexpr TimeDelta kMinTimeBetweenStatsUpdates = TimeDelta::Millis(1);
}  // namespace

TaskQueuePacedSender::TaskQueuePacedSender(
    Clock* clock,
    PacketRouter* packet_router,
    RtcEventLog* event_log,
    const WebRtcKeyValueConfig* field_trials,
    TaskQueueFactory* task_queue_factory)
    : clock_(clock),
      packet_router_(packet_router),
      pacing_controller_(clock,
                         static_cast<PacingController::PacketSender*>(this),
                         event_log,
                         field_trials,
                         PacingController::ProcessMode::kDynamic),
      next_process_time_(Timestamp::MinusInfinity()),
      stats_update_scheduled_(false),
      last_stats_time_(Timestamp::MinusInfinity()),
      is_shutdown_(false),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "TaskQueuePacedSender",
          TaskQueueFactory::Priority::NORMAL)) {}

TaskQueuePacedSender::~TaskQueuePacedSender() {
  // Post an immediate task to mark the queue as shutting down.
  // The rtc::TaskQueue destructor will wait for pending tasks to
  // complete before continuing.
  task_queue_.PostTask([&]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    is_shutdown_ = true;
  });
}

void TaskQueuePacedSender::CreateProbeCluster(DataRate bitrate,
                                              int cluster_id) {
  task_queue_.PostTask([this, bitrate, cluster_id]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.CreateProbeCluster(bitrate, cluster_id);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::Pause() {
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.Pause();
  });
}

void TaskQueuePacedSender::Resume() {
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.Resume();
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetCongestionWindow(
    DataSize congestion_window_size) {
  task_queue_.PostTask([this, congestion_window_size]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetCongestionWindow(congestion_window_size);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::UpdateOutstandingData(DataSize outstanding_data) {
  if (task_queue_.IsCurrent()) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    // Fast path since this can be called once per sent packet while on the
    // task queue.
    pacing_controller_.UpdateOutstandingData(outstanding_data);
    return;
  }

  task_queue_.PostTask([this, outstanding_data]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.UpdateOutstandingData(outstanding_data);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetPacingRates(DataRate pacing_rate,
                                          DataRate padding_rate) {
  task_queue_.PostTask([this, pacing_rate, padding_rate]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetPacingRates(pacing_rate, padding_rate);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  task_queue_.PostTask([this, packets_ = std::move(packets)]() mutable {
    RTC_DCHECK_RUN_ON(&task_queue_);
    for (auto& packet : packets_) {
      pacing_controller_.EnqueuePacket(std::move(packet));
    }
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

void TaskQueuePacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  task_queue_.PostTask([this, account_for_audio]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetAccountForAudioPackets(account_for_audio);
  });
}

void TaskQueuePacedSender::SetIncludeOverhead() {
  task_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetIncludeOverhead();
  });
}

void TaskQueuePacedSender::SetTransportOverhead(DataSize overhead_per_packet) {
  task_queue_.PostTask([this, overhead_per_packet]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetTransportOverhead(overhead_per_packet);
  });
}

void TaskQueuePacedSender::SetQueueTimeLimit(TimeDelta limit) {
  task_queue_.PostTask([this, limit]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    pacing_controller_.SetQueueTimeLimit(limit);
    MaybeProcessPackets(Timestamp::MinusInfinity());
  });
}

TimeDelta TaskQueuePacedSender::ExpectedQueueTime() const {
  return GetStats().expected_queue_time;
}

DataSize TaskQueuePacedSender::QueueSizeData() const {
  return GetStats().queue_size;
}

absl::optional<Timestamp> TaskQueuePacedSender::FirstSentPacketTime() const {
  return GetStats().first_sent_packet_time;
}

TimeDelta TaskQueuePacedSender::OldestPacketWaitTime() const {
  return GetStats().oldest_packet_wait_time;
}

void TaskQueuePacedSender::MaybeProcessPackets(
    Timestamp scheduled_process_time) {
  RTC_DCHECK_RUN_ON(&task_queue_);

  if (is_shutdown_) {
    return;
  }

  const Timestamp now = clock_->CurrentTime();
  // Run ProcessPackets() only if this is the schedules task, or if there is
  // no scheduled task and we need to process immediately.
  if ((scheduled_process_time.IsFinite() &&
       scheduled_process_time == next_process_time_) ||
      (next_process_time_.IsInfinite() &&
       pacing_controller_.NextSendTime() <= now)) {
    pacing_controller_.ProcessPackets();
    next_process_time_ = Timestamp::MinusInfinity();
  }

  Timestamp next_process_time = std::max(now + PacingController::kMinSleepTime,
                                         pacing_controller_.NextSendTime());
  TimeDelta sleep_time = next_process_time - now;
  if (next_process_time_.IsMinusInfinity() ||
      next_process_time <=
          next_process_time_ - PacingController::kMinSleepTime) {
    next_process_time_ = next_process_time;

    task_queue_.PostDelayedTask(
        [this, next_process_time]() { MaybeProcessPackets(next_process_time); },
        sleep_time.ms<uint32_t>());
  }

  MaybeUpdateStats(false);
}

std::vector<std::unique_ptr<RtpPacketToSend>>
TaskQueuePacedSender::GeneratePadding(DataSize size) {
  return packet_router_->GeneratePadding(size.bytes());
}

void TaskQueuePacedSender::SendRtpPacket(
    std::unique_ptr<RtpPacketToSend> packet,
    const PacedPacketInfo& cluster_info) {
  packet_router_->SendPacket(std::move(packet), cluster_info);
}

void TaskQueuePacedSender::MaybeUpdateStats(bool is_scheduled_call) {
  if (is_shutdown_) {
    return;
  }

  Timestamp now = clock_->CurrentTime();
  if (!is_scheduled_call &&
      now - last_stats_time_ < kMinTimeBetweenStatsUpdates) {
    // Too frequent unscheduled stats update, return early.
    return;
  }

  rtc::CritScope cs(&stats_crit_);
  current_stats_.expected_queue_time = pacing_controller_.ExpectedQueueTime();
  current_stats_.first_sent_packet_time =
      pacing_controller_.FirstSentPacketTime();
  current_stats_.oldest_packet_wait_time =
      pacing_controller_.OldestPacketWaitTime();
  current_stats_.queue_size = pacing_controller_.QueueSizeData();
  last_stats_time_ = now;

  bool pacer_drained = pacing_controller_.QueueSizePackets() == 0 &&
                       pacing_controller_.CurrentBufferLevel().IsZero();

  // If there's anything interesting to get from the pacer and this is a
  // scheduled call (no scheduled call in flight), post a new scheduled stats
  // update.
  if (!pacer_drained && (is_scheduled_call || !stats_update_scheduled_)) {
    task_queue_.PostDelayedTask(
        [this]() {
          RTC_DCHECK_RUN_ON(&task_queue_);
          MaybeUpdateStats(true);
        },
        kMaxTimeBetweenStatsUpdates.ms<uint32_t>());
    stats_update_scheduled_ = true;
  } else {
    stats_update_scheduled_ = false;
  }
}

TaskQueuePacedSender::Stats TaskQueuePacedSender::GetStats() const {
  rtc::CritScope cs(&stats_crit_);
  return current_stats_;
}

}  // namespace webrtc
