/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FUZZERS_UTILS_VALIDATE_RTP_PACKETIZER_H_
#define TEST_FUZZERS_UTILS_VALIDATE_RTP_PACKETIZER_H_

#include "modules/rtp_rtcp/source/rtp_format.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

// Generates valid `RtpPacketizer::PayloadSizeLimits` from the `fuzz_input`.
RtpPacketizer::PayloadSizeLimits ReadPayloadSizeLimits(
    test::FuzzDataHelper& fuzz_input);

// RTC_CHECKs if `rtp_packetizer` created packets with respect to the `limits`.
void ValidateRtpPacketizer(const RtpPacketizer::PayloadSizeLimits& limits,
                           RtpPacketizer& rtp_packetizer);

}  // namespace webrtc

#endif  // TEST_FUZZERS_UTILS_VALIDATE_RTP_PACKETIZER_H_
