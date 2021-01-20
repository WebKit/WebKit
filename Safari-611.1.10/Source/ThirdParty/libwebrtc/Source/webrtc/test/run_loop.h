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

#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {

// This utility class allows you to run a TaskQueue supported interface on the
// main test thread, call Run() while doing things asynchonously and break
// the loop (from the same thread) from a callback by calling Quit().
class RunLoop {
 public:
  RunLoop();
  ~RunLoop();

  TaskQueueBase* task_queue();

  void Run();
  void Quit();

  void Flush();

  // Convenience methods since TaskQueueBase doesn't support this sort of magic.
  template <typename Closure>
  void PostTask(Closure&& task) {
    task_queue()->PostTask(ToQueuedTask(std::forward<Closure>(task)));
  }

  template <typename Closure>
  void PostDelayedTask(Closure&& task, uint32_t milliseconds) {
    task_queue()->PostDelayedTask(ToQueuedTask(std::forward<Closure>(task)),
                                  milliseconds);
  }

 private:
  class FakeSocketServer : public rtc::SocketServer {
   public:
    FakeSocketServer();
    ~FakeSocketServer();

    void FailNextWait();

   private:
    bool Wait(int cms, bool process_io) override;
    void WakeUp() override;

    rtc::Socket* CreateSocket(int family, int type) override;
    rtc::AsyncSocket* CreateAsyncSocket(int family, int type) override;

   private:
    bool fail_next_wait_ = false;
  };

  class WorkerThread : public rtc::Thread {
   public:
    explicit WorkerThread(rtc::SocketServer* ss);

   private:
    CurrentTaskQueueSetter tq_setter_;
  };

  FakeSocketServer socket_server_;
  WorkerThread worker_thread_{&socket_server_};
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RUN_LOOP_H_
