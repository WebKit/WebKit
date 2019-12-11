/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_TIME_CONTROLLER_H_

#include <functional>
#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Interface for controlling time progress. This allows us to execute test code
// in either real time or simulated time by using different implementation of
// this interface.
class TimeController {
 public:
  virtual ~TimeController() = default;
  // Provides a clock instance that follows implementation defined time
  // progress.
  virtual Clock* GetClock() = 0;
  // The returned factory will created task queues that runs in implementation
  // defined time domain.
  virtual TaskQueueFactory* GetTaskQueueFactory() = 0;
  // Creates a process thread.
  virtual std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) = 0;
  // Allow task queues and process threads created by this instance to execute
  // for the given |duration|.
  virtual void Sleep(TimeDelta duration) = 0;
  // Execute closure in an implementation defined scope where rtc::Event::Wait
  // might yield to execute other tasks. This allows doing blocking waits on
  // tasks on other task queues froma a task queue without deadlocking.
  virtual void InvokeWithControlledYield(std::function<void()> closure) = 0;
};
}  // namespace webrtc
#endif  // TEST_TIME_CONTROLLER_TIME_CONTROLLER_H_
