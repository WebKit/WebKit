/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/transport_adapter.h"

#include "webrtc/base/checks.h"

namespace webrtc {
namespace internal {

TransportAdapter::TransportAdapter(Transport* transport)
    : transport_(transport), enabled_(0) {
  RTC_DCHECK(nullptr != transport);
}

bool TransportAdapter::SendRtp(const uint8_t* packet,
                               size_t length,
                               const PacketOptions& options) {
  if (enabled_.Value() == 0)
    return false;

  return transport_->SendRtp(packet, length, options);
}

bool TransportAdapter::SendRtcp(const uint8_t* packet, size_t length) {
  if (enabled_.Value() == 0)
    return false;

  return transport_->SendRtcp(packet, length);
}

void TransportAdapter::Enable() {
  // If this exchange fails it means enabled_ was already true, no need to
  // check result and iterate.
  enabled_.CompareExchange(1, 0);
}

void TransportAdapter::Disable() { enabled_.CompareExchange(0, 1); }

}  // namespace internal
}  // namespace webrtc
