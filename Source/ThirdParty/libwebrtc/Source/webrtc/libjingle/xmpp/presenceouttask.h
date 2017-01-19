/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PRESENCEOUTTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_PRESENCEOUTTASK_H_

#include "webrtc/libjingle/xmpp/presencestatus.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

class PresenceOutTask : public XmppTask {
public:
  explicit PresenceOutTask(XmppTaskParentInterface* parent)
      : XmppTask(parent) {}
  virtual ~PresenceOutTask() {}

  XmppReturnStatus Send(const PresenceStatus & s);
  XmppReturnStatus SendDirected(const Jid & j, const PresenceStatus & s);
  XmppReturnStatus SendProbe(const Jid& jid);

  virtual int ProcessStart();
private:
  XmlElement * TranslateStatus(const PresenceStatus & s);
};

}

#endif  // WEBRTC_LIBJINGLE_XMPP_PRESENCEOUTTASK_H_
