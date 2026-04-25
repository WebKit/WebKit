/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtp_packet_simulator.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/environment/environment.h"
#include "api/rtp_headers.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"

namespace webrtc::video_timing_simulator {

RtpPacketSimulator::RtpPacketSimulator(const Environment& env)
    : env_(env),
      rtp_header_extension_map_(
          ParsedRtcEventLog::GetDefaultHeaderExtensionMap()) {}

RtpPacketSimulator::SimulatedPacket
RtpPacketSimulator::SimulateRtpPacketReceived(
    const LoggedRtpPacket& logged_packet) const {
  RtpPacketReceived rtp_packet(&rtp_header_extension_map_);
  rtp_packet.set_arrival_time(env_.clock().CurrentTime());

  // RTP header.
  const RTPHeader& header = logged_packet.header;
  rtp_packet.SetMarker(header.markerBit);
  rtp_packet.SetPayloadType(header.payloadType);
  rtp_packet.SetSequenceNumber(header.sequenceNumber);
  rtp_packet.SetTimestamp(header.timestamp);
  rtp_packet.SetSsrc(header.ssrc);

  // RTP header extensions.
  const RTPHeaderExtension& extension = header.extension;
  if (extension.hasTransportSequenceNumber) {
    rtp_packet.SetExtension<TransportSequenceNumber>(
        extension.transportSequenceNumber);
  }
  if (extension.hasTransmissionTimeOffset) {
    rtp_packet.SetExtension<TransmissionOffset>(
        extension.transmissionTimeOffset);
  }
  if (extension.hasAbsoluteSendTime) {
    rtp_packet.SetExtension<AbsoluteSendTime>(extension.absoluteSendTime);
  }
  rtp_packet.SetRawExtension<RtpDependencyDescriptorExtension>(
      logged_packet.dependency_descriptor_wire_format);

  // Payload and padding.
  size_t payload_size = logged_packet.total_length -
                        logged_packet.header_length - header.paddingLength;
  std::vector<uint8_t> payload(payload_size, 0u);  // Zero initialize.
  bool has_rtx_osn = logged_packet.rtx_original_sequence_number.has_value();
  if (has_rtx_osn) {
    if (payload.size() < kRtpHeaderSize) {
      RTC_LOG(LS_WARNING) << "Packet was logged with RTX OSN, but payload size "
                             "could not fit it";
    } else {
      // Storing the RTX OSN in-band is required for downstream handling of the
      // packets.
      uint16_t rtx_osn = *logged_packet.rtx_original_sequence_number;
      ByteWriter<uint16_t>::WriteBigEndian(payload.data(), rtx_osn);
    }
  }
  rtp_packet.SetPayload(payload);
  rtp_packet.SetPadding(header.paddingLength);

  return {.rtp_packet = rtp_packet, .has_rtx_osn = has_rtx_osn};
}

}  // namespace webrtc::video_timing_simulator
