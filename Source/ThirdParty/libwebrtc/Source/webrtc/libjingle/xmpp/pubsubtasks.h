/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_PUBSUBTASKS_H_
#define WEBRTC_LIBJINGLE_XMPP_PUBSUBTASKS_H_

#include <vector>

#include "webrtc/libjingle/xmpp/iqtask.h"
#include "webrtc/libjingle/xmpp/receivetask.h"
#include "webrtc/base/sigslot.h"

namespace buzz {

// A PubSub itemid + payload.  Useful for signaling items.
struct PubSubItem {
  std::string itemid;
  // The entire <item>, owned by the stanza handler.  To keep a
  // reference after handling, make a copy.
  const XmlElement* elem;
};

// An IqTask which gets a <pubsub><items> for a particular jid and
// node, parses the items in the response and signals the items.
class PubSubRequestTask : public IqTask {
 public:
  PubSubRequestTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node);

  sigslot::signal2<PubSubRequestTask*,
                   const std::vector<PubSubItem>&> SignalResult;
  // SignalError inherited by IqTask.
 private:
  virtual void HandleResult(const XmlElement* stanza);
};

// A ReceiveTask which listens for <event><items> of a particular
// pubsub JID and node and then signals them items.
class PubSubReceiveTask : public ReceiveTask {
 public:
  PubSubReceiveTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node)
      : ReceiveTask(parent),
        pubsubjid_(pubsubjid),
        node_(node) {
  }

  virtual int ProcessStart();
  sigslot::signal2<PubSubReceiveTask*,
                   const std::vector<PubSubItem>&> SignalUpdate;

 protected:
  virtual bool WantsStanza(const XmlElement* stanza);
  virtual void ReceiveStanza(const XmlElement* stanza);

 private:
  Jid pubsubjid_;
  std::string node_;
};

// An IqTask which publishes a <pubsub><publish><item> to a particular
// pubsub jid and node.
class PubSubPublishTask : public IqTask {
 public:
  // Takes ownership of children
  PubSubPublishTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node,
                    const std::string& itemid,
                    const std::vector<XmlElement*>& children);

  const std::string& itemid() const { return itemid_; }

  sigslot::signal1<PubSubPublishTask*> SignalResult;

 private:
  // SignalError inherited by IqTask.
  virtual void HandleResult(const XmlElement* stanza);

  std::string itemid_;
};

// An IqTask which publishes a <pubsub><publish><retract> to a particular
// pubsub jid and node.
class PubSubRetractTask : public IqTask {
 public:
  PubSubRetractTask(XmppTaskParentInterface* parent,
                    const Jid& pubsubjid,
                    const std::string& node,
                    const std::string& itemid);

  const std::string& itemid() const { return itemid_; }

  sigslot::signal1<PubSubRetractTask*> SignalResult;

 private:
  // SignalError inherited by IqTask.
  virtual void HandleResult(const XmlElement* stanza);

  std::string itemid_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_PUBSUBTASKS_H_
