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

#include <memory>

#include "api/transport/stun.h"
#include "rtc_base/byte_buffer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  // Normally we'd check the integrity first, but those checks are
  // fuzzed separately in stun_validator_fuzzer.cc. We still want to
  // fuzz this target since the integrity checks could be forged by a
  // malicious adversary who receives a call.
  std::unique_ptr<webrtc::IceMessage> stun_msg(new webrtc::IceMessage());
  webrtc::ByteBufferReader buf(fuzz_data.ReadRemaining());
  stun_msg->Read(&buf);
  stun_msg->ValidateMessageIntegrity("");
}
}  // namespace webrtc
