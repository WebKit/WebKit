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

#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace test {

RunLoop::RunLoop() {
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

void RunLoop::Flush() {
  worker_thread_.PostTask(
      ToQueuedTask([this]() { socket_server_.FailNextWait(); }));
  worker_thread_.ProcessMessages(1000);
}

RunLoop::FakeSocketServer::FakeSocketServer() = default;
RunLoop::FakeSocketServer::~FakeSocketServer() = default;

void RunLoop::FakeSocketServer::FailNextWait() {
  fail_next_wait_ = true;
}

bool RunLoop::FakeSocketServer::Wait(int cms, bool process_io) {
  if (fail_next_wait_) {
    fail_next_wait_ = false;
    return false;
  }
  return true;
}

void RunLoop::FakeSocketServer::WakeUp() {}

rtc::Socket* RunLoop::FakeSocketServer::CreateSocket(int family, int type) {
  return nullptr;
}

rtc::AsyncSocket* RunLoop::FakeSocketServer::CreateAsyncSocket(int family,
                                                               int type) {
  return nullptr;
}

RunLoop::WorkerThread::WorkerThread(rtc::SocketServer* ss)
    : rtc::Thread(ss), tq_setter_(this) {}

}  // namespace test
}  // namespace webrtc
