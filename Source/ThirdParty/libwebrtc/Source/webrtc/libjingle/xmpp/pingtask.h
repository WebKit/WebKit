/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PINGTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_PINGTASK_H_

#include "webrtc/libjingle/xmpp/xmpptask.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"

namespace buzz {

// Task to periodically send pings to the server to ensure that the network
// connection is valid, implementing XEP-0199.
//
// This is especially useful on cellular networks because:
// 1. It keeps the connections alive through the cellular network's NATs or
//    proxies.
// 2. It detects when the server has crashed or any other case in which the
//    connection has broken without a fin or reset packet being sent to us.
class PingTask : public buzz::XmppTask, private rtc::MessageHandler {
 public:
  PingTask(buzz::XmppTaskParentInterface* parent,
           rtc::MessageQueue* message_queue,
           uint32_t ping_period_millis,
           uint32_t ping_timeout_millis);

  virtual bool HandleStanza(const buzz::XmlElement* stanza);
  virtual int ProcessStart();

  // Raised if there is no response to a ping within ping_timeout_millis.
  // The task is automatically aborted after a timeout.
  sigslot::signal0<> SignalTimeout;

 private:
  // Implementation of MessageHandler.
  virtual void OnMessage(rtc::Message* msg);

  rtc::MessageQueue* message_queue_;
  uint32_t ping_period_millis_;
  uint32_t ping_timeout_millis_;
  int64_t next_ping_time_;
  int64_t ping_response_deadline_;  // 0 if the response has been received
};

} // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_PINGTASK_H_
