/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/data_channel_interface.h"

#include <cstdint>
#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "rtc_base/checks.h"

namespace webrtc {

bool DataChannelInterface::ordered() const {
  return false;
}

uint16_t DataChannelInterface::maxRetransmitTime() const {
  return 0;
}

uint16_t DataChannelInterface::maxRetransmits() const {
  return 0;
}

std::optional<int> DataChannelInterface::maxRetransmitsOpt() const {
  return std::nullopt;
}

std::optional<int> DataChannelInterface::maxPacketLifeTime() const {
  return std::nullopt;
}

std::string DataChannelInterface::protocol() const {
  return std::string();
}

bool DataChannelInterface::negotiated() const {
  return false;
}

PriorityValue DataChannelInterface::priority() const {
  return PriorityValue(Priority::kLow);
}

uint64_t DataChannelInterface::MaxSendQueueSize() {
  return 16 * 1024 * 1024;  // 16 MiB
}

// TODO(tommi): Remove method once downstream implementations have been removed.
bool DataChannelInterface::Send(const DataBuffer& /* buffer */) {
  RTC_DCHECK_NOTREACHED();
  return false;
}

// TODO(tommi): Remove implementation once method is pure virtual.
void DataChannelInterface::SendAsync(
    DataBuffer /* buffer */,
    absl::AnyInvocable<void(RTCError) &&> /* on_complete */) {
  RTC_DCHECK_NOTREACHED();
}

}  // namespace webrtc
