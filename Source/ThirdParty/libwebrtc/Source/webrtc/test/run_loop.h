/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RUN_LOOP_H_
#define TEST_RUN_LOOP_H_

#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {
namespace test {

// RunLoop is a helper class for tests that need to process tasks posted
// to a task queue, but still want to run everything on a single thread.
// This is useful for tests that need to simulate asynchronous operations
// without the complexity of managing real threads.
class RunLoop {
 public:
  RunLoop();
  ~RunLoop();

  // Returns a pointer to the task queue implementation managed by this RunLoop.
  TaskQueueBase* task_queue();

  // Runs tasks posted to the task queue via PostTask, until Quit() is called.
  void Run();

  // Stops a call to Run() or RunFor() once all tasks scheduled to run before or
  // at the current time are completed.
  void Quit();

  // Returns a closure that will call Quit() if it is still valid when called.
  absl::AnyInvocable<void()> QuitClosure();

  // Runs tasks posted to the task queue via PostTask, until Quit() is called or
  // `max_wait_duration` has passed. May only be called once at a time.
  void RunFor(TimeDelta max_wait_duration);

  // Processes all pending tasks and returns. This can be useful to
  // synchronously wait for a posted task to execute.
  void Flush();

  // Post a task for execution on the task queue.
  void PostTask(absl::AnyInvocable<void() &&> task) {
    task_queue()->PostTask(std::move(task));
  }

 private:
  class FakeSocketServer : public SocketServer {
   public:
    FakeSocketServer();
    ~FakeSocketServer() override;

    void FailNextWait();

   private:
    bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override;
    void WakeUp() override;

    Socket* CreateSocket(int family, int type) override;

   private:
    bool fail_next_wait_ = false;
  };

  class WorkerThread : public Thread {
   public:
    explicit WorkerThread(SocketServer* ss);

   private:
    CurrentTaskQueueSetter tq_setter_;
  };

  FakeSocketServer socket_server_;
  WorkerThread worker_thread_{&socket_server_};
  WeakPtrFactory<RunLoop> weak_factory_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RUN_LOOP_H_
