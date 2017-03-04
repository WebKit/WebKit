/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/rtc_event_log_helper_thread.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/logging.h"

#ifdef ENABLE_RTC_EVENT_LOG

namespace webrtc {

namespace {
const int kEventsInHistory = 10000;

bool IsConfigEvent(const rtclog::Event& event) {
  rtclog::Event_EventType event_type = event.type();
  return event_type == rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT ||
         event_type == rtclog::Event::VIDEO_SENDER_CONFIG_EVENT ||
         event_type == rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT ||
         event_type == rtclog::Event::AUDIO_SENDER_CONFIG_EVENT;
}
}  // namespace

// RtcEventLogImpl member functions.
RtcEventLogHelperThread::RtcEventLogHelperThread(
    SwapQueue<ControlMessage>* message_queue,
    SwapQueue<std::unique_ptr<rtclog::Event>>* event_queue)
    : message_queue_(message_queue),
      event_queue_(event_queue),
      history_(kEventsInHistory),
      config_history_(),
      file_(FileWrapper::Create()),
      thread_(&ThreadOutputFunction, this, "RtcEventLog thread"),
      max_size_bytes_(std::numeric_limits<int64_t>::max()),
      written_bytes_(0),
      start_time_(0),
      stop_time_(std::numeric_limits<int64_t>::max()),
      has_recent_event_(false),
      most_recent_event_(),
      output_string_(),
      wake_periodically_(false, false),
      wake_from_hibernation_(false, false),
      file_finished_(false, false) {
  RTC_DCHECK(message_queue_);
  RTC_DCHECK(event_queue_);
  thread_.Start();
}

RtcEventLogHelperThread::~RtcEventLogHelperThread() {
  ControlMessage message;
  message.message_type = ControlMessage::TERMINATE_THREAD;
  message.stop_time = rtc::TimeMicros();
  while (!message_queue_->Insert(&message)) {
    // We can't destroy the event log until we have stopped the thread,
    // so clear the message queue and try again. Note that if we clear
    // any STOP_FILE events, then the threads calling StopLogging would likely
    // wait indefinitely. However, there should not be any such calls as we
    // are executing the destructor.
    LOG(LS_WARNING) << "Clearing message queue to terminate thread.";
    message_queue_->Clear();
  }
  wake_from_hibernation_.Set();
  wake_periodically_.Set();  // Wake up the output thread.
  thread_.Stop();            // Wait for the thread to terminate.
}

void RtcEventLogHelperThread::WaitForFileFinished() {
  wake_from_hibernation_.Set();
  wake_periodically_.Set();
  file_finished_.Wait(rtc::Event::kForever);
}

void RtcEventLogHelperThread::SignalNewEvent() {
  wake_from_hibernation_.Set();
}

bool RtcEventLogHelperThread::AppendEventToString(rtclog::Event* event) {
  rtclog::EventStream event_stream;
  event_stream.add_stream();
  event_stream.mutable_stream(0)->Swap(event);
  // We create a new event stream per event but because of the way protobufs
  // are encoded, events can be merged by concatenating them. Therefore,
  // it will look like a single stream when we read it back from file.
  bool stop = true;
  if (written_bytes_ + static_cast<int64_t>(output_string_.size()) +
          event_stream.ByteSize() <=
      max_size_bytes_) {
    event_stream.AppendToString(&output_string_);
    stop = false;
  }
  // Swap the event back so that we don't mix event types in the queues.
  event_stream.mutable_stream(0)->Swap(event);
  return stop;
}

bool RtcEventLogHelperThread::LogToMemory() {
  RTC_DCHECK(!file_->is_open());
  bool message_received = false;

  // Process each event earlier than the current time and append it to the
  // appropriate history_.
  int64_t current_time = rtc::TimeMicros();
  if (!has_recent_event_) {
    has_recent_event_ = event_queue_->Remove(&most_recent_event_);
  }
  while (has_recent_event_ &&
         most_recent_event_->timestamp_us() <= current_time) {
    if (IsConfigEvent(*most_recent_event_)) {
      config_history_.push_back(std::move(most_recent_event_));
    } else {
      history_.push_back(std::move(most_recent_event_));
    }
    has_recent_event_ = event_queue_->Remove(&most_recent_event_);
    message_received = true;
  }
  return message_received;
}

void RtcEventLogHelperThread::StartLogFile() {
  RTC_DCHECK(file_->is_open());
  bool stop = false;
  output_string_.clear();

  // Create and serialize the LOG_START event.
  rtclog::Event start_event;
  start_event.set_timestamp_us(start_time_);
  start_event.set_type(rtclog::Event::LOG_START);
  AppendEventToString(&start_event);

  // Serialize the config information for all old streams.
  for (auto& event : config_history_) {
    AppendEventToString(event.get());
  }

  // Serialize the events in the event queue.
  while (!history_.empty() && !stop) {
    stop = AppendEventToString(history_.front().get());
    if (!stop) {
      history_.pop_front();
    }
  }

  // Write to file.
  if (!file_->Write(output_string_.data(), output_string_.size())) {
    LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
    // The current FileWrapper implementation closes the file on error.
    RTC_DCHECK(!file_->is_open());
    return;
  }
  written_bytes_ += output_string_.size();

  // Free the allocated memory since we probably won't need this amount of
  // space again.
  output_string_.clear();
  output_string_.shrink_to_fit();

  if (stop) {
    RTC_DCHECK(file_->is_open());
    StopLogFile();
  }
}

bool RtcEventLogHelperThread::LogToFile() {
  RTC_DCHECK(file_->is_open());
  output_string_.clear();
  bool message_received = false;

  // Append each event older than both the current time and the stop time
  // to the output_string_.
  int64_t current_time = rtc::TimeMicros();
  int64_t time_limit = std::min(current_time, stop_time_);
  if (!has_recent_event_) {
    has_recent_event_ = event_queue_->Remove(&most_recent_event_);
  }
  bool stop = false;
  while (!stop && has_recent_event_ &&
         most_recent_event_->timestamp_us() <= time_limit) {
    stop = AppendEventToString(most_recent_event_.get());
    if (!stop) {
      if (IsConfigEvent(*most_recent_event_)) {
        config_history_.push_back(std::move(most_recent_event_));
      }
      has_recent_event_ = event_queue_->Remove(&most_recent_event_);
    }
    message_received = true;
  }

  // Write string to file.
  if (!file_->Write(output_string_.data(), output_string_.size())) {
    LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
    // The current FileWrapper implementation closes the file on error.
    RTC_DCHECK(!file_->is_open());
    return message_received;
  }
  written_bytes_ += output_string_.size();

  // We want to stop logging if we have reached the file size limit. We also
  // want to stop logging if the remaining events are more recent than the
  // time limit, or in other words if we have terminated the loop despite
  // having more events in the queue.
  if ((has_recent_event_ && most_recent_event_->timestamp_us() > stop_time_) ||
      stop) {
    RTC_DCHECK(file_->is_open());
    StopLogFile();
  }
  return message_received;
}

void RtcEventLogHelperThread::StopLogFile() {
  RTC_DCHECK(file_->is_open());
  output_string_.clear();

  rtclog::Event end_event;
  // This function can be called either because we have reached the stop time,
  // or because we have reached the log file size limit. Therefore, use the
  // current time if we have not reached the time limit.
  end_event.set_timestamp_us(
      std::min(stop_time_, rtc::TimeMicros()));
  end_event.set_type(rtclog::Event::LOG_END);
  AppendEventToString(&end_event);

  if (written_bytes_ + static_cast<int64_t>(output_string_.size()) <=
      max_size_bytes_) {
    if (!file_->Write(output_string_.data(), output_string_.size())) {
      LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
      // The current FileWrapper implementation closes the file on error.
      RTC_DCHECK(!file_->is_open());
    }
    written_bytes_ += output_string_.size();
  }

  max_size_bytes_ = std::numeric_limits<int64_t>::max();
  written_bytes_ = 0;
  start_time_ = 0;
  stop_time_ = std::numeric_limits<int64_t>::max();
  output_string_.clear();
  file_->CloseFile();
  RTC_DCHECK(!file_->is_open());
}

void RtcEventLogHelperThread::ProcessEvents() {
  ControlMessage message;

  while (true) {
    bool message_received = false;
    // Process control messages.
    while (message_queue_->Remove(&message)) {
      switch (message.message_type) {
        case ControlMessage::START_FILE:
          if (!file_->is_open()) {
            max_size_bytes_ = message.max_size_bytes;
            start_time_ = message.start_time;
            stop_time_ = message.stop_time;
            file_.swap(message.file);
            StartLogFile();
          } else {
            // Already started. Ignore message and close file handle.
            message.file->CloseFile();
          }
          message_received = true;
          break;
        case ControlMessage::STOP_FILE:
          if (file_->is_open()) {
            stop_time_ = message.stop_time;
            LogToFile();  // Log remaining events from message queues.
          }
          // LogToFile might stop on it's own so we need to recheck the state.
          if (file_->is_open()) {
            StopLogFile();
          }
          file_finished_.Set();
          message_received = true;
          break;
        case ControlMessage::TERMINATE_THREAD:
          if (file_->is_open()) {
            StopLogFile();
          }
          return;
      }
    }

    // Write events to file or memory.
    if (file_->is_open()) {
      message_received |= LogToFile();
    } else {
      message_received |= LogToMemory();
    }

    // Accumulate a new batch of events instead of processing them one at a
    // time.
    if (message_received) {
      wake_periodically_.Wait(100);
    } else {
      wake_from_hibernation_.Wait(rtc::Event::kForever);
    }
  }
}

void RtcEventLogHelperThread::ThreadOutputFunction(void* obj) {
  RtcEventLogHelperThread* helper = static_cast<RtcEventLogHelperThread*>(obj);
  helper->ProcessEvents();
}

}  // namespace webrtc

#endif  // ENABLE_RTC_EVENT_LOG
