/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_MUCROOMCONFIGTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_MUCROOMCONFIGTASK_H_

#include <string>
#include "webrtc/libjingle/xmpp/iqtask.h"

namespace buzz {

// This task configures the muc room for document sharing and other enterprise
// specific goodies.
class MucRoomConfigTask : public IqTask {
 public:
  MucRoomConfigTask(XmppTaskParentInterface* parent,
                    const Jid& room_jid,
                    const std::string& room_name,
                    const std::vector<std::string>& room_features);

  // Room configuration does not return any reasonable error
  // values. The First config request configures the room, subseqent
  // ones are just ignored by server and server returns empty
  // response.
  sigslot::signal1<MucRoomConfigTask*> SignalResult;

  const Jid& room_jid() const { return room_jid_; }

 protected:
  virtual void HandleResult(const XmlElement* stanza);

 private:
  static XmlElement* MakeRequest(const std::string& room_name,
                                 const std::vector<std::string>& room_features);
  Jid room_jid_;
};

}  // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_MUCROOMCONFIGTASK_H_
