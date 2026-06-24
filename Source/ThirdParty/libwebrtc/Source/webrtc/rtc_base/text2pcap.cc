/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/text2pcap.h"

#include <cstdint>
#include <span>
#include <string>

#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

std::string Text2Pcap::DumpPacket(bool outbound,
                                  std::span<const uint8_t> payload,
                                  int64_t timestamp_ms) {
  webrtc::StringBuilder s;
  s << "\n" << (outbound ? "O " : "I ");

  int64_t remaining = timestamp_ms % (24 * 60 * 60 * kNumMillisecsPerSec);
  int hours = remaining / (24 * 60 * 60 * kNumMillisecsPerSec);
  remaining = remaining % (60 * 60 * kNumMillisecsPerSec);
  int minutes = remaining / (60 * 60 * kNumMillisecsPerSec);
  remaining = remaining % (60 * kNumMillisecsPerSec);
  int seconds = remaining / kNumMillisecsPerSec;
  int ms = remaining % kNumMillisecsPerSec;
  s.AppendFormat("%02d:%02d:%02d.%03d", hours, minutes, seconds, ms);
  s << " 0000";
  for (uint8_t byte : payload) {
    s.AppendFormat(" %02x", byte);
  }
  return s.str();
}

}  // namespace webrtc
