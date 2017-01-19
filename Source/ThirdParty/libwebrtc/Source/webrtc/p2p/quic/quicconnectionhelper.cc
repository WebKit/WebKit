/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quicconnectionhelper.h"

namespace cricket {

QuicAlarm* QuicConnectionHelper::CreateAlarm(
    net::QuicAlarm::Delegate* delegate) {
  return new QuicAlarm(GetClock(), thread_,
                       net::QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate));
}

net::QuicArenaScopedPtr<net::QuicAlarm> QuicConnectionHelper::CreateAlarm(
    net::QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
    net::QuicConnectionArena* arena) {
  return net::QuicArenaScopedPtr<QuicAlarm>(
      new QuicAlarm(GetClock(), thread_, std::move(delegate)));
}

QuicAlarm::QuicAlarm(const net::QuicClock* clock,
                     rtc::Thread* thread,
                     net::QuicArenaScopedPtr<net::QuicAlarm::Delegate> delegate)
    : net::QuicAlarm(std::move(delegate)), clock_(clock), thread_(thread) {}

QuicAlarm::~QuicAlarm() {}

void QuicAlarm::OnMessage(rtc::Message* msg) {
  // The alarm may have been cancelled.
  if (!deadline().IsInitialized()) {
    return;
  }

  // The alarm may have been re-set to a later time.
  if (clock_->Now() < deadline()) {
    SetImpl();
    return;
  }

  Fire();
}

int64_t QuicAlarm::GetDelay() const {
  return deadline().Subtract(clock_->Now()).ToMilliseconds();
}

void QuicAlarm::SetImpl() {
  DCHECK(deadline().IsInitialized());
  CancelImpl();  // Unregister if already posted.

  int64_t delay_ms = GetDelay();
  if (delay_ms < 0) {
    delay_ms = 0;
  }
  thread_->PostDelayed(RTC_FROM_HERE, delay_ms, this);
}

void QuicAlarm::CancelImpl() {
  thread_->Clear(this);
}

QuicConnectionHelper::QuicConnectionHelper(rtc::Thread* thread)
    : thread_(thread) {}

QuicConnectionHelper::~QuicConnectionHelper() {}

const net::QuicClock* QuicConnectionHelper::GetClock() const {
  return &clock_;
}

net::QuicRandom* QuicConnectionHelper::GetRandomGenerator() {
  return net::QuicRandom::GetInstance();
}

net::QuicBufferAllocator* QuicConnectionHelper::GetBufferAllocator() {
  return &buffer_allocator_;
}

}  // namespace cricket
