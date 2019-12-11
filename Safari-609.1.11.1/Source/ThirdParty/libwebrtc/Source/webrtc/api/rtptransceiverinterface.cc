/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtptransceiverinterface.h"

#include "rtc_base/checks.h"

namespace webrtc {

RtpTransceiverInit::RtpTransceiverInit() = default;

RtpTransceiverInit::RtpTransceiverInit(const RtpTransceiverInit& rhs) = default;

RtpTransceiverInit::~RtpTransceiverInit() = default;

absl::optional<RtpTransceiverDirection>
RtpTransceiverInterface::fired_direction() const {
  return absl::nullopt;
}

void RtpTransceiverInterface::SetCodecPreferences(
    rtc::ArrayView<RtpCodecCapability>) {
  RTC_NOTREACHED() << "Not implemented";
}

}  // namespace webrtc
