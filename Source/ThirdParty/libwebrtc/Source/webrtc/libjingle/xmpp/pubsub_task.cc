/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/pubsub_task.h"

#include <map>
#include <memory>
#include <string>

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/base/common.h"

namespace buzz {

PubsubTask::PubsubTask(XmppTaskParentInterface* parent,
                       const buzz::Jid& pubsub_node_jid)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SENDER),
      pubsub_node_jid_(pubsub_node_jid) {
}

PubsubTask::~PubsubTask() {
}

// Checks for pubsub publish events as well as responses to get IQs.
bool PubsubTask::HandleStanza(const buzz::XmlElement* stanza) {
  const buzz::QName& stanza_name(stanza->Name());
  if (stanza_name == buzz::QN_MESSAGE) {
    if (MatchStanzaFrom(stanza, pubsub_node_jid_)) {
      const buzz::XmlElement* pubsub_event_item =
          stanza->FirstNamed(QN_PUBSUB_EVENT);
      if (pubsub_event_item != NULL) {
        QueueStanza(pubsub_event_item);
        return true;
      }
    }
  } else if (stanza_name == buzz::QN_IQ) {
    if (MatchResponseIq(stanza, pubsub_node_jid_, task_id())) {
      const buzz::XmlElement* pubsub_item = stanza->FirstNamed(QN_PUBSUB);
      if (pubsub_item != NULL) {
        QueueStanza(pubsub_item);
        return true;
      }
    }
  }
  return false;
}

int PubsubTask::ProcessResponse() {
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }

  if (stanza->Attr(buzz::QN_TYPE) == buzz::STR_ERROR) {
    OnPubsubError(stanza->FirstNamed(buzz::QN_ERROR));
    return STATE_RESPONSE;
  }

  const buzz::QName& stanza_name(stanza->Name());
  if (stanza_name == QN_PUBSUB_EVENT) {
    HandlePubsubEventMessage(stanza);
  } else if (stanza_name == QN_PUBSUB) {
    HandlePubsubIqGetResponse(stanza);
  }

  return STATE_RESPONSE;
}

// Registers a function pointer to be called when the value of the pubsub
// node changes.
// Note that this does not actually change the XMPP pubsub
// subscription. All publish events are always received by everyone in the
// MUC. This function just controls whether the handle function will get
// called when the event is received.
bool PubsubTask::SubscribeToNode(const std::string& pubsub_node,
                                 NodeHandler handler) {
  subscribed_nodes_[pubsub_node] = handler;
  std::unique_ptr<buzz::XmlElement> get_iq_request(
      MakeIq(buzz::STR_GET, pubsub_node_jid_, task_id()));
  if (!get_iq_request) {
    return false;
  }
  buzz::XmlElement* pubsub_element = new buzz::XmlElement(QN_PUBSUB, true);
  buzz::XmlElement* items_element = new buzz::XmlElement(QN_PUBSUB_ITEMS, true);

  items_element->AddAttr(buzz::QN_NODE, pubsub_node);
  pubsub_element->AddElement(items_element);
  get_iq_request->AddElement(pubsub_element);

  if (SendStanza(get_iq_request.get()) != buzz::XMPP_RETURN_OK) {
    return false;
  }

  return true;
}

void PubsubTask::UnsubscribeFromNode(const std::string& pubsub_node) {
  subscribed_nodes_.erase(pubsub_node);
}

void PubsubTask::OnPubsubError(const buzz::XmlElement* error_stanza) {
}

// Checks for a pubsub event message like the following:
//
//  <message from="muvc-private-chat-some-id@groupchat.google.com"
//   to="john@site.com/gcomm582B14C9">
//    <event xmlns:"http://jabber.org/protocol/pubsub#event">
//      <items node="node-name">
//        <item id="some-id">
//          <payload/>
//        </item>
//      </items>
//    </event>
//  </message>
//
// It also checks for retraction event messages like the following:
//
//  <message from="muvc-private-chat-some-id@groupchat.google.com"
//   to="john@site.com/gcomm582B14C9">
//    <event xmlns:"http://jabber.org/protocol/pubsub#event">
//      <items node="node-name">
//        <retract id="some-id"/>
//      </items>
//    </event>
//  </message>
void PubsubTask::HandlePubsubEventMessage(
    const buzz::XmlElement* pubsub_event) {
  ASSERT(pubsub_event->Name() == QN_PUBSUB_EVENT);
  for (const buzz::XmlChild* child = pubsub_event->FirstChild();
       child != NULL;
       child = child->NextChild()) {
    const buzz::XmlElement* child_element = child->AsElement();
    const buzz::QName& child_name(child_element->Name());
    if (child_name == QN_PUBSUB_EVENT_ITEMS) {
      HandlePubsubItems(child_element);
    }
  }
}

// Checks for a response to an pubsub IQ get like the following:
//
//  <iq from="muvc-private-chat-some-id@groupchat.google.com"
//   to="john@site.com/gcomm582B14C9"
//   type="result">
//    <pubsub xmlns:"http://jabber.org/protocol/pubsub">
//      <items node="node-name">
//        <item id="some-id">
//          <payload/>
//        </item>
//      </items>
//    </event>
//  </message>
void PubsubTask::HandlePubsubIqGetResponse(
    const buzz::XmlElement* pubsub_iq_response) {
  ASSERT(pubsub_iq_response->Name() == QN_PUBSUB);
  for (const buzz::XmlChild* child = pubsub_iq_response->FirstChild();
       child != NULL;
       child = child->NextChild()) {
    const buzz::XmlElement* child_element = child->AsElement();
    const buzz::QName& child_name(child_element->Name());
    if (child_name == QN_PUBSUB_ITEMS) {
      HandlePubsubItems(child_element);
    }
  }
}

// Calls registered handlers in response to pubsub event or response to
// IQ pubsub get.
// 'items' is the child of a pubsub#event:event node or pubsub:pubsub node.
void PubsubTask::HandlePubsubItems(const buzz::XmlElement* items) {
  ASSERT(items->HasAttr(QN_NODE));
  const std::string& node_name(items->Attr(QN_NODE));
  NodeSubscriptions::iterator iter = subscribed_nodes_.find(node_name);
  if (iter != subscribed_nodes_.end()) {
    NodeHandler handler = iter->second;
    const buzz::XmlElement* item = items->FirstElement();
    while (item != NULL) {
      const buzz::QName& item_name(item->Name());
      if (item_name != QN_PUBSUB_EVENT_ITEM &&
          item_name != QN_PUBSUB_EVENT_RETRACT &&
          item_name != QN_PUBSUB_ITEM) {
        continue;
      }

      (this->*handler)(item);
      item = item->NextElement();
    }
    return;
  }
}

}
