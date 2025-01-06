/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {

constexpr int kFlexfecPayloadType = 123;
constexpr uint32_t kMediaSsrc = 1234;
constexpr uint32_t kFlexfecSsrc = 5678;
const char kNoMid[] = "";
const std::vector<RtpExtension> kNoRtpHeaderExtensions;
const std::vector<RtpExtensionSize> kNoRtpHeaderExtensionSizes;

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  // Create Environment once because creating it for each input noticably
  // reduces the speed of the fuzzer.
  static SimulatedClock* const clock = new SimulatedClock(1);
  static const Environment* const env =
      new Environment(CreateEnvironment(clock));

  size_t i = 0;
  if (size < 5 || size > 200) {
    return;
  }
  // Set time to (1 + data[i++]);
  clock->AdvanceTimeMicroseconds(1 + data[i++] - clock->TimeInMicroseconds());
  FlexfecSender sender(*env, kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoMid, kNoRtpHeaderExtensions,
                       kNoRtpHeaderExtensionSizes, nullptr /* rtp_state */);
  FecProtectionParams params = {
      data[i++], static_cast<int>(data[i++] % 100),
      data[i++] <= 127 ? kFecMaskRandom : kFecMaskBursty};
  sender.SetProtectionParameters(params, params);
  uint16_t seq_num = data[i++];

  while (i + 1 < size) {
    // Everything past the base RTP header (12 bytes) is payload,
    // from the perspective of FlexFEC.
    size_t payload_size = data[i++];
    if (i + kRtpHeaderSize + payload_size >= size)
      break;
    std::unique_ptr<uint8_t[]> packet(
        new uint8_t[kRtpHeaderSize + payload_size]);
    memcpy(packet.get(), &data[i], kRtpHeaderSize + payload_size);
    i += kRtpHeaderSize + payload_size;
    ByteWriter<uint16_t>::WriteBigEndian(&packet[2], seq_num++);
    ByteWriter<uint32_t>::WriteBigEndian(&packet[8], kMediaSsrc);
    RtpPacketToSend rtp_packet(nullptr);
    if (!rtp_packet.Parse(packet.get(), kRtpHeaderSize + payload_size))
      break;
    sender.AddPacketAndGenerateFec(rtp_packet);
    sender.GetFecPackets();
  }
}

}  // namespace webrtc
