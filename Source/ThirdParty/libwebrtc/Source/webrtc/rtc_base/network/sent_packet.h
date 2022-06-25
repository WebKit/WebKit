/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_SENT_PACKET_H_
#define RTC_BASE_NETWORK_SENT_PACKET_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

enum class PacketType {
  kUnknown,
  kData,
  kIceConnectivityCheck,
  kIceConnectivityCheckResponse,
  kStunMessage,
  kTurnMessage,
};

enum class PacketInfoProtocolType {
  kUnknown,
  kUdp,
  kTcp,
  kSsltcp,
  kTls,
};

struct RTC_EXPORT PacketInfo {
  PacketInfo();
  PacketInfo(const PacketInfo& info);
  ~PacketInfo();

  bool included_in_feedback = false;
  bool included_in_allocation = false;
  PacketType packet_type = PacketType::kUnknown;
  PacketInfoProtocolType protocol = PacketInfoProtocolType::kUnknown;
  // A unique id assigned by the network manager, and absl::nullopt if not set.
  absl::optional<uint16_t> network_id;
  size_t packet_size_bytes = 0;
  size_t turn_overhead_bytes = 0;
  size_t ip_overhead_bytes = 0;
};

struct RTC_EXPORT SentPacket {
  SentPacket();
  SentPacket(int64_t packet_id, int64_t send_time_ms);
  SentPacket(int64_t packet_id,
             int64_t send_time_ms,
             const rtc::PacketInfo& info);

  int64_t packet_id = -1;
  int64_t send_time_ms = -1;
  rtc::PacketInfo info;
};

}  // namespace rtc

#endif  // RTC_BASE_NETWORK_SENT_PACKET_H_
