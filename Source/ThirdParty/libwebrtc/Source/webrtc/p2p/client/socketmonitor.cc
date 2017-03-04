/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/client/socketmonitor.h"

#include "webrtc/base/checks.h"

namespace cricket {

enum {
  MSG_MONITOR_POLL,
  MSG_MONITOR_START,
  MSG_MONITOR_STOP,
  MSG_MONITOR_SIGNAL
};

ConnectionMonitor::ConnectionMonitor(ConnectionStatsGetter* stats_getter,
                                     rtc::Thread* network_thread,
                                     rtc::Thread* monitoring_thread) {
  stats_getter_ = stats_getter;
  network_thread_ = network_thread;
  monitoring_thread_ = monitoring_thread;
  monitoring_ = false;
}

ConnectionMonitor::~ConnectionMonitor() {
  network_thread_->Clear(this);
  monitoring_thread_->Clear(this);
}

void ConnectionMonitor::Start(int milliseconds) {
  rate_ = milliseconds;
  if (rate_ < 250)
    rate_ = 250;
  network_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_START);
}

void ConnectionMonitor::Stop() {
  network_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_STOP);
}

void ConnectionMonitor::OnMessage(rtc::Message *message) {
  rtc::CritScope cs(&crit_);
  switch (message->message_id) {
    case MSG_MONITOR_START:
      RTC_DCHECK(rtc::Thread::Current() == network_thread_);
      if (!monitoring_) {
        monitoring_ = true;
        PollConnectionStats_w();
      }
      break;

    case MSG_MONITOR_STOP:
      RTC_DCHECK(rtc::Thread::Current() == network_thread_);
      if (monitoring_) {
        monitoring_ = false;
        network_thread_->Clear(this);
      }
      break;

    case MSG_MONITOR_POLL:
      RTC_DCHECK(rtc::Thread::Current() == network_thread_);
      PollConnectionStats_w();
      break;

    case MSG_MONITOR_SIGNAL: {
      RTC_DCHECK(rtc::Thread::Current() == monitoring_thread_);
      std::vector<ConnectionInfo> infos = connection_infos_;
      crit_.Leave();
      SignalUpdate(this, infos);
      crit_.Enter();
      break;
    }
  }
}

void ConnectionMonitor::PollConnectionStats_w() {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  rtc::CritScope cs(&crit_);

  // Gather connection infos
  stats_getter_->GetConnectionStats(&connection_infos_);

  // Signal the monitoring thread, start another poll timer
  monitoring_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_SIGNAL);
  network_thread_->PostDelayed(RTC_FROM_HERE, rate_, this, MSG_MONITOR_POLL);
}

}  // namespace cricket
