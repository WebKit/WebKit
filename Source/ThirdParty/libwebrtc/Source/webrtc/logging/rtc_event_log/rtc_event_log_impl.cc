/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log.h"

#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/rtceventlogoutput.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_new_format.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

#ifdef ENABLE_RTC_EVENT_LOG

namespace {
constexpr size_t kMaxEventsInHistory = 10000;
// The config-history is supposed to be unbounded, but needs to have some bound
// to prevent an attack via unreasonable memory use.
constexpr size_t kMaxEventsInConfigHistory = 1000;

// TODO(eladalon): This class exists because C++11 doesn't allow transferring a
// unique_ptr to a lambda (a copy constructor is required). We should get
// rid of this when we move to C++14.
template <typename T>
class ResourceOwningTask final : public rtc::QueuedTask {
 public:
  ResourceOwningTask(std::unique_ptr<T> resource,
                     std::function<void(std::unique_ptr<T>)> handler)
      : resource_(std::move(resource)), handler_(handler) {}

  bool Run() override {
    handler_(std::move(resource_));
    return true;
  }

 private:
  std::unique_ptr<T> resource_;
  std::function<void(std::unique_ptr<T>)> handler_;
};

std::unique_ptr<RtcEventLogEncoder> CreateEncoder(
    RtcEventLog::EncodingType type) {
  switch (type) {
    case RtcEventLog::EncodingType::Legacy:
      RTC_LOG(LS_INFO) << "Creating legacy encoder for RTC event log.";
      return absl::make_unique<RtcEventLogEncoderLegacy>();
    case RtcEventLog::EncodingType::NewFormat:
      RTC_LOG(LS_INFO) << "Creating new format encoder for RTC event log.";
      return absl::make_unique<RtcEventLogEncoderNewFormat>();
    default:
      RTC_LOG(LS_ERROR) << "Unknown RtcEventLog encoder type (" << int(type)
                        << ")";
      RTC_NOTREACHED();
      return std::unique_ptr<RtcEventLogEncoder>(nullptr);
  }
}

class RtcEventLogImpl final : public RtcEventLog {
 public:
  RtcEventLogImpl(std::unique_ptr<RtcEventLogEncoder> event_encoder,
                  std::unique_ptr<rtc::TaskQueue> task_queue);

  ~RtcEventLogImpl() override;

  // TODO(eladalon): We should change these name to reflect that what we're
  // actually starting/stopping is the output of the log, not the log itself.
  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override;
  void StopLogging() override;

  void Log(std::unique_ptr<RtcEvent> event) override;

 private:
  void LogToMemory(std::unique_ptr<RtcEvent> event) RTC_RUN_ON(task_queue_);
  void LogEventsFromMemoryToOutput() RTC_RUN_ON(task_queue_);

  void StopOutput() RTC_RUN_ON(task_queue_);

  void WriteConfigsAndHistoryToOutput(const std::string& encoded_configs,
                                      const std::string& encoded_history)
      RTC_RUN_ON(task_queue_);
  void WriteToOutput(const std::string& output_string) RTC_RUN_ON(task_queue_);

  void StopLoggingInternal() RTC_RUN_ON(task_queue_);

  void ScheduleOutput() RTC_RUN_ON(task_queue_);

  // Make sure that the event log is "managed" - created/destroyed, as well
  // as started/stopped - from the same thread/task-queue.
  rtc::SequencedTaskChecker owner_sequence_checker_;

  // History containing all past configuration events.
  std::deque<std::unique_ptr<RtcEvent>> config_history_
      RTC_GUARDED_BY(*task_queue_);

  // History containing the most recent (non-configuration) events (~10s).
  std::deque<std::unique_ptr<RtcEvent>> history_ RTC_GUARDED_BY(*task_queue_);

  size_t max_size_bytes_ RTC_GUARDED_BY(*task_queue_);
  size_t written_bytes_ RTC_GUARDED_BY(*task_queue_);

  std::unique_ptr<RtcEventLogEncoder> event_encoder_
      RTC_GUARDED_BY(*task_queue_);
  std::unique_ptr<RtcEventLogOutput> event_output_ RTC_GUARDED_BY(*task_queue_);

  size_t num_config_events_written_ RTC_GUARDED_BY(*task_queue_);
  int64_t output_period_ms_ RTC_GUARDED_BY(*task_queue_);
  int64_t last_output_ms_ RTC_GUARDED_BY(*task_queue_);
  bool output_scheduled_ RTC_GUARDED_BY(*task_queue_);

