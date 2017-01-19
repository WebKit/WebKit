/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/mucroomuniquehangoutidtask.h"

#include "webrtc/libjingle/xmpp/constants.h"

namespace buzz {

MucRoomUniqueHangoutIdTask::MucRoomUniqueHangoutIdTask(XmppTaskParentInterface* parent,
                                             const Jid& lookup_server_jid)
    : IqTask(parent, STR_GET, lookup_server_jid, MakeUniqueRequestXml()) {
}

// Construct a stanza to request a unique room id. eg:
//
// <unique hangout-id="true" xmlns="http://jabber.org/protocol/muc#unique"/>
XmlElement* MucRoomUniqueHangoutIdTask::MakeUniqueRequestXml() {
  XmlElement* xml = new XmlElement(QN_MUC_UNIQUE_QUERY, false);
  xml->SetAttr(QN_HANGOUT_ID, STR_TRUE);
  return xml;
}

// Handle a response like the following:
//
// <unique hangout-id="hangout_id"
//    xmlns="http://jabber.org/protocol/muc#unique"/>
//  muvc-private-chat-guid@groupchat.google.com
// </unique>
void MucRoomUniqueHangoutIdTask::HandleResult(const XmlElement* stanza) {

  const XmlElement* unique_elem = stanza->FirstNamed(QN_MUC_UNIQUE_QUERY);
  if (unique_elem == NULL ||
      !unique_elem->HasAttr(QN_HANGOUT_ID)) {
    SignalError(this, stanza);
    return;
  }

  std::string hangout_id = unique_elem->Attr(QN_HANGOUT_ID);

  SignalResult(this, hangout_id);
}

} // namespace buzz
