/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_RECEIVETASK_H_
#define WEBRTC_LIBJINGLE_XMPP_RECEIVETASK_H_

#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

// A base class for receiving stanzas.  Override WantsStanza to
// indicate that a stanza should be received and ReceiveStanza to
// process it.  Once started, ReceiveStanza will be called for all
// stanzas that return true when passed to WantsStanza. This saves
// you from having to remember how to setup the queueing and the task
// states, etc.
class ReceiveTask : public XmppTask {
 public:
  explicit ReceiveTask(XmppTaskParentInterface* parent) :
      XmppTask(parent, XmppEngine::HL_TYPE) {}
  virtual int ProcessStart();

 protected:
  virtual bool HandleStanza(const XmlElement* stanza);

  // Return true if the stanza should be received.
  virtual bool WantsStanza(const XmlElement* stanza) = 0;
  // Process the received stanza.
  virtual void ReceiveStanza(const XmlElement* stanza) = 0;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_RECEIVETASK_H_
