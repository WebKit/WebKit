/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the declaration of the VP8 packetizer class.
 * A packetizer object is created for each encoded video frame. The
 * constructor is called with the payload data and size,
 * together with the fragmentation information and a packetizer mode
 * of choice. Alternatively, if no fragmentation info is available, the
 * second constructor can be used with only payload data and size; in that
 * case the mode kEqualSize is used.
 *
 * After creating the packetizer, the method NextPacket is called
 * repeatedly to get all packets for the frame. The method returns
 * false as long as there are more packets left to fetch.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_

#include <queue>
#include <string>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "rtc_base/constructormagic.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

// Packetizer for VP8.
class RtpPacketizerVp8 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded VP8 frame.
  RtpPacketizerVp8(const RTPVideoHeaderVP8& hdr_info,
                   size_t max_payload_len,
                   size_t last_packet_reduction_len);

  virtual ~RtpPacketizerVp8();

  size_t SetPayloadData(const uint8_t* payload_data,
                        size_t payload_size,
                        const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with VP8 payload header.
  // Write payload and set marker bit of the |packet|.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet) override;

  std::string ToString() override;

 private:
  typedef struct {
    size_t payload_start_pos;
    size_t size;
    bool first_packet;
  } InfoStruct;
  typedef std::queue<InfoStruct> InfoQueue;

  static const int kXBit = 0x80;
  static const int kNBit = 0x20;
  static const int kSBit = 0x10;
  static const int kPartIdField = 0x0F;
  static const int kKeyIdxField = 0x1F;
  static const int kIBit = 0x80;
  static const int kLBit = 0x40;
  static const int kTBit = 0x20;
  static const int kKBit = 0x10;
  static const int kYBit = 0x20;

  // Calculate all packet sizes and load to packet info queue.
  int GeneratePackets();

  // Splits given part of payload to packets with a given capacity. The last
  // packet should be reduced by last_packet_reduction_len_.
  void GeneratePacketsSplitPayloadBalanced(size_t payload_len,
                                           size_t capacity);

  // Insert packet into packet queue.
  void QueuePacket(size_t start_pos,
                   size_t packet_size,
                   bool first_packet);

  // Write the payload header and copy the payload to the buffer.
  // The info in packet_info determines which part of the payload is written
  // and what to write in the header fields.
  int WriteHeaderAndPayload(const InfoStruct& packet_info,
                            uint8_t* buffer,
                            size_t buffer_length) const;

  // Write the X field and the appropriate extension fields to buffer.
  // The function returns the extension length (including X field), or -1
  // on error.
  int WriteExtensionFields(uint8_t* buffer, size_t buffer_length) const;

  // Set the I bit in the x_field, and write PictureID to the appropriate
  // position in buffer. The function returns 0 on success, -1 otherwise.
  int WritePictureIDFields(uint8_t* x_field,
                           uint8_t* buffer,
                           size_t buffer_length,
                           size_t* extension_length) const;

  // Set the L bit in the x_field, and write Tl0PicIdx to the appropriate
  // position in buffer. The function returns 0 on success, -1 otherwise.
  int WriteTl0PicIdxFields(uint8_t* x_field,
                           uint8_t* buffer,
                           size_t buffer_length,
                           size_t* extension_length) const;

  // Set the T and K bits in the x_field, and write TID, Y and KeyIdx to the
  // appropriate position in buffer. The function returns 0 on success,
  // -1 otherwise.
  int WriteTIDAndKeyIdxFields(uint8_t* x_field,
                              uint8_t* buffer,
                              size_t buffer_length,
                              size_t* extension_length) const;

  // Write the PictureID from codec_specific_info_ to buffer. One or two
  // bytes are written, depending on magnitude of PictureID. The function
  // returns the number of bytes written.
  int WritePictureID(uint8_t* buffer, size_t buffer_length) const;

  // Calculate and return length (octets) of the variable header fields in
  // the next header (i.e., header length in addition to vp8_header_bytes_).
  size_t PayloadDescriptorExtraLength() const;

  // Calculate and return length (octets) of PictureID field in the next
  // header. Can be 0, 1, or 2.
  size_t PictureIdLength() const;

  // Check whether each of the optional fields will be included in the header.
  bool XFieldPresent() const;
  bool TIDFieldPresent() const;
  bool KeyIdxFieldPresent() const;
  bool TL0PicIdxFieldPresent() const;
  bool PictureIdPresent() const { return (PictureIdLength() > 0); }

  const uint8_t* payload_data_;
  size_t payload_size_;
  const size_t vp8_fixed_payload_descriptor_bytes_;  // Length of VP8 payload
                                                     // descriptors' fixed part.
  const RTPVideoHeaderVP8 hdr_info_;
  const size_t max_payload_len_;
  const size_t last_packet_reduction_len_;
  InfoQueue packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpPacketizerVp8);
};

// Depacketizer for VP8.
class RtpDepacketizerVp8 : public RtpDepacketizer {
 public:
  virtual ~RtpDepacketizerVp8() {}

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_
