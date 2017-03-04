/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INCLUDE_ULPFEC_RECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_INCLUDE_ULPFEC_RECEIVER_H_

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct FecPacketCounter {
  FecPacketCounter()
      : num_packets(0),
        num_fec_packets(0),
        num_recovered_packets(0),
        first_packet_time_ms(-1) {}

  size_t num_packets;            // Number of received packets.
  size_t num_fec_packets;        // Number of received FEC packets.
  size_t num_recovered_packets;  // Number of recovered media packets using FEC.
  int64_t first_packet_time_ms;  // Time when first packet is received.
};

class UlpfecReceiver {
 public:
  static UlpfecReceiver* Create(RtpData* callback);

  virtual ~UlpfecReceiver() {}

  // Takes a RED packet, strips the RED header, and adds the resulting
  // "virtual" RTP packet(s) into the internal buffer.
  //
  // TODO(brandtr): Set |ulpfec_payload_type| during constructor call,
  // rather than as a parameter here.
  virtual int32_t AddReceivedRedPacket(const RTPHeader& rtp_header,
                                       const uint8_t* incoming_rtp_packet,
                                       size_t packet_length,
                                       uint8_t ulpfec_payload_type) = 0;

  // Sends the received packets to the FEC and returns all packets
  // (both original media and recovered) through the callback.
  virtual int32_t ProcessReceivedFec() = 0;

  // Returns a counter describing the added and recovered packets.
  virtual FecPacketCounter GetPacketCounter() const = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_INCLUDE_ULPFEC_RECEIVER_H_
