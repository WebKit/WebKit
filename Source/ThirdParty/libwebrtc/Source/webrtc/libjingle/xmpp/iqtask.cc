/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/iqtask.h"

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/xmppclient.h"

namespace buzz {

static const int kDefaultIqTimeoutSecs = 15;

IqTask::IqTask(XmppTaskParentInterface* parent,
               const std::string& verb,
               const buzz::Jid& to,
               buzz::XmlElement* el)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      to_(to),
      stanza_(MakeIq(verb, to_, task_id())) {
  stanza_->AddElement(el);
  set_timeout_seconds(kDefaultIqTimeoutSecs);
}

int IqTask::ProcessStart() {
  buzz::XmppReturnStatus ret = SendStanza(stanza_.get());
  // TODO: HandleError(NULL) if SendStanza fails?
  return (ret == buzz::XMPP_RETURN_OK) ? STATE_RESPONSE : STATE_ERROR;
}

bool IqTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, to_, task_id()))
    return false;

  if (stanza->Attr(buzz::QN_TYPE) != buzz::STR_RESULT &&
      stanza->Attr(buzz::QN_TYPE) != buzz::STR_ERROR) {
    return false;
  }

  QueueStanza(stanza);
  return true;
}

int IqTask::ProcessResponse() {
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  bool success = (stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT);
  if (success) {
    HandleResult(stanza);
  } else {
    SignalError(this, stanza->FirstNamed(QN_ERROR));
  }
  return STATE_DONE;
}

int IqTask::OnTimeout() {
  SignalError(this, NULL);
  return XmppTask::OnTimeout();
}

}  // namespace buzz
