/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/simulated_thread.h"

#include <algorithm>
#include <utility>

#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {
namespace {

// A socket server that does nothing. It's different from NullSocketServer in
// that it does allow sleep/wakeup. This avoids usage of an Event instance which
// otherwise would cause issues with the simulated Yeild behavior.
class DummySocketServer : public rtc::SocketServer {
 public:
  rtc::Socket* CreateSocket(int family, int type) override {
    RTC_NOTREACHED();
    return nullptr;
  }
  rtc::AsyncSocket* CreateAsyncSocket(int family, int type) override {
    RTC_NOTREACHED();
    return nullptr;
  }
  bool Wait(int cms, bool process_io) override {
    RTC_CHECK_EQ(cms, 0);
    return true;
  }
  void WakeUp() override {}
};

}  // namespace

SimulatedThread::SimulatedThread(
    sim_time_impl::SimulatedTimeControllerImpl* handler,
    absl::string_view name,
    std::unique_ptr<rtc::SocketServer> socket_server)
    : rtc::Thread(socket_server ? std::move(socket_server)
                                : std::make_unique<DummySocketServer>()),
      handler_(handler),
      name_(new char[name.size()]) {
  std::copy_n(name.begin(), name.size(), name_);
}

SimulatedThread::~SimulatedThread() {
  handler_->Unregister(this);
  delete[] name_;
}

void SimulatedThread::RunReady(Timestamp at_time) {
  CurrentThreadSetter set_current(this);
  ProcessMessages(0);
  int delay_ms = GetDelay();
  MutexLock lock(&lock_);
  if (delay_ms == kForever) {
    next_run_time_ = Timestamp::PlusInfinity();
  } else {
    next_run_time_ = at_time + TimeDelta::Millis(delay_ms);
  }
}

void SimulatedThread::Send(const rtc::Location& posted_from,
                           rtc::MessageHandler* phandler,
                           uint32_t id,
                           rtc::MessageData* pdata) {
  if (IsQuitting())
    return;
  rtc::Message msg;
  msg.posted_from = posted_from;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  if (IsCurrent()) {
    msg.phandler->OnMessage(&msg);
  } else {
    TaskQueueBase* yielding_from = TaskQueueBase::Current();
    handler_->StartYield(yielding_from);
    RunReady(Timestamp::MinusInfinity());
    CurrentThreadSetter set_current(this);
    msg.phandler->OnMessage(&msg);
    handler_->StopYield(yielding_from);
  }
}

void SimulatedThread::Post(const rtc::Location& posted_from,
                           rtc::MessageHandler* phandler,
                           uint32_t id,
                           rtc::MessageData* pdata,
                           bool time_sensitive) {
  rtc::Thread::Post(posted_from, phandler, id, pdata, time_sensitive);
  MutexLock lock(&lock_);
  next_run_time_ = Timestamp::MinusInfinity();
}

void SimulatedThread::PostDelayed(const rtc::Location& posted_from,
                                  int delay_ms,
                                  rtc::MessageHandler* phandler,
                                  uint32_t id,
                                  rtc::MessageData* pdata) {
  rtc::Thread::PostDelayed(posted_from, delay_ms, phandler, id, pdata);
  MutexLock lock(&lock_);
  next_run_time_ =
      std::min(next_run_time_, Timestamp::Millis(rtc::TimeMillis() + delay_ms));
}

void SimulatedThread::PostAt(const rtc::Location& posted_from,
                             int64_t target_time_ms,
                             rtc::MessageHandler* phandler,
                             uint32_t id,
                             rtc::MessageData* pdata) {
  rtc::Thread::PostAt(posted_from, target_time_ms, phandler, id, pdata);
  MutexLock lock(&lock_);
  next_run_time_ = std::min(next_run_time_, Timestamp::Millis(target_time_ms));
}

void SimulatedThread::Stop() {
  Thread::Quit();
}

SimulatedMainThread::SimulatedMainThread(
    sim_time_impl::SimulatedTimeControllerImpl* handler)
    : SimulatedThread(handler, "main", nullptr), current_setter_(this) {}

SimulatedMainThread::~SimulatedMainThread() {
  // Removes pending tasks in case they keep shared pointer references to
  // objects whose destructor expects to run before the Thread destructor.
  Stop();
  DoDestroy();
}

}  // namespace webrtc
