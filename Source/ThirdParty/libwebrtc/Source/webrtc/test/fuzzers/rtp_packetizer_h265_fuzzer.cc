/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstddef>

#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packetizer_h265.h"
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/fuzzers/utils/validate_rtp_packetizer.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  RtpPacketizer::PayloadSizeLimits limits = ReadPayloadSizeLimits(fuzz_data);

  // Main function under test: RtpPacketizerH265's constructor.
  RtpPacketizerH265 packetizer(fuzz_data.ReadRemaining(), limits);

  ValidateRtpPacketizer(limits, packetizer);
}

}  // namespace webrtc
