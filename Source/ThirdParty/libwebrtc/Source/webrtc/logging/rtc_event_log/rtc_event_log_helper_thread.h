/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_HELPER_THREAD_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_HELPER_THREAD_H_

#include <deque>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/event.h"
#include "webrtc/base/ignore_wundef.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/protobuf_utils.h"
#include "webrtc/base/swap_queue.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"

#ifdef ENABLE_RTC_EVENT_LOG
// *.ph.h files are generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()
#endif

#ifdef ENABLE_RTC_EVENT_LOG

namespace webrtc {

class RtcEventLogHelperThread final {
 public:
  struct ControlMessage {
    ControlMessage()
        : message_type(STOP_FILE),
          file(nullptr),
          max_size_bytes(0),
          start_time(0),
          stop_time(0) {}
    enum { START_FILE, STOP_FILE, TERMINATE_THREAD } message_type;

    std::unique_ptr<FileWrapper> file;  // Only used with START_FILE.
    int64_t max_size_bytes;             // Only used with START_FILE.
    int64_t start_time;                 // Only used with START_FILE.
    int64_t stop_time;                  // Used with all 3 message types.

    friend void swap(ControlMessage& lhs, ControlMessage& rhs) {
      using std::swap;
      swap(lhs.message_type, rhs.message_type);
      lhs.file.swap(rhs.file);
      swap(lhs.max_size_bytes, rhs.max_size_bytes);
      swap(lhs.start_time, rhs.start_time);
      swap(lhs.stop_time, rhs.stop_time);
    }
  };

  RtcEventLogHelperThread(
      SwapQueue<ControlMessage>* message_queue,
      SwapQueue<std::unique_ptr<rtclog::Event>>* event_queue);
  ~RtcEventLogHelperThread();

  // This function MUST be called once a STOP_FILE message is added to the
  // signalling queue. The function will make sure that the output thread
  // wakes up to read the message, and it blocks until the output thread has
  // finished writing to the file.
  void WaitForFileFinished();

  // This fuction MUST be called once an event is added to the event queue.
  void SignalNewEvent();

 private:
  static void ThreadOutputFunction(void* obj);

  bool AppendEventToString(rtclog::Event* event);
  bool LogToMemory();
  void StartLogFile();
  bool LogToFile();
  void StopLogFile();
  void ProcessEvents();

  // Message queues for passing events to the logging thread.
  SwapQueue<ControlMessage>* message_queue_;
  SwapQueue<std::unique_ptr<rtclog::Event>>* event_queue_;

  // History containing the most recent events (~ 10 s).
  std::deque<std::unique_ptr<rtclog::Event>> history_;

  // History containing all past configuration events.
  std::vector<std::unique_ptr<rtclog::Event>> config_history_;

  std::unique_ptr<FileWrapper> file_;
  rtc::PlatformThread thread_;

  int64_t max_size_bytes_;
  int64_t written_bytes_;
  int64_t start_time_;
  int64_t stop_time_;

  bool has_recent_event_;
  std::unique_ptr<rtclog::Event> most_recent_event_;

  // Temporary space for serializing profobuf data.
  ProtoString output_string_;

  rtc::Event wake_periodically_;
  rtc::Event wake_from_hibernation_;
  rtc::Event file_finished_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcEventLogHelperThread);
};

}  // namespace webrtc

#endif  // ENABLE_RTC_EVENT_LOG

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_HELPER_THREAD_H_