  // Since we are posting tasks bound to |this|,  it is critical that the event
  // log and it's members outlive the |task_queue_|. Keep the "task_queue_|
  // last to ensure it destructs first, or else tasks living on the queue might
  // access other members after they've been torn down.
  std::unique_ptr<rtc::TaskQueue> task_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogImpl);
};

RtcEventLogImpl::RtcEventLogImpl(
    std::unique_ptr<RtcEventLogEncoder> event_encoder,
    std::unique_ptr<rtc::TaskQueue> task_queue)
    : max_size_bytes_(std::numeric_limits<decltype(max_size_bytes_)>::max()),
      written_bytes_(0),
      event_encoder_(std::move(event_encoder)),
      num_config_events_written_(0),
      output_period_ms_(kImmediateOutput),
      last_output_ms_(rtc::TimeMillis()),
      output_scheduled_(false),
      task_queue_(std::move(task_queue)) {
  RTC_DCHECK(task_queue_);
}

RtcEventLogImpl::~RtcEventLogImpl() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  // If we're logging to the output, this will stop that. Blocking function.
  StopLogging();
}

bool RtcEventLogImpl::StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                                   int64_t output_period_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  RTC_DCHECK(output_period_ms == kImmediateOutput || output_period_ms > 0);

  if (!output->IsActive()) {
    // TODO(eladalon): We may want to remove the IsActive method. Otherwise
    // we probably want to be consistent and terminate any existing output.
    return false;
  }

  const int64_t timestamp_us = rtc::TimeMicros();
  const int64_t utc_time_us = rtc::TimeUTCMicros();
  RTC_LOG(LS_INFO) << "Starting WebRTC event log. (Timestamp, UTC) = "
                   << "(" << timestamp_us << ", " << utc_time_us << ").";

  // Binding to |this| is safe because |this| outlives the |task_queue_|.
  auto start = [this, timestamp_us,
                utc_time_us](std::unique_ptr<RtcEventLogOutput> output) {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    RTC_DCHECK(output->IsActive());
    event_output_ = std::move(output);
    num_config_events_written_ = 0;
    WriteToOutput(event_encoder_->EncodeLogStart(timestamp_us, utc_time_us));
    LogEventsFromMemoryToOutput();
  };

  task_queue_->PostTask(
      absl::make_unique<ResourceOwningTask<RtcEventLogOutput>>(
          std::move(output), start));

  return true;
}

void RtcEventLogImpl::StopLogging() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  RTC_LOG(LS_INFO) << "Stopping WebRTC event log.";

  rtc::Event output_stopped;

  // Binding to |this| is safe because |this| outlives the |task_queue_|.
  task_queue_->PostTask([this, &output_stopped]() {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    if (event_output_) {
      RTC_DCHECK(event_output_->IsActive());
      LogEventsFromMemoryToOutput();
    }
    StopLoggingInternal();
    output_stopped.Set();
  });

  output_stopped.Wait(rtc::Event::kForever);

  RTC_LOG(LS_INFO) << "WebRTC event log successfully stopped.";
}

void RtcEventLogImpl::Log(std::unique_ptr<RtcEvent> event) {
  RTC_CHECK(event);

  // Binding to |this| is safe because |this| outlives the |task_queue_|.
  auto event_handler = [this](std::unique_ptr<RtcEvent> unencoded_event) {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    LogToMemory(std::move(unencoded_event));
    if (event_output_)
      ScheduleOutput();
  };

  task_queue_->PostTask(absl::make_unique<ResourceOwningTask<RtcEvent>>(
      std::move(event), event_handler));
}

void RtcEventLogImpl::ScheduleOutput() {
  RTC_DCHECK(event_output_ && event_output_->IsActive());
  if (history_.size() >= kMaxEventsInHistory) {
    // We have to emergency drain the buffer. We can't wait for the scheduled
    // output task because there might be other event incoming before that.
    LogEventsFromMemoryToOutput();
    return;
  }

  if (output_period_ms_ == kImmediateOutput) {
    // We are already on the |task_queue_| so there is no reason to post a task
    // if we want to output immediately.
    LogEventsFromMemoryToOutput();
    return;
  }

  if (!output_scheduled_) {
    output_scheduled_ = true;
    // Binding to |this| is safe because |this| outlives the |task_queue_|.
    auto output_task = [this]() {
      RTC_DCHECK_RUN_ON(task_queue_.get());
      if (event_output_) {
        RTC_DCHECK(event_output_->IsActive());
        LogEventsFromMemoryToOutput();
      }
      output_scheduled_ = false;
    };
    int64_t now_ms = rtc::TimeMillis();
    int64_t time_since_output_ms = now_ms - last_output_ms_;
    uint32_t delay = rtc::SafeClamp(output_period_ms_ - time_since_output_ms, 0,
                                    output_period_ms_);
    task_queue_->PostDelayedTask(output_task, delay);
  }
}

