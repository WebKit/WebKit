/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/pingtask.h"

#include <memory>

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/base/logging.h"

namespace buzz {

PingTask::PingTask(buzz::XmppTaskParentInterface* parent,
                   rtc::MessageQueue* message_queue,
                   uint32_t ping_period_millis,
                   uint32_t ping_timeout_millis)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      message_queue_(message_queue),
      ping_period_millis_(ping_period_millis),
      ping_timeout_millis_(ping_timeout_millis),
      next_ping_time_(0),
      ping_response_deadline_(0) {
  ASSERT(ping_period_millis >= ping_timeout_millis);
}

bool PingTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, Jid(STR_EMPTY), task_id())) {
    return false;
  }

  if (stanza->Attr(buzz::QN_TYPE) != buzz::STR_RESULT &&
      stanza->Attr(buzz::QN_TYPE) != buzz::STR_ERROR) {
    return false;
  }

  QueueStanza(stanza);
  return true;
}

// This task runs indefinitely and remains in either the start or blocked
// states.
int PingTask::ProcessStart() {
  if (ping_period_millis_ < ping_timeout_millis_) {
    LOG(LS_ERROR) << "ping_period_millis should be >= ping_timeout_millis";
    return STATE_ERROR;
  }
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza != NULL) {
    // Received a ping response of some sort (don't care what it is).
    ping_response_deadline_ = 0;
  }

  int64_t now = rtc::TimeMillis();

  // If the ping timed out, signal.
  if (ping_response_deadline_ != 0 && now >= ping_response_deadline_) {
    SignalTimeout();
    return STATE_ERROR;
  }

  // Send a ping if it's time.
  if (now >= next_ping_time_) {
    std::unique_ptr<buzz::XmlElement> stanza(
        MakeIq(buzz::STR_GET, Jid(STR_EMPTY), task_id()));
    stanza->AddElement(new buzz::XmlElement(QN_PING));
    SendStanza(stanza.get());

    ping_response_deadline_ = now + ping_timeout_millis_;
    next_ping_time_ = now + ping_period_millis_;

    // Wake ourselves up when it's time to send another ping or when the ping
    // times out (so we can fire a signal).
    message_queue_->PostDelayed(RTC_FROM_HERE, ping_timeout_millis_, this);
    message_queue_->PostDelayed(RTC_FROM_HERE, ping_period_millis_, this);
  }

  return STATE_BLOCKED;
}

void PingTask::OnMessage(rtc::Message* msg) {
  // Get the task manager to run this task so we can send a ping or signal or
  // process a ping response.
  Wake();
}

} // namespace buzz
