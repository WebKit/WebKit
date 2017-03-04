/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// This file contains the declaration of the VP9 packetizer class.
// A packetizer object is created for each encoded video frame. The
// constructor is called with the payload data and size.
//
// After creating the packetizer, the method NextPacket is called
// repeatedly to get all packets for the frame. The method returns
// false as long as there are more packets left to fetch.
//

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP9_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP9_H_

#include <queue>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpPacketizerVp9 : public RtpPacketizer {
 public:
  RtpPacketizerVp9(const RTPVideoHeaderVP9& hdr, size_t max_payload_length);

  virtual ~RtpPacketizerVp9();

  ProtectionType GetProtectionType() override;

  StorageType GetStorageType(uint32_t retransmission_settings) override;

  std::string ToString() override;

  // The payload data must be one encoded VP9 frame.
  void SetPayloadData(const uint8_t* payload,
                      size_t payload_size,
                      const RTPFragmentationHeader* fragmentation) override;

  // Gets the next payload with VP9 payload header.
  // Write payload and set marker bit of the |packet|.
  // The parameter |last_packet| is true for the last packet of the frame, false
  // otherwise (i.e., call the function again to get the next packet).
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet, bool* last_packet) override;

  typedef struct {
    size_t payload_start_pos;
    size_t size;
    bool layer_begin;
    bool layer_end;
  } PacketInfo;
  typedef std::queue<PacketInfo> PacketInfoQueue;

 private:
  // Calculates all packet sizes and loads info to packet queue.
  void GeneratePackets();

  // Writes the payload descriptor header and copies payload to the |buffer|.
  // |packet_info| determines which part of the payload to write.
  // |bytes_to_send| contains the number of written bytes to the buffer.
  // Returns true on success, false otherwise.
  bool WriteHeaderAndPayload(const PacketInfo& packet_info,
                             RtpPacketToSend* packet) const;

  // Writes payload descriptor header to |buffer|.
  // Returns true on success, false otherwise.
  bool WriteHeader(const PacketInfo& packet_info,
                   uint8_t* buffer,
                   size_t* header_length) const;

  const RTPVideoHeaderVP9 hdr_;
  const size_t max_payload_length_;  // The max length in bytes of one packet.
  const uint8_t* payload_;           // The payload data to be packetized.
  size_t payload_size_;              // The size in bytes of the payload data.
  PacketInfoQueue packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpPacketizerVp9);
};


class RtpDepacketizerVp9 : public RtpDepacketizer {
 public:
  virtual ~RtpDepacketizerVp9() {}

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload,
             size_t payload_length) override;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP9_H_
