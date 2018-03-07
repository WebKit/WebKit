/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_config.h"

#include <sstream>

namespace webrtc {

std::string NackConfig::ToString() const {
  std::stringstream ss;
  ss << "{rtp_history_ms: " << rtp_history_ms;
  ss << '}';
  return ss.str();
}

std::string UlpfecConfig::ToString() const {
  std::stringstream ss;
  ss << "{ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << ", red_rtx_payload_type: " << red_rtx_payload_type;
  ss << '}';
  return ss.str();
}

bool UlpfecConfig::operator==(const UlpfecConfig& other) const {
  return ulpfec_payload_type == other.ulpfec_payload_type &&
         red_payload_type == other.red_payload_type &&
         red_rtx_payload_type == other.red_rtx_payload_type;
}
}  // namespace webrtc
