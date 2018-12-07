/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_

#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <memory>
#include <queue>

#include "api/array_view.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/buffer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class RtpPacketizerH264 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded H264 frame.
  RtpPacketizerH264(rtc::ArrayView<const uint8_t> payload,
                    PayloadSizeLimits limits,
                    H264PacketizationMode packetization_mode,
                    const RTPFragmentationHeader& fragmentation);

  ~RtpPacketizerH264() override;

  size_t NumPackets() const override;

  // Get the next payload with H264 payload header.
  // Write payload and set marker bit of the |packet|.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* rtp_packet) override;

 private:
  // Input fragments (NAL units), with an optionally owned temporary buffer,
  // used in case the fragment gets modified.
  struct Fragment {
    Fragment(const uint8_t* buffer, size_t length);
    explicit Fragment(const Fragment& fragment);
    ~Fragment();
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

  bool GeneratePackets(H264PacketizationMode packetization_mode);
  bool PacketizeFuA(size_t fragment_index);
  size_t PacketizeStapA(size_t fragment_index);
  bool PacketizeSingleNalu(size_t fragment_index);

  void NextAggregatePacket(RtpPacketToSend* rtp_packet);
  void NextFragmentPacket(RtpPacketToSend* rtp_packet);

  const PayloadSizeLimits limits_;
  size_t num_packets_left_;
  std::deque<Fragment> input_fragments_;
  std::queue<PacketUnit> packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpPacketizerH264);
};

// Depacketizer for H264.
class RtpDepacketizerH264 : public RtpDepacketizer {
 public:
  RtpDepacketizerH264();
  ~RtpDepacketizerH264() override;

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
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
