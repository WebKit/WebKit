/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/presencereceivetask.h"

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/base/stringencode.h"

namespace buzz {

static bool IsUtf8FirstByte(int c) {
  return (((c)&0x80)==0) || // is single byte
    ((unsigned char)((c)-0xc0)<0x3e); // or is lead byte
}

PresenceReceiveTask::PresenceReceiveTask(XmppTaskParentInterface* parent)
 : XmppTask(parent, XmppEngine::HL_TYPE) {
}

PresenceReceiveTask::~PresenceReceiveTask() {
  Stop();
}

int PresenceReceiveTask::ProcessStart() {
  const XmlElement * stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }

  Jid from(stanza->Attr(QN_FROM));
  HandlePresence(from, stanza);

  return STATE_START;
}

bool PresenceReceiveTask::HandleStanza(const XmlElement * stanza) {
  // Verify that this is a presence stanze
  if (stanza->Name() != QN_PRESENCE) {
    return false; // not sure if this ever happens.
  }

  // Queue it up
  QueueStanza(stanza);

  return true;
}

void PresenceReceiveTask::HandlePresence(const Jid& from,
                                         const XmlElement* stanza) {
  if (stanza->Attr(QN_TYPE) == STR_ERROR) {
    return;
  }

  PresenceStatus status;
  DecodeStatus(from, stanza, &status);
  PresenceUpdate(status);
}

void PresenceReceiveTask::DecodeStatus(const Jid& from,
                                       const XmlElement* stanza,
                                       PresenceStatus* presence_status) {
  presence_status->set_jid(from);
  if (stanza->Attr(QN_TYPE) == STR_UNAVAILABLE) {
    presence_status->set_available(false);
  } else {
    presence_status->set_available(true);
    const XmlElement * status_elem = stanza->FirstNamed(QN_STATUS);
    if (status_elem != NULL) {
      presence_status->set_status(status_elem->BodyText());

      // Truncate status messages longer than 300 bytes
      if (presence_status->status().length() > 300) {
        size_t len = 300;

        // Be careful not to split legal utf-8 chars in half
        while (!IsUtf8FirstByte(presence_status->status()[len]) && len > 0) {
          len -= 1;
        }
        std::string truncated(presence_status->status(), 0, len);
        presence_status->set_status(truncated);
      }
    }

    const XmlElement * priority = stanza->FirstNamed(QN_PRIORITY);
    if (priority != NULL) {
      int pri;
      if (rtc::FromString(priority->BodyText(), &pri)) {
        presence_status->set_priority(pri);
      }
    }

    const XmlElement * show = stanza->FirstNamed(QN_SHOW);
    if (show == NULL || show->FirstChild() == NULL) {
      presence_status->set_show(PresenceStatus::SHOW_ONLINE);
    } else if (show->BodyText() == "away") {
      presence_status->set_show(PresenceStatus::SHOW_AWAY);
    } else if (show->BodyText() == "xa") {
      presence_status->set_show(PresenceStatus::SHOW_XA);
    } else if (show->BodyText() == "dnd") {
      presence_status->set_show(PresenceStatus::SHOW_DND);
    } else if (show->BodyText() == "chat") {
      presence_status->set_show(PresenceStatus::SHOW_CHAT);
    } else {
      presence_status->set_show(PresenceStatus::SHOW_ONLINE);
    }

    const XmlElement * caps = stanza->FirstNamed(QN_CAPS_C);
    if (caps != NULL) {
      std::string node = caps->Attr(QN_NODE);
      std::string ver = caps->Attr(QN_VER);
      std::string exts = caps->Attr(QN_EXT);

      presence_status->set_know_capabilities(true);
      presence_status->set_caps_node(node);
      presence_status->set_version(ver);
    }

    const XmlElement* delay = stanza->FirstNamed(kQnDelayX);
    if (delay != NULL) {
      // Ideally we would parse this according to the Psuedo ISO-8601 rules
      // that are laid out in JEP-0082:
      // http://www.jabber.org/jeps/jep-0082.html
      std::string stamp = delay->Attr(kQnStamp);
      presence_status->set_sent_time(stamp);
    }

    const XmlElement* nick = stanza->FirstNamed(QN_NICKNAME);
    if (nick) {
      presence_status->set_nick(nick->BodyText());
    }
  }
}

} // namespace buzz
