/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/transport_adapter.h"

#include <cstdint>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace internal {

TransportAdapter::TransportAdapter(Transport* transport)
    : transport_(transport), enabled_(false) {
  RTC_DCHECK(nullptr != transport);
}

TransportAdapter::~TransportAdapter() = default;

bool TransportAdapter::SendRtp(ArrayView<const uint8_t> packet,
                               const PacketOptions& options) {
  if (!enabled_.load())
    return false;

  return transport_->SendRtp(packet, options);
}

bool TransportAdapter::SendRtcp(ArrayView<const uint8_t> packet,
                                const PacketOptions& options) {
  if (!enabled_.load())
    return false;

  return transport_->SendRtcp(packet, options);
}

void TransportAdapter::Enable() {
  enabled_.store(true);
}

void TransportAdapter::Disable() {
  enabled_.store(false);
}

}  // namespace internal
}  // namespace webrtc
