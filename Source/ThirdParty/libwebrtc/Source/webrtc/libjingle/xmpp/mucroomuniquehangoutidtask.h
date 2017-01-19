/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_XMPP_MUCROOMUNIQUEHANGOUTIDTASK_H_
#define WEBRTC_LIBJINGLE_XMPP_MUCROOMUNIQUEHANGOUTIDTASK_H_

#include "webrtc/libjingle/xmpp/iqtask.h"

namespace buzz {

// Task to request a unique hangout id to be used when starting a hangout.
// The protocol is described in https://docs.google.com/a/google.com/
// document/d/1EFLT6rCYPDVdqQXSQliXwqB3iUkpZJ9B_MNFeOZgN7g/edit
class MucRoomUniqueHangoutIdTask : public buzz::IqTask {
 public:
  MucRoomUniqueHangoutIdTask(buzz::XmppTaskParentInterface* parent,
                        const Jid& lookup_server_jid);
  // signal(task, hangout_id)
  sigslot::signal2<MucRoomUniqueHangoutIdTask*, const std::string&> SignalResult;

 protected:
  virtual void HandleResult(const buzz::XmlElement* stanza);

 private:
  static buzz::XmlElement* MakeUniqueRequestXml();

};

} // namespace buzz

#endif  // WEBRTC_LIBJINGLE_XMPP_MUCROOMUNIQUEHANGOUTIDTASK_H_
