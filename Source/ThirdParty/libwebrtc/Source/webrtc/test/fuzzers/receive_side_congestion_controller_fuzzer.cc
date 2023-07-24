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

#include "api/array_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  Timestamp arrival_time = Timestamp::Micros(123'456'789);
  SimulatedClock clock(arrival_time);
  ReceiveSideCongestionController cc(
      &clock,
      /*feedback_sender=*/[](auto...) {},
      /*remb_sender=*/[](auto...) {},
      /*network_state_estimator=*/nullptr);
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  extensions.Register<AbsoluteSendTime>(2);
  extensions.Register<TransportSequenceNumber>(3);
  extensions.Register<TransportSequenceNumberV2>(4);
  RtpPacketReceived rtp_packet(&extensions);

  constexpr int kMinPacketSize = sizeof(uint16_t) + sizeof(uint8_t) + 12;
  const uint8_t* const end_data = data + size;
  while (end_data - data >= kMinPacketSize) {
    size_t packet_size = ByteReader<uint16_t>::ReadBigEndian(data) % 1500;
    data += sizeof(uint16_t);
    arrival_time += TimeDelta::Millis(ByteReader<uint8_t>::ReadBigEndian(data));
    data += sizeof(uint8_t);
    packet_size = std::min<size_t>(end_data - data, packet_size);
    auto raw_packet = rtc::MakeArrayView(data, packet_size);
    data += packet_size;

    if (!rtp_packet.Parse(raw_packet)) {
      continue;
    }
    rtp_packet.set_arrival_time(arrival_time);

    cc.OnReceivedPacket(rtp_packet, MediaType::VIDEO);
    clock.AdvanceTimeMilliseconds(5);
    cc.MaybeProcess();
  }
}
}  // namespace webrtc
