/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "api/environment/environment_factory.h"
#include "api/media_types.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "system_wrappers/include/clock.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  Timestamp arrival_time = Timestamp::Micros(123'456'789);
  SimulatedClock clock(arrival_time);
  ReceiveSideCongestionController cc(
      CreateEnvironment(&clock),
      /*feedback_sender=*/[](auto...) {},
      /*remb_sender=*/[](auto...) {});
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  extensions.Register<AbsoluteSendTime>(2);
  extensions.Register<TransportSequenceNumber>(3);
  extensions.Register<TransportSequenceNumberV2>(4);
  RtpPacketReceived rtp_packet(&extensions);

  constexpr int kMinPacketSize = sizeof(uint16_t) + sizeof(uint8_t) + 12;
  while (fuzz_data.BytesLeft() >= kMinPacketSize) {
    size_t packet_size = fuzz_data.Read<uint16_t>() % 1500;
    arrival_time += TimeDelta::Millis(fuzz_data.Read<uint8_t>());
    packet_size = std::min<size_t>(fuzz_data.BytesLeft(), packet_size);

    if (!rtp_packet.Parse(fuzz_data.ReadByteArray(packet_size))) {
      continue;
    }
    rtp_packet.set_arrival_time(arrival_time);

    cc.OnReceivedPacket(rtp_packet, MediaType::VIDEO);
    clock.AdvanceTimeMilliseconds(5);
    cc.MaybeProcess();
  }
}
}  // namespace webrtc
