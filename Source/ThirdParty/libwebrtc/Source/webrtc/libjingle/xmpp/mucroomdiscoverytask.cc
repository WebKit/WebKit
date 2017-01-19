/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/mucroomdiscoverytask.h"

#include "webrtc/libjingle/xmpp/constants.h"

namespace buzz {

MucRoomDiscoveryTask::MucRoomDiscoveryTask(
    XmppTaskParentInterface* parent,
    const Jid& room_jid)
    : IqTask(parent, STR_GET, room_jid,
             new buzz::XmlElement(buzz::QN_DISCO_INFO_QUERY)) {
}

void MucRoomDiscoveryTask::HandleResult(const XmlElement* stanza) {
  const XmlElement* query = stanza->FirstNamed(QN_DISCO_INFO_QUERY);
  if (query == NULL) {
    SignalError(this, NULL);
    return;
  }

  std::set<std::string> features;
  std::map<std::string, std::string> extended_info;
  const XmlElement* identity = query->FirstNamed(QN_DISCO_IDENTITY);
  if (identity == NULL || !identity->HasAttr(QN_NAME)) {
    SignalResult(this, false, "", "", features, extended_info);
    return;
  }

  const std::string name(identity->Attr(QN_NAME));

  // Get the conversation id
  const XmlElement* conversation =
      identity->FirstNamed(QN_GOOGLE_MUC_HANGOUT_CONVERSATION_ID);
  std::string conversation_id;
  if (conversation != NULL) {
    conversation_id = conversation->BodyText();
  }

  for (const XmlElement* feature = query->FirstNamed(QN_DISCO_FEATURE);
       feature != NULL; feature = feature->NextNamed(QN_DISCO_FEATURE)) {
    features.insert(feature->Attr(QN_VAR));
  }

  const XmlElement* data_x = query->FirstNamed(QN_XDATA_X);
  if (data_x != NULL) {
    for (const XmlElement* field = data_x->FirstNamed(QN_XDATA_FIELD);
         field != NULL; field = field->NextNamed(QN_XDATA_FIELD)) {
      const std::string key(field->Attr(QN_VAR));
      extended_info[key] = field->Attr(QN_XDATA_VALUE);
    }
  }

  SignalResult(this, true, name, conversation_id, features, extended_info);
}

}  // namespace buzz
