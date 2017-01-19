/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_

#include <deque>
#include <queue>
#include <string>

#include "webrtc/base/buffer.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"

namespace webrtc {

class RtpPacketizerH264 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded H264 frame.
  RtpPacketizerH264(FrameType frame_type, size_t max_payload_len);

  virtual ~RtpPacketizerH264();

  void SetPayloadData(const uint8_t* payload_data,
                      size_t payload_size,
                      const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with H264 payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. The parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns true on success or false if there was no payload to packetize.
  bool NextPacket(uint8_t* buffer,
                  size_t* bytes_to_send,
                  bool* last_packet) override;

  ProtectionType GetProtectionType() override;

  StorageType GetStorageType(uint32_t retransmission_settings) override;

  std::string ToString() override;

 private:
  // Input fragments (NAL units), with an optionally owned temporary buffer,
  // used in case the fragment gets modified.
  struct Fragment {
    Fragment(const uint8_t* buffer, size_t length);
    explicit Fragment(const Fragment& fragment);
    const uint8_t* buffer = nullptr;
    size_t length = 0;
    std::unique_ptr<rtc::Buffer> tmp_buffer;
  };

  // A packet unit (H264 packet), to be put into an RTP packet:
  // If a NAL unit is too large for an RTP packet, this packet unit will
  // represent a FU-A packet of a single fragment of the NAL unit.
  // If a NAL unit is small enough to fit within a single RTP packet, this
  // packet unit may represent a single NAL unit or a STAP-A packet, of which
  // there may be multiple in a single RTP packet (if so, aggregated = true).
  struct PacketUnit {
    PacketUnit(const Fragment& source_fragment,
               bool first_fragment,
               bool last_fragment,
               bool aggregated,
               uint8_t header)
        : source_fragment(source_fragment),
          first_fragment(first_fragment),
          last_fragment(last_fragment),
          aggregated(aggregated),
          header(header) {}

    const Fragment source_fragment;
    bool first_fragment;
    bool last_fragment;
    bool aggregated;
    uint8_t header;
  };

  void GeneratePackets();
  void PacketizeFuA(size_t fragment_index);
  size_t PacketizeStapA(size_t fragment_index);
  void NextAggregatePacket(uint8_t* buffer, size_t* bytes_to_send);
  void NextFragmentPacket(uint8_t* buffer, size_t* bytes_to_send);

  const size_t max_payload_len_;
  std::deque<Fragment> input_fragments_;
  std::queue<PacketUnit> packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpPacketizerH264);
};

// Depacketizer for H264.
class RtpDepacketizerH264 : public RtpDepacketizer {
 public:
  RtpDepacketizerH264();
  virtual ~RtpDepacketizerH264();

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;

 private:
  bool ParseFuaNalu(RtpDepacketizer::ParsedPayload* parsed_payload,
                    const uint8_t* payload_data);
  bool ProcessStapAOrSingleNalu(RtpDepacketizer::ParsedPayload* parsed_payload,
                                const uint8_t* payload_data);

  size_t offset_;
  size_t length_;
  std::unique_ptr<rtc::Buffer> modified_buffer_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
