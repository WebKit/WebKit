/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPPUMP_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPPUMP_H_

#include "webrtc/libjingle/xmpp/xmppclient.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/taskrunner.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"

namespace buzz {

// Simple xmpp pump

class XmppPumpNotify {
public:
  virtual ~XmppPumpNotify() {}
  virtual void OnStateChange(buzz::XmppEngine::State state) = 0;
};

class XmppPump : public rtc::MessageHandler, public rtc::TaskRunner {
public:
  XmppPump(buzz::XmppPumpNotify * notify = NULL);

  buzz::XmppClient *client() { return client_; }

  void DoLogin(const buzz::XmppClientSettings & xcs,
               buzz::AsyncSocket* socket,
               buzz::PreXmppAuth* auth);
  void DoDisconnect();

  void OnStateChange(buzz::XmppEngine::State state);

  void WakeTasks();

  int64_t CurrentTime();

  void OnMessage(rtc::Message *pmsg);

  buzz::XmppReturnStatus SendStanza(const buzz::XmlElement *stanza);

private:
  buzz::XmppClient *client_;
  buzz::XmppEngine::State state_;
  buzz::XmppPumpNotify *notify_;
};

}  // namespace buzz

#endif // WEBRTC_LIBJINGLE_XMPP_XMPPPUMP_H_

