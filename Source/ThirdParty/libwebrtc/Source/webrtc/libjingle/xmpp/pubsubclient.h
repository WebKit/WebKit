/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PUBSUBCLIENT_H_
#define WEBRTC_LIBJINGLE_XMPP_PUBSUBCLIENT_H_

#include <string>
#include <vector>

#include "webrtc/libjingle/xmpp/jid.h"
#include "webrtc/libjingle/xmpp/pubsubtasks.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/task.h"

// Easy to use clients built on top of the tasks for XEP-0060
// (http://xmpp.org/extensions/xep-0060.html).

namespace buzz {

class Jid;
class XmlElement;
class XmppTaskParentInterface;

// An easy-to-use pubsub client that handles the three tasks of
// getting, publishing, and listening for updates.  Tied to a specific
// pubsub jid and node.  All you have to do is RequestItems, listen
// for SignalItems and PublishItems.
class PubSubClient : public sigslot::has_slots<> {
 public:
  PubSubClient(XmppTaskParentInterface* parent,
               const Jid& pubsubjid,
               const std::string& node)
    : parent_(parent),
      pubsubjid_(pubsubjid),
      node_(node) {}

  const std::string& node() const { return node_; }

  // Requests the <pubsub><items>, which will be returned via
  // SignalItems, or SignalRequestError if there is a failure.  Should
  // auto-subscribe.
  void RequestItems();
  // Fired when either <pubsub><items> are returned or when
  // <event><items> are received.
  sigslot::signal2<PubSubClient*,
                   const std::vector<PubSubItem>&> SignalItems;
  // Signal (this, error stanza)
  sigslot::signal2<PubSubClient*,
                   const XmlElement*> SignalRequestError;
  // Signal (this, task_id, item, error stanza)
  sigslot::signal4<PubSubClient*,
                   const std::string&,
                   const XmlElement*,
                   const XmlElement*> SignalPublishError;
  // Signal (this, task_id, item)
  sigslot::signal3<PubSubClient*,
                   const std::string&,
                   const XmlElement*> SignalPublishResult;
  // Signal (this, task_id, error stanza)
  sigslot::signal3<PubSubClient*,
                   const std::string&,
                   const XmlElement*> SignalRetractError;
  // Signal (this, task_id)
  sigslot::signal2<PubSubClient*,
                   const std::string&> SignalRetractResult;

  // Publish an item.  Takes ownership of payload.
  void PublishItem(const std::string& itemid,
                   XmlElement* payload,
                   std::string* task_id_out);
  // Publish an item.  Takes ownership of children.
  void PublishItem(const std::string& itemid,
                   const std::vector<XmlElement*>& children,
                   std::string* task_id_out);
  // Retract (delete) an item.
  void RetractItem(const std::string& itemid,
                   std::string* task_id_out);

  // Get the publisher nick if it exists from the pubsub item.
  const std::string GetPublisherNickFromPubSubItem(const XmlElement* item_elem);

 private:
  void OnRequestError(IqTask* task,
                      const XmlElement* stanza);
  void OnRequestResult(PubSubRequestTask* task,
                       const std::vector<PubSubItem>& items);
  void OnReceiveUpdate(PubSubReceiveTask* task,
                       const std::vector<PubSubItem>& items);
  void OnPublishResult(PubSubPublishTask* task);
  void OnPublishError(IqTask* task,
                      const XmlElement* stanza);
  void OnRetractResult(PubSubRetractTask* task);
  void OnRetractError(IqTask* task,
                      const XmlElement* stanza);

  XmppTaskParentInterface* parent_;
  Jid pubsubjid_;
  std::string node_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_PUBSUBCLIENT_H_
