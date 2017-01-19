/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/xmpppump.h"

#include "webrtc/libjingle/xmpp/xmppauth.h"

namespace buzz {

XmppPump::XmppPump(XmppPumpNotify * notify) {
  state_ = buzz::XmppEngine::STATE_NONE;
  notify_ = notify;
  client_ = new buzz::XmppClient(this);  // NOTE: deleted by TaskRunner
}

void XmppPump::DoLogin(const buzz::XmppClientSettings & xcs,
                       buzz::AsyncSocket* socket,
                       buzz::PreXmppAuth* auth) {
  OnStateChange(buzz::XmppEngine::STATE_START);
  if (!AllChildrenDone()) {
    client_->SignalStateChange.connect(this, &XmppPump::OnStateChange);
    client_->Connect(xcs, "", socket, auth);
    client_->Start();
  }
}

void XmppPump::DoDisconnect() {
  if (!AllChildrenDone())
    client_->Disconnect();
  OnStateChange(buzz::XmppEngine::STATE_CLOSED);
}

void XmppPump::OnStateChange(buzz::XmppEngine::State state) {
  if (state_ == state)
    return;
  state_ = state;
  if (notify_ != NULL)
    notify_->OnStateChange(state);
}

void XmppPump::WakeTasks() {
  rtc::Thread::Current()->Post(RTC_FROM_HERE, this);
}

int64_t XmppPump::CurrentTime() {
  return (int64_t)rtc::TimeMillis();
}

void XmppPump::OnMessage(rtc::Message *pmsg) {
  RunTasks();
}

buzz::XmppReturnStatus XmppPump::SendStanza(const buzz::XmlElement *stanza) {
  if (!AllChildrenDone())
    return client_->SendStanza(stanza);
  return buzz::XMPP_RETURN_BADSTATE;
}

}  // namespace buzz

