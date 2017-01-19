/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_MUCROOMDISCOVERYTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_MUCROOMDISCOVERYTASK_H_

#include <map>
#include <string>
#include "webrtc/libjingle/xmpp/iqtask.h"

namespace buzz {

// This task requests the feature capabilities of the room. It is based on
// XEP-0030, and extended using XEP-0004.
class MucRoomDiscoveryTask : public IqTask {
 public:
  MucRoomDiscoveryTask(XmppTaskParentInterface* parent,
                       const Jid& room_jid);

  // Signal (exists, name, conversationId, features, extended_info)
  sigslot::signal6<MucRoomDiscoveryTask*,
                   bool,
                   const std::string&,
                   const std::string&,
                   const std::set<std::string>&,
                   const std::map<std::string, std::string>& > SignalResult;

 protected:
  virtual void HandleResult(const XmlElement* stanza);
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_MUCROOMDISCOVERYTASK_H_
