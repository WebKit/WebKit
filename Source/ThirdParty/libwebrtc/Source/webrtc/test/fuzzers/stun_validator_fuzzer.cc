/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include "webrtc/p2p/base/stun.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  const char* message = reinterpret_cast<const char*>(data);

  cricket::StunMessage::ValidateFingerprint(message, size);
  cricket::StunMessage::ValidateMessageIntegrity(message, size, "");
}
}  // namespace webrtc