void RtcEventLogImpl::LogToMemory(std::unique_ptr<RtcEvent> event) {
  std::deque<std::unique_ptr<RtcEvent>>& container =
      event->IsConfigEvent() ? config_history_ : history_;
  const size_t container_max_size =
      event->IsConfigEvent() ? kMaxEventsInConfigHistory : kMaxEventsInHistory;

  if (container.size() >= container_max_size) {
    RTC_DCHECK(!event_output_);  // Shouldn't lose events if we have an output.
    container.pop_front();
  }
  container.push_back(std::move(event));
}

void RtcEventLogImpl::LogEventsFromMemoryToOutput() {
  RTC_DCHECK(event_output_ && event_output_->IsActive());
  last_output_ms_ = rtc::TimeMillis();

  // Serialize all stream configurations that haven't already been written to
  // this output. |num_config_events_written_| is used to track which configs we
  // have already written. (Note that the config may have been written to
  // previous outputs; configs are not discarded.)
  std::string encoded_configs;
  RTC_DCHECK_LE(num_config_events_written_, config_history_.size());
  if (num_config_events_written_ < config_history_.size()) {
    const auto begin = config_history_.begin() + num_config_events_written_;
    const auto end = config_history_.end();
    encoded_configs = event_encoder_->EncodeBatch(begin, end);
    num_config_events_written_ = config_history_.size();
  }

  // Serialize the events in the event queue. Note that the write may fail,
  // for example if we are writing to a file and have reached the maximum limit.
  // We don't get any feedback if this happens, so we still remove the events
  // from the event log history. This is normally not a problem, but if another
  // log is started immediately after the first one becomes full, then one
  // cannot rely on the second log to contain everything that isn't in the first
  // log; one batch of events might be missing.
  std::string encoded_history =
      event_encoder_->EncodeBatch(history_.begin(), history_.end());
  history_.clear();

  WriteConfigsAndHistoryToOutput(encoded_configs, encoded_history);
}

void RtcEventLogImpl::WriteConfigsAndHistoryToOutput(
    const std::string& encoded_configs,
    const std::string& encoded_history) {
  // This function is used to merge the strings instead of calling the output
  // object twice with small strings. The function also avoids copying any
  // strings in the typical case where there are no config events.
  if (encoded_configs.size() == 0) {
    WriteToOutput(encoded_history);  // Typical case.
  } else if (encoded_history.size() == 0) {
    WriteToOutput(encoded_configs);  // Very unusual case.
  } else {
    WriteToOutput(encoded_configs + encoded_history);
  }
}

void RtcEventLogImpl::StopOutput() {
  max_size_bytes_ = std::numeric_limits<decltype(max_size_bytes_)>::max();
  written_bytes_ = 0;
  event_output_.reset();
}

void RtcEventLogImpl::StopLoggingInternal() {
  if (event_output_) {
    RTC_DCHECK(event_output_->IsActive());
    const int64_t timestamp_us = rtc::TimeMicros();
    event_output_->Write(event_encoder_->EncodeLogEnd(timestamp_us));
  }
  StopOutput();
}

void RtcEventLogImpl::WriteToOutput(const std::string& output_string) {
  RTC_DCHECK(event_output_ && event_output_->IsActive());
  if (!event_output_->Write(output_string)) {
    RTC_LOG(LS_ERROR) << "Failed to write RTC event to output.";
    // The first failure closes the output.
    RTC_DCHECK(!event_output_->IsActive());
    StopOutput();  // Clean-up.
    return;
  }
  written_bytes_ += output_string.size();
}

}  // namespace

#endif  // ENABLE_RTC_EVENT_LOG

// RtcEventLog member functions.
std::unique_ptr<RtcEventLog> RtcEventLog::Create(EncodingType encoding_type) {
  return Create(encoding_type,
                absl::make_unique<rtc::TaskQueue>("rtc_event_log"));
}

std::unique_ptr<RtcEventLog> RtcEventLog::Create(
    EncodingType encoding_type,
    std::unique_ptr<rtc::TaskQueue> task_queue) {
#ifdef ENABLE_RTC_EVENT_LOG
  return absl::make_unique<RtcEventLogImpl>(CreateEncoder(encoding_type),
                                            std::move(task_queue));
#else
  return CreateNull();
#endif  // ENABLE_RTC_EVENT_LOG
}

}  // namespace webrtc
