/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/run_loop.h"

#include "absl/functional/any_invocable.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace test {

RunLoop::RunLoop() : weak_factory_(this) {
  worker_thread_.WrapCurrent();
}

RunLoop::~RunLoop() {
  worker_thread_.UnwrapCurrent();
}

TaskQueueBase* RunLoop::task_queue() {
  return &worker_thread_;
}

void RunLoop::Run() {
  worker_thread_.ProcessMessages(WorkerThread::kForever);
}

void RunLoop::Quit() {
  socket_server_.FailNextWait();
}

absl::AnyInvocable<void()> RunLoop::QuitClosure() {
  return [loop = weak_factory_.GetWeakPtr()] {
    if (loop) {
      loop->Quit();
    }
  };
}

void RunLoop::RunFor(TimeDelta max_wait_duration) {
  // If Quit is called before the timeout expires, then we'll cancel this post
  // task automatically.
  ScopedTaskSafety auto_cancel;
  worker_thread_.PostDelayedHighPrecisionTask(
      SafeTask(auto_cancel.flag(), QuitClosure()), max_wait_duration);
  Run();
}

void RunLoop::Flush() {
  worker_thread_.PostTask([this]() { socket_server_.FailNextWait(); });
  // If a test clock is used, like with GlobalSimulatedTimeController then the
  // thread will loop forever since time never increases. Since the clock is
  // simulated, 0ms can be used as the loop delay, which will process all
  // messages ready for execution.
  int cms = GetClockForTesting() ? 0 : 1000;
  worker_thread_.ProcessMessages(cms);
}

RunLoop::FakeSocketServer::FakeSocketServer() = default;
RunLoop::FakeSocketServer::~FakeSocketServer() = default;

void RunLoop::FakeSocketServer::FailNextWait() {
  fail_next_wait_ = true;
}

bool RunLoop::FakeSocketServer::Wait(TimeDelta max_wait_duration,
                                     bool process_io) {
  if (fail_next_wait_) {
    fail_next_wait_ = false;
    return false;
  }
  return true;
}

void RunLoop::FakeSocketServer::WakeUp() {}

Socket* RunLoop::FakeSocketServer::CreateSocket(int family, int type) {
  return nullptr;
}

RunLoop::WorkerThread::WorkerThread(SocketServer* ss)
    : Thread(ss), tq_setter_(this) {}

}  // namespace test
}  // namespace webrtc
