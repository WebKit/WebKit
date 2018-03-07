/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"

#include <stdlib.h>

namespace webrtc {

RTPReceiverStrategy::RTPReceiverStrategy(RtpData* data_callback)
    : data_callback_(data_callback) {}

void RTPReceiverStrategy::GetLastMediaSpecificPayload(
    PayloadUnion* payload) const {
  rtc::CritScope cs(&crit_sect_);
  if (last_payload_) {
    *payload = *last_payload_;
  }
}

void RTPReceiverStrategy::SetLastMediaSpecificPayload(
    const PayloadUnion& payload) {
  rtc::CritScope cs(&crit_sect_);
  last_payload_.emplace(payload);
}

void RTPReceiverStrategy::CheckPayloadChanged(int8_t payload_type,
                                              PayloadUnion* specific_payload,
                                              bool* should_discard_changes) {
  // Default: Keep changes.
  *should_discard_changes = false;
}

int RTPReceiverStrategy::Energy(uint8_t array_of_energy[kRtpCsrcSize]) const {
  return -1;
}

}  // namespace webrtc
