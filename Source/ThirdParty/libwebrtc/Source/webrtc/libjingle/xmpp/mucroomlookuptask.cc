/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/mucroomlookuptask.h"

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/base/logging.h"


namespace buzz {

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForRoomName(XmppTaskParentInterface* parent,
                                     const Jid& lookup_server_jid,
                                     const std::string& room_name,
                                     const std::string& room_domain) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeNameQuery(room_name, room_domain));
}

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForRoomJid(XmppTaskParentInterface* parent,
                                              const Jid& lookup_server_jid,
                                              const Jid& room_jid) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeJidQuery(room_jid));
}

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForHangoutId(XmppTaskParentInterface* parent,
                                                const Jid& lookup_server_jid,
                                                const std::string& hangout_id) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeHangoutIdQuery(hangout_id));
}

MucRoomLookupTask*
MucRoomLookupTask::CreateLookupTaskForExternalId(
    XmppTaskParentInterface* parent,
    const Jid& lookup_server_jid,
    const std::string& external_id,
    const std::string& type) {
  return new MucRoomLookupTask(parent, lookup_server_jid,
                               MakeExternalIdQuery(external_id, type));
}

MucRoomLookupTask::MucRoomLookupTask(XmppTaskParentInterface* parent,
                                     const Jid& lookup_server_jid,
                                     XmlElement* query)
    : IqTask(parent, STR_SET, lookup_server_jid, query) {
}

XmlElement* MucRoomLookupTask::MakeNameQuery(
    const std::string& room_name, const std::string& room_domain) {
  XmlElement* name_elem = new XmlElement(QN_SEARCH_ROOM_NAME, false);
  name_elem->SetBodyText(room_name);

  XmlElement* domain_elem = new XmlElement(QN_SEARCH_ROOM_DOMAIN, false);
  domain_elem->SetBodyText(room_domain);

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY, true);
  query->AddElement(name_elem);
  query->AddElement(domain_elem);
  return query;
}

XmlElement* MucRoomLookupTask::MakeJidQuery(const Jid& room_jid) {
  XmlElement* jid_elem = new XmlElement(QN_SEARCH_ROOM_JID);
  jid_elem->SetBodyText(room_jid.Str());

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY);
  query->AddElement(jid_elem);
  return query;
}

XmlElement* MucRoomLookupTask::MakeExternalIdQuery(
    const std::string& external_id, const std::string& type) {
  XmlElement* external_id_elem = new XmlElement(QN_SEARCH_EXTERNAL_ID);
  external_id_elem->SetAttr(QN_TYPE, type);
  external_id_elem->SetBodyText(external_id);

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY);
  query->AddElement(external_id_elem);
  return query;
}

// Construct a stanza to lookup the muc jid for a given hangout id. eg:
//
// <query xmlns="jabber:iq:search">
//   <hangout-id>0b48ad092c893a53b7bfc87422caf38e93978798e</hangout-id>
// </query>
XmlElement* MucRoomLookupTask::MakeHangoutIdQuery(
    const std::string& hangout_id) {
  XmlElement* hangout_id_elem = new XmlElement(QN_SEARCH_HANGOUT_ID, false);
  hangout_id_elem->SetBodyText(hangout_id);

  XmlElement* query = new XmlElement(QN_SEARCH_QUERY, true);
  query->AddElement(hangout_id_elem);
  return query;
}

// Handle a response like the following:
//
// <query xmlns="jabber:iq:search">
//   <item jid="muvc-private-chat-guid@groupchat.google.com">
//     <room-name>0b48ad092c893a53b7bfc87422caf38e93978798e</room-name>
//     <room-domain>hangout.google.com</room-domain>
//   </item>
// </query>
void MucRoomLookupTask::HandleResult(const XmlElement* stanza) {
  const XmlElement* query_elem = stanza->FirstNamed(QN_SEARCH_QUERY);
  if (query_elem == NULL) {
    SignalError(this, stanza);
    return;
  }

  const XmlElement* item_elem = query_elem->FirstNamed(QN_SEARCH_ITEM);
  if (item_elem == NULL) {
    SignalError(this, stanza);
    return;
  }

  MucRoomInfo room;
  room.jid = Jid(item_elem->Attr(buzz::QN_JID));
  if (!room.jid.IsValid()) {
    SignalError(this, stanza);
    return;
  }

  const XmlElement* room_name_elem =
      item_elem->FirstNamed(QN_SEARCH_ROOM_NAME);
  if (room_name_elem != NULL) {
    room.name = room_name_elem->BodyText();
  }

  const XmlElement* room_domain_elem =
      item_elem->FirstNamed(QN_SEARCH_ROOM_DOMAIN);
  if (room_domain_elem != NULL) {
    room.domain = room_domain_elem->BodyText();
  }

  const XmlElement* hangout_id_elem =
      item_elem->FirstNamed(QN_SEARCH_HANGOUT_ID);
  if (hangout_id_elem != NULL) {
    room.hangout_id = hangout_id_elem->BodyText();
  }

  SignalResult(this, room);
}

}  // namespace buzz
