/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/rtp_parameters.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extension_size.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

namespace {

constexpr int kFlexfecPayloadType = 123;
constexpr uint32_t kMediaSsrc = 1234;
constexpr uint32_t kFlexfecSsrc = 5678;
const char kNoMid[] = "";
const std::vector<RtpExtension> kNoRtpHeaderExtensions;
const std::vector<RtpExtensionSize> kNoRtpHeaderExtensionSizes;

}  // namespace

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  // Create Environment once because creating it for each input noticably
  // reduces the speed of the fuzzer.
  static SimulatedClock* const clock = new SimulatedClock(1);
  static const Environment* const env =
      new Environment(CreateEnvironment(clock));

  if (fuzz_data.size() < 5 || fuzz_data.size() > 200) {
    return;
  }
  // Set time to (1 + data[i++]);
  clock->AdvanceTimeMicroseconds(1 + fuzz_data.Read<uint8_t>() -
                                 clock->TimeInMicroseconds());
  FlexfecSender sender(*env, kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoMid, kNoRtpHeaderExtensions,
                       kNoRtpHeaderExtensionSizes, nullptr /* rtp_state */);
  FecProtectionParams params = {
      .fec_rate = fuzz_data.Read<uint8_t>(),
      .max_fec_frames = static_cast<int>(fuzz_data.Read<uint8_t>() % 100),
      .fec_mask_type =
          fuzz_data.Read<uint8_t>() <= 127 ? kFecMaskRandom : kFecMaskBursty};
  sender.SetProtectionParameters(params, params);
  uint16_t seq_num = fuzz_data.Read<uint8_t>();

  while (fuzz_data.BytesLeft() >= 1) {
    // Everything past the base RTP header (12 bytes) is payload,
    // from the perspective of FlexFEC.
    size_t payload_size = fuzz_data.Read<uint8_t>();
    if (fuzz_data.BytesLeft() <= kRtpHeaderSize + payload_size)
      break;
    RtpPacketToSend rtp_packet(nullptr);
    if (!rtp_packet.Parse(
            fuzz_data.ReadByteArray(kRtpHeaderSize + payload_size)))
      break;
    rtp_packet.SetSequenceNumber(seq_num++);
    rtp_packet.SetSsrc(kMediaSsrc);
    sender.AddPacketAndGenerateFec(rtp_packet);
    sender.GetFecPackets();
  }
}

}  // namespace webrtc
