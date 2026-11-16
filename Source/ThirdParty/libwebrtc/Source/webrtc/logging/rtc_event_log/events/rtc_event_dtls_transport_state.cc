/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"

#include <memory>

#include "absl/memory/memory.h"
#include "api/dtls_transport_interface.h"

namespace webrtc {

RtcEventDtlsTransportState::RtcEventDtlsTransportState(DtlsTransportState state)
    : dtls_transport_state_(state) {}

RtcEventDtlsTransportState::~RtcEventDtlsTransportState() = default;

std::unique_ptr<RtcEventDtlsTransportState> RtcEventDtlsTransportState::Copy()
    const {
  return absl::WrapUnique(new RtcEventDtlsTransportState(*this));
}

}  // namespace webrtc
