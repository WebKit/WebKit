/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/tools/neteq_input.h"

#include <sstream>

namespace webrtc {
namespace test {

std::string NetEqInput::PacketData::ToString() const {
  std::stringstream ss;
  ss << "{"
     << "time_ms: " << static_cast<int64_t>(time_ms) << ", "
     << "header: {"
     << "pt: " << static_cast<int>(header.payloadType) << ", "
     << "sn: " << header.sequenceNumber << ", "
     << "ts: " << header.timestamp << ", "
     << "ssrc: " << header.ssrc << "}, "
     << "payload bytes: " << payload.size() << "}";
  return ss.str();
}

}  // namespace test
}  // namespace webrtc
