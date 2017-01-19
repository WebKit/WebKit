/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef THIRD_PARTY_LIBJINGLE_FILES_WEBRTC_LIBJINGLE_XMPP_PRESENCERECEIVETASK_H_
#define THIRD_PARTY_LIBJINGLE_FILES_WEBRTC_LIBJINGLE_XMPP_PRESENCERECEIVETASK_H_

#include "webrtc/base/sigslot.h"

#include "webrtc/libjingle/xmpp/presencestatus.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"

namespace buzz {

// A task to receive presence status callbacks from the XMPP server.
class PresenceReceiveTask : public XmppTask {
 public:
  // Arguments:
  //   parent a reference to task interface associated withe the XMPP client.
  explicit PresenceReceiveTask(XmppTaskParentInterface* parent);

  // Shuts down the thread associated with this task.
  virtual ~PresenceReceiveTask();

  // Starts pulling queued status messages and dispatching them to the
  // PresenceUpdate() callback.
  virtual int ProcessStart();

  // Slot for presence message callbacks
  sigslot::signal1<const PresenceStatus&> PresenceUpdate;

 protected:
  // Called by the XMPP engine when presence stanzas are received from the
  // server.
  virtual bool HandleStanza(const XmlElement * stanza);

 private:
  // Handles presence stanzas by converting the data to PresenceStatus
  // objects and passing those along to the SignalStatusUpadate() callback.
  void HandlePresence(const Jid& from, const XmlElement * stanza);

  // Extracts presence information for the presence stanza sent form the
  // server.
  static void DecodeStatus(const Jid& from, const XmlElement * stanza,
                           PresenceStatus* status);
};

} // namespace buzz

#endif // THIRD_PARTY_LIBJINGLE_FILES_WEBRTC_LIBJINGLE_XMPP_PRESENCERECEIVETASK_H_
