/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/xmpp/presencestatus.h"

namespace buzz {
PresenceStatus::PresenceStatus()
  : pri_(0),
    show_(SHOW_NONE),
    available_(false),
    e_code_(0),
    feedback_probation_(false),
    know_capabilities_(false),
    voice_capability_(false),
    pmuc_capability_(false),
    video_capability_(false),
    camera_capability_(false) {
}

void PresenceStatus::UpdateWith(const PresenceStatus& new_value) {
  if (!new_value.know_capabilities()) {
    bool k = know_capabilities();
    bool p = voice_capability();
    std::string node = caps_node();
    std::string v = version();

    *this = new_value;

    set_know_capabilities(k);
    set_caps_node(node);
    set_voice_capability(p);
     set_version(v);
  } else {
    *this = new_value;
  }
}

} // namespace buzz
