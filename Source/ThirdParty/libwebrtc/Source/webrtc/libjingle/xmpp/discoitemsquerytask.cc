/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/discoitemsquerytask.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

DiscoItemsQueryTask::DiscoItemsQueryTask(XmppTaskParentInterface* parent,
                                         const Jid& to,
                                         const std::string& node)
    : IqTask(parent, STR_GET, to, MakeRequest(node)) {
}

XmlElement* DiscoItemsQueryTask::MakeRequest(const std::string& node) {
  XmlElement* element = new XmlElement(QN_DISCO_ITEMS_QUERY, true);
  if (!node.empty()) {
    element->AddAttr(QN_NODE, node);
  }
  return element;
}

void DiscoItemsQueryTask::HandleResult(const XmlElement* stanza) {
  const XmlElement* query = stanza->FirstNamed(QN_DISCO_ITEMS_QUERY);
  if (query) {
    std::vector<DiscoItem> items;
    for (const buzz::XmlChild* child = query->FirstChild(); child;
         child = child->NextChild()) {
      DiscoItem item;
      const buzz::XmlElement* child_element = child->AsElement();
      if (ParseItem(child_element, &item)) {
        items.push_back(item);
      }
    }
    SignalResult(items);
  } else {
    SignalError(this, NULL);
  }
}

bool DiscoItemsQueryTask::ParseItem(const XmlElement* element,
                                    DiscoItem* item) {
  if (element->HasAttr(QN_JID)) {
    return false;
  }

  item->jid = element->Attr(QN_JID);
  item->name = element->Attr(QN_NAME);
  item->node = element->Attr(QN_NODE);
  return true;
}

}  // namespace buzz
