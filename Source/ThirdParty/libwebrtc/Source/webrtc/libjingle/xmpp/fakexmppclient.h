/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A fake XmppClient for use in unit tests.

#ifndef WEBRTC_LIBJINGLE_XMPP_FAKEXMPPCLIENT_H_
#define WEBRTC_LIBJINGLE_XMPP_FAKEXMPPCLIENT_H_

#include <algorithm>
#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

class XmlElement;

class FakeXmppClient : public XmppTaskParentInterface,
                       public XmppClientInterface {
 public:
  explicit FakeXmppClient(rtc::TaskParent* parent)
      : XmppTaskParentInterface(parent) {
  }

  // As XmppTaskParentInterface
  virtual XmppClientInterface* GetClient() {
    return this;
  }

  virtual int ProcessStart() {
    return STATE_RESPONSE;
  }

  // As XmppClientInterface
  virtual XmppEngine::State GetState() const {
    return XmppEngine::STATE_OPEN;
  }

  virtual const Jid& jid() const {
    return jid_;
  }

  virtual std::string NextId() {
    // Implement if needed for tests.
    return "0";
  }

  virtual XmppReturnStatus SendStanza(const XmlElement* stanza) {
    sent_stanzas_.push_back(stanza);
    return XMPP_RETURN_OK;
  }

  const std::vector<const XmlElement*>& sent_stanzas() {
    return sent_stanzas_;
  }

  virtual XmppReturnStatus SendStanzaError(
      const XmlElement * pelOriginal,
      XmppStanzaError code,
      const std::string & text) {
    // Implement if needed for tests.
    return XMPP_RETURN_OK;
  }

  virtual void AddXmppTask(XmppTask* task,
                           XmppEngine::HandlerLevel level) {
    tasks_.push_back(task);
  }

  virtual void RemoveXmppTask(XmppTask* task) {
    std::remove(tasks_.begin(), tasks_.end(), task);
  }

  // As FakeXmppClient
  void set_jid(const Jid& jid) {
    jid_ = jid;
  }

  // Takes ownership of stanza.
  void HandleStanza(XmlElement* stanza) {
    for (std::vector<XmppTask*>::iterator task = tasks_.begin();
         task != tasks_.end(); ++task) {
      if ((*task)->HandleStanza(stanza)) {
        delete stanza;
        return;
      }
    }
    delete stanza;
  }

 private:
  Jid jid_;
  std::vector<XmppTask*> tasks_;
  std::vector<const XmlElement*> sent_stanzas_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_FAKEXMPPCLIENT_H_
