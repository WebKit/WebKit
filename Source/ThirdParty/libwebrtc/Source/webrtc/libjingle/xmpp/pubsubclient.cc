/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/pubsubclient.h"

#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/pubsubtasks.h"

namespace buzz {

void PubSubClient::RequestItems() {
  PubSubRequestTask* request_task =
      new PubSubRequestTask(parent_, pubsubjid_, node_);
  request_task->SignalResult.connect(this, &PubSubClient::OnRequestResult);
  request_task->SignalError.connect(this, &PubSubClient::OnRequestError);

  PubSubReceiveTask* receive_task =
      new PubSubReceiveTask(parent_, pubsubjid_, node_);
  receive_task->SignalUpdate.connect(this, &PubSubClient::OnReceiveUpdate);

  receive_task->Start();
  request_task->Start();
}

void PubSubClient::PublishItem(
    const std::string& itemid, XmlElement* payload, std::string* task_id_out) {
  std::vector<XmlElement*> children;
  children.push_back(payload);
  PublishItem(itemid, children, task_id_out);
}

void PubSubClient::PublishItem(
    const std::string& itemid, const std::vector<XmlElement*>& children,
    std::string* task_id_out) {
  PubSubPublishTask* publish_task =
      new PubSubPublishTask(parent_, pubsubjid_, node_, itemid, children);
  publish_task->SignalError.connect(this, &PubSubClient::OnPublishError);
  publish_task->SignalResult.connect(this, &PubSubClient::OnPublishResult);
  publish_task->Start();
  if (task_id_out) {
    *task_id_out = publish_task->task_id();
  }
}

void PubSubClient::RetractItem(
    const std::string& itemid, std::string* task_id_out) {
  PubSubRetractTask* retract_task =
      new PubSubRetractTask(parent_, pubsubjid_, node_, itemid);
  retract_task->SignalError.connect(this, &PubSubClient::OnRetractError);
  retract_task->SignalResult.connect(this, &PubSubClient::OnRetractResult);
  retract_task->Start();
  if (task_id_out) {
    *task_id_out = retract_task->task_id();
  }
}

void PubSubClient::OnRequestResult(PubSubRequestTask* task,
                                   const std::vector<PubSubItem>& items) {
  SignalItems(this, items);
}

void PubSubClient::OnRequestError(IqTask* task,
                                  const XmlElement* stanza) {
  SignalRequestError(this, stanza);
}

void PubSubClient::OnReceiveUpdate(PubSubReceiveTask* task,
                                   const std::vector<PubSubItem>& items) {
  SignalItems(this, items);
}

const XmlElement* GetItemFromStanza(const XmlElement* stanza) {
  if (stanza != NULL) {
    const XmlElement* pubsub = stanza->FirstNamed(QN_PUBSUB);
    if (pubsub != NULL) {
      const XmlElement* publish = pubsub->FirstNamed(QN_PUBSUB_PUBLISH);
      if (publish != NULL) {
        return publish->FirstNamed(QN_PUBSUB_ITEM);
      }
    }
  }
  return NULL;
}

void PubSubClient::OnPublishResult(PubSubPublishTask* task) {
  const XmlElement* item = GetItemFromStanza(task->stanza());
  SignalPublishResult(this, task->task_id(), item);
}

void PubSubClient::OnPublishError(IqTask* task,
                                  const XmlElement* error_stanza) {
  PubSubPublishTask* publish_task =
      static_cast<PubSubPublishTask*>(task);
  const XmlElement* item = GetItemFromStanza(publish_task->stanza());
  SignalPublishError(this, publish_task->task_id(), item, error_stanza);
}

void PubSubClient::OnRetractResult(PubSubRetractTask* task) {
  SignalRetractResult(this, task->task_id());
}

void PubSubClient::OnRetractError(IqTask* task,
                                  const XmlElement* stanza) {
  PubSubRetractTask* retract_task =
      static_cast<PubSubRetractTask*>(task);
  SignalRetractError(this, retract_task->task_id(), stanza);
}


const std::string PubSubClient::GetPublisherNickFromPubSubItem(
    const XmlElement* item_elem) {
  if (item_elem == NULL) {
    return "";
  }

  return Jid(item_elem->Attr(QN_ATTR_PUBLISHER)).resource();
}
}  // namespace buzz
