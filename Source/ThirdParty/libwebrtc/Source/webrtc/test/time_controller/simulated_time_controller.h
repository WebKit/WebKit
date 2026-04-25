/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_

#include <memory>
#include <string>

#include "api/task_queue/task_queue_factory.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/simulated_time_controller_impl.h"

namespace webrtc {

// TimeController implementation using completely simulated time. Task queues
// and process threads created by this controller will run delayed activities
// when AdvanceTime() is called. Overrides the global clock backing
// webrtc::TimeMillis() and webrtc::TimeMicros(). Note that this is not thread
// safe since it modifies global state.
class GlobalSimulatedTimeController : public TimeController {
 public:
  explicit GlobalSimulatedTimeController(Timestamp start_time);
  ~GlobalSimulatedTimeController() override;

  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<SocketServer> socket_server) override;
  Thread* GetMainThread() override;

  void AdvanceTime(TimeDelta duration) override;

  // Advances time by `duration`and do not run delayed tasks in the meantime.
  // Useful for simulating contention on destination queues.
  void SkipForwardBy(TimeDelta duration);

 private:
  ScopedBaseFakeClock global_clock_;
  // Provides simulated CurrentNtpInMilliseconds()
  SimulatedClock sim_clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
  ScopedYieldPolicy yield_policy_;
  std::unique_ptr<Thread> main_thread_;
};
}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
