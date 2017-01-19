/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/receivetask.h"

namespace buzz {

bool ReceiveTask::HandleStanza(const XmlElement* stanza) {
  if (WantsStanza(stanza)) {
    QueueStanza(stanza);
    return true;
  }

  return false;
}

int ReceiveTask::ProcessStart() {
  const XmlElement* stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;

  ReceiveStanza(stanza);
  return STATE_START;
}

}  // namespace buzz
