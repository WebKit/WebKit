/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PUBSUB_TASK_H_
#define WEBRTC_LIBJINGLE_XMPP_PUBSUB_TASK_H_

#include <map>
#include <string>
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

// Base class to help write pubsub tasks.
// In ProcessStart call SubscribeNode with namespaces of interest along with
// NodeHandlers.
// When pubsub notifications arrive and matches the namespace, the NodeHandlers
// will be called back.
class PubsubTask : public buzz::XmppTask {
 public:
  virtual ~PubsubTask();

 protected:
  typedef void (PubsubTask::*NodeHandler)(const buzz::XmlElement* node);

  PubsubTask(XmppTaskParentInterface* parent, const buzz::Jid& pubsub_node_jid);

  virtual bool HandleStanza(const buzz::XmlElement* stanza);
  virtual int ProcessResponse();

  bool SubscribeToNode(const std::string& pubsub_node, NodeHandler handler);
  void UnsubscribeFromNode(const std::string& pubsub_node);

  // Called when there is an error. Derived class can do what it needs to.
  virtual void OnPubsubError(const buzz::XmlElement* error_stanza);

 private:
  typedef std::map<std::string, NodeHandler> NodeSubscriptions;

  void HandlePubsubIqGetResponse(const buzz::XmlElement* pubsub_iq_response);
  void HandlePubsubEventMessage(const buzz::XmlElement* pubsub_event_message);
  void HandlePubsubItems(const buzz::XmlElement* items);

  buzz::Jid pubsub_node_jid_;
  NodeSubscriptions subscribed_nodes_;
};

}  // namespace buzz

#endif // WEBRTC_LIBJINGLE_XMPP_PUBSUB_TASK_H_
