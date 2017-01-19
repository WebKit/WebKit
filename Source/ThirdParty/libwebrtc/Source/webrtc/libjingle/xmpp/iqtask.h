/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_IQTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_IQTASK_H_

#include <memory>
#include <string>

#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

class IqTask : public XmppTask {
 public:
  IqTask(XmppTaskParentInterface* parent,
         const std::string& verb, const Jid& to,
         XmlElement* el);
  virtual ~IqTask() {}

  const XmlElement* stanza() const { return stanza_.get(); }

  sigslot::signal2<IqTask*,
                   const XmlElement*> SignalError;

 protected:
  virtual void HandleResult(const XmlElement* element) = 0;

 private:
  virtual int ProcessStart();
  virtual bool HandleStanza(const XmlElement* stanza);
  virtual int ProcessResponse();
  virtual int OnTimeout();

  Jid to_;
  std::unique_ptr<XmlElement> stanza_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_IQTASK_H_
