/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/xmppthread.h"

#include "webrtc/libjingle/xmpp/xmppauth.h"
#include "webrtc/libjingle/xmpp/xmppclientsettings.h"

namespace buzz {
namespace {

const uint32_t MSG_LOGIN = 1;
const uint32_t MSG_DISCONNECT = 2;

struct LoginData: public rtc::MessageData {
  LoginData(const buzz::XmppClientSettings& s) : xcs(s) {}
  virtual ~LoginData() {}

  buzz::XmppClientSettings xcs;
};

} // namespace

XmppThread::XmppThread() {
  pump_ = new buzz::XmppPump(this);
}

XmppThread::~XmppThread() {
  Stop();
  delete pump_;
}

void XmppThread::ProcessMessages(int cms) {
  rtc::Thread::ProcessMessages(cms);
}

void XmppThread::Login(const buzz::XmppClientSettings& xcs) {
  Post(RTC_FROM_HERE, this, MSG_LOGIN, new LoginData(xcs));
}

void XmppThread::Disconnect() {
  Post(RTC_FROM_HERE, this, MSG_DISCONNECT);
}

void XmppThread::OnStateChange(buzz::XmppEngine::State state) {
}

void XmppThread::OnMessage(rtc::Message* pmsg) {
  if (pmsg->message_id == MSG_LOGIN) {
    ASSERT(pmsg->pdata != NULL);
    LoginData* data = reinterpret_cast<LoginData*>(pmsg->pdata);
    pump_->DoLogin(data->xcs, new XmppSocket(buzz::TLS_DISABLED),
        new XmppAuth());
    delete data;
  } else if (pmsg->message_id == MSG_DISCONNECT) {
    pump_->DoDisconnect();
  } else {
    ASSERT(false);
  }
}

}  // namespace buzz
