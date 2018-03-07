/*
 *  Copyright 2005 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/mediamonitor.h"
#include "pc/channelmanager.h"
#include "rtc_base/checks.h"

namespace cricket {

enum {
  MSG_MONITOR_POLL = 1,
  MSG_MONITOR_START = 2,
  MSG_MONITOR_STOP = 3,
  MSG_MONITOR_SIGNAL = 4
};

MediaMonitor::MediaMonitor(rtc::Thread* worker_thread,
                           rtc::Thread* monitor_thread)
    : worker_thread_(worker_thread),
      monitor_thread_(monitor_thread), monitoring_(false), rate_(0) {
}

MediaMonitor::~MediaMonitor() {
  monitoring_ = false;
  monitor_thread_->Clear(this);
  worker_thread_->Clear(this);
}

void MediaMonitor::Start(uint32_t milliseconds) {
  rate_ = milliseconds;
  if (rate_ < 100)
    rate_ = 100;
  worker_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_START);
}

void MediaMonitor::Stop() {
  worker_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_STOP);
  rate_ = 0;
}

void MediaMonitor::OnMessage(rtc::Message* message) {
  rtc::CritScope cs(&crit_);

  switch (message->message_id) {
  case MSG_MONITOR_START:
    RTC_DCHECK(rtc::Thread::Current() == worker_thread_);
    if (!monitoring_) {
      monitoring_ = true;
      PollMediaChannel();
    }
    break;

  case MSG_MONITOR_STOP:
    RTC_DCHECK(rtc::Thread::Current() == worker_thread_);
    if (monitoring_) {
      monitoring_ = false;
      worker_thread_->Clear(this);
    }
    break;

  case MSG_MONITOR_POLL:
    RTC_DCHECK(rtc::Thread::Current() == worker_thread_);
    PollMediaChannel();
    break;

  case MSG_MONITOR_SIGNAL:
    RTC_DCHECK(rtc::Thread::Current() == monitor_thread_);
    Update();
    break;
  }
}

void MediaMonitor::PollMediaChannel() {
  rtc::CritScope cs(&crit_);
  RTC_DCHECK(rtc::Thread::Current() == worker_thread_);

  GetStats();

  // Signal the monitoring thread, start another poll timer
  monitor_thread_->Post(RTC_FROM_HERE, this, MSG_MONITOR_SIGNAL);
  worker_thread_->PostDelayed(RTC_FROM_HERE, rate_, this, MSG_MONITOR_POLL);
}

}  // namespace cricket
