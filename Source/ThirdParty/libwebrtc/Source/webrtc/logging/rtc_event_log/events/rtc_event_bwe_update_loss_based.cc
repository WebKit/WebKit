/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"

namespace webrtc {

RtcEventBweUpdateLossBased::RtcEventBweUpdateLossBased(int32_t bitrate_bps,
                                                       uint8_t fraction_loss,
                                                       int32_t total_packets)
    : bitrate_bps_(bitrate_bps),
      fraction_loss_(fraction_loss),
      total_packets_(total_packets) {}

RtcEventBweUpdateLossBased::~RtcEventBweUpdateLossBased() = default;

RtcEvent::Type RtcEventBweUpdateLossBased::GetType() const {
  return RtcEvent::Type::BweUpdateLossBased;
}

bool RtcEventBweUpdateLossBased::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
