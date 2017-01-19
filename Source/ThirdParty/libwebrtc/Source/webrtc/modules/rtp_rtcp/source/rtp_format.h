/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtpPacketizer {
 public:
  static RtpPacketizer* Create(RtpVideoCodecTypes type,
                               size_t max_payload_len,
                               const RTPVideoTypeHeader* rtp_type_header,
                               FrameType frame_type);

  virtual ~RtpPacketizer() {}

  virtual void SetPayloadData(const uint8_t* payload_data,
                              size_t payload_size,
                              const RTPFragmentationHeader* fragmentation) = 0;

  // Get the next payload with payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. The parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns true on success or false if there was no payload to packetize.
  virtual bool NextPacket(uint8_t* buffer,
                          size_t* bytes_to_send,
                          bool* last_packet) = 0;

  virtual ProtectionType GetProtectionType() = 0;

  virtual StorageType GetStorageType(uint32_t retransmission_settings) = 0;

  virtual std::string ToString() = 0;
};

// TODO(sprang): Update the depacketizer to return a std::unqie_ptr with a copy
// of the parsed payload, rather than just a pointer into the incoming buffer.
// This way we can move some parsing out from the jitter buffer into here, and
// the jitter buffer can just store that pointer rather than doing a copy there.
class RtpDepacketizer {
 public:
  struct ParsedPayload {
    const uint8_t* payload;
    size_t payload_length;
    FrameType frame_type;
    RTPTypeHeader type;
  };

  static RtpDepacketizer* Create(RtpVideoCodecTypes type);

  virtual ~RtpDepacketizer() {}

  // Parses the RTP payload, parsed result will be saved in |parsed_payload|.
  virtual bool Parse(ParsedPayload* parsed_payload,
                     const uint8_t* payload_data,
                     size_t payload_data_length) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
