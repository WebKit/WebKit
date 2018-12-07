/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include "absl/memory/memory.h"
#include "p2p/base/mdns_message.h"
#include "rtc_base/message_buffer_reader.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  MessageBufferReader buf(reinterpret_cast<const char*>(data), size);
  auto mdns_msg = absl::make_unique<MdnsMessage>();
  mdns_msg->Read(&buf);
}

}  // namespace webrtc
