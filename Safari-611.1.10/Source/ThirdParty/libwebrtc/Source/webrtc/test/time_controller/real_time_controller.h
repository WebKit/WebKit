/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_REAL_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_REAL_TIME_CONTROLLER_H_

#include <functional>
#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
class RealTimeController : public TimeController {
 public:
  RealTimeController();

  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  std::unique_ptr<rtc::Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<rtc::SocketServer> socket_server) override;
  rtc::Thread* GetMainThread() override;
  void AdvanceTime(TimeDelta duration) override;

 private:
  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  const std::unique_ptr<rtc::Thread> main_thread_;
};

}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_REAL_TIME_CONTROLLER_H_
