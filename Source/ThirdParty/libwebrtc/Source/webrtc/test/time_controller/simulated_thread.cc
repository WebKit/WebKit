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

namespace webrtc {
namespace {

// A socket server that does nothing. It's different from NullSocketServer in
// that it does allow sleep/wakeup. This avoids usage of an Event instance which
// otherwise would cause issues with the simulated Yeild behavior.
class DummySocketServer : public rtc::SocketServer {
 public:
  rtc::Socket* CreateSocket(int family, int type) override {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }
  bool Wait(TimeDelta max_wait_duration, bool process_io) override {
    RTC_CHECK(max_wait_duration.IsZero());
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

void SimulatedThread::BlockingCallImpl(rtc::FunctionView<void()> functor,
                                       const Location& /*location*/) {
  if (IsQuitting())
    return;

  if (IsCurrent()) {
    functor();
  } else {
    TaskQueueBase* yielding_from = TaskQueueBase::Current();
    handler_->StartYield(yielding_from);
    RunReady(Timestamp::MinusInfinity());
    CurrentThreadSetter set_current(this);
    functor();
    handler_->StopYield(yielding_from);
  }
}

void SimulatedThread::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                                   const PostTaskTraits& traits,
                                   const Location& location) {
  rtc::Thread::PostTaskImpl(std::move(task), traits, location);
  MutexLock lock(&lock_);
  next_run_time_ = Timestamp::MinusInfinity();
}

void SimulatedThread::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                          TimeDelta delay,
                                          const PostDelayedTaskTraits& traits,
                                          const Location& location) {
  rtc::Thread::PostDelayedTaskImpl(std::move(task), delay, traits, location);
  MutexLock lock(&lock_);
  next_run_time_ =
      std::min(next_run_time_, Timestamp::Millis(rtc::TimeMillis()) + delay);
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
