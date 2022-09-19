/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/real_time_controller.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/null_socket_server.h"

namespace webrtc {
namespace {
class MainThread : public rtc::Thread {
 public:
  MainThread()
      : Thread(std::make_unique<rtc::NullSocketServer>(), false),
        current_setter_(this) {
    DoInit();
  }
  ~MainThread() {
    Stop();
    DoDestroy();
  }

 private:
  CurrentThreadSetter current_setter_;
};
}  // namespace
RealTimeController::RealTimeController()
    : task_queue_factory_(CreateDefaultTaskQueueFactory()),
      main_thread_(std::make_unique<MainThread>()) {
  main_thread_->SetName("Main", this);
}

Clock* RealTimeController::GetClock() {
  return Clock::GetRealTimeClock();
}

TaskQueueFactory* RealTimeController::GetTaskQueueFactory() {
  return task_queue_factory_.get();
}

std::unique_ptr<rtc::Thread> RealTimeController::CreateThread(
    const std::string& name,
    std::unique_ptr<rtc::SocketServer> socket_server) {
  if (!socket_server)
    socket_server = std::make_unique<rtc::NullSocketServer>();
  auto res = std::make_unique<rtc::Thread>(std::move(socket_server));
  res->SetName(name, nullptr);
  res->Start();
  return res;
}

rtc::Thread* RealTimeController::GetMainThread() {
  return main_thread_.get();
}

void RealTimeController::AdvanceTime(TimeDelta duration) {
  main_thread_->ProcessMessages(duration.ms());
}

}  // namespace webrtc
