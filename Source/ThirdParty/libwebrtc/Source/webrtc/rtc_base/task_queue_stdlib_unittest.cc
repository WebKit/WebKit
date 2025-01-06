/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue_stdlib.h"

#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/task_queue_test.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory(
    const webrtc::FieldTrialsView*) {
  return CreateTaskQueueStdlibFactory();
}

INSTANTIATE_TEST_SUITE_P(TaskQueueStdlib,
                         TaskQueueTest,
                         ::testing::Values(CreateTaskQueueFactory));

class StringPtrLogSink : public rtc::LogSink {
 public:
  explicit StringPtrLogSink(std::string* log_data) : log_data_(log_data) {}

 private:
  void OnLogMessage(const std::string& message) override {
    OnLogMessage(absl::string_view(message));
  }
  void OnLogMessage(absl::string_view message) override {
    log_data_->append(message.begin(), message.end());
  }
  std::string* const log_data_;
};

TEST(TaskQueueStdlib, AvoidsSpammingLogOnInactivity) {
  std::string log_output;
  StringPtrLogSink stream(&log_output);
  rtc::LogMessage::AddLogToStream(&stream, rtc::LS_VERBOSE);
  auto task_queue = CreateTaskQueueStdlibFactory()->CreateTaskQueue(
      "test", TaskQueueFactory::Priority::NORMAL);
  auto wait_duration = rtc::Event::kDefaultWarnDuration + TimeDelta::Seconds(1);
  SleepMs(wait_duration.ms());
  EXPECT_EQ(log_output.length(), 0u);
  task_queue = nullptr;
  rtc::LogMessage::RemoveLogToStream(&stream);
}

}  // namespace
}  // namespace webrtc
