/*
 *  Intel License
 * See https://01.org/open-webrtc-toolkit
 * This is released under Apache License 2.0 and it is free for both academic and commercial use.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H265_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H265_H_

#include <queue>
#include <string>
#include "api/array_view.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/video_coding/codecs/h265/include/h265_globals.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class RtpPacketizerH265 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded H.265 frame.
  RtpPacketizerH265(rtc::ArrayView<const uint8_t> payload,
                    PayloadSizeLimits limits,
                    H265PacketizationMode packetization_mode);

   ~RtpPacketizerH265() override;

  size_t NumPackets() const override;

  // Get the next payload with H.265 payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. The parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns true on success or false if there was no payload to packetize.
  bool NextPacket(RtpPacketToSend* rtp_packet) override;

 private:
  struct Packet {
    Packet(size_t offset,
           size_t size,
           bool first_fragment,
           bool last_fragment,
           bool aggregated,
           uint16_t header)
        : offset(offset),
          size(size),
          first_fragment(first_fragment),
          last_fragment(last_fragment),
          aggregated(aggregated),
          header(header) {}

    size_t offset;
    size_t size;
    bool first_fragment;
    bool last_fragment;
    bool aggregated;
    uint16_t header;  // Different from H264
  };
  struct Fragment {
    Fragment(const uint8_t* buffer, size_t length);
    explicit Fragment(const Fragment& fragment);
    const uint8_t* buffer = nullptr;
    size_t length = 0;
    std::unique_ptr<rtc::Buffer> tmp_buffer;
  };
  struct PacketUnit {
    PacketUnit(const Fragment& source_fragment,
               bool first_fragment,
               bool last_fragment,
               bool aggregated,
               uint16_t header)
        : source_fragment(source_fragment),
          first_fragment(first_fragment),
          last_fragment(last_fragment),
          aggregated(aggregated),
          header(header) {}

    const Fragment source_fragment;
    bool first_fragment;
    bool last_fragment;
    bool aggregated;
    uint16_t header;
  };
  typedef std::queue<Packet> PacketQueue;
  std::deque<Fragment> input_fragments_;
  std::queue<PacketUnit> packets_;

  bool GeneratePackets(H265PacketizationMode packetization_mode);
  bool PacketizeFu(size_t fragment_index);
  int PacketizeAp(size_t fragment_index);
  bool PacketizeSingleNalu(size_t fragment_index);

  void NextAggregatePacket(RtpPacketToSend* rtp_packet, bool last);
  void NextFragmentPacket(RtpPacketToSend* rtp_packet);

  const PayloadSizeLimits limits_;
  size_t num_packets_left_;
};

// Depacketizer for H.265.
class VideoRtpDepacketizerH265 : public VideoRtpDepacketizer {
 public:
  virtual ~VideoRtpDepacketizerH265() {}

  absl::optional<ParsedRtpPayload> Parse(
             rtc::CopyOnWriteBuffer rtp_payload) override;

 private:
  bool ParseFuNalu(ParsedRtpPayload* parsed_payload,
                   const uint8_t* payload_data);
  bool ProcessApOrSingleNalu(ParsedRtpPayload* parsed_payload,
                             const uint8_t* payload_data);

  size_t offset_;
  size_t length_;
  std::unique_ptr<rtc::Buffer> modified_buffer_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H265_H_
