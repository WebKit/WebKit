/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_XMPPTHREAD_H_
#define WEBRTC_LIBJINGLE_XMPP_XMPPTHREAD_H_

#include "webrtc/libjingle/xmpp/xmppclientsettings.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpppump.h"
#include "webrtc/libjingle/xmpp/xmppsocket.h"
#include "webrtc/base/thread.h"

namespace buzz {

class XmppThread:
    public rtc::Thread, buzz::XmppPumpNotify, rtc::MessageHandler {
public:
  XmppThread();
  ~XmppThread();

  buzz::XmppClient* client() { return pump_->client(); }

  void ProcessMessages(int cms);

  void Login(const buzz::XmppClientSettings & xcs);
  void Disconnect();

private:
  buzz::XmppPump* pump_;

  void OnStateChange(buzz::XmppEngine::State state);
  void OnMessage(rtc::Message* pmsg);
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_XMPPTHREAD_H_

