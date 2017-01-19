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

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_

#include <queue>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/typedefs.h"

namespace webrtc {

enum VP8PacketizerMode {
  kStrict = 0,  // Split partitions if too large;
                // never aggregate, balance size.
  kAggregate,   // Split partitions if too large; aggregate whole partitions.
  kEqualSize,   // Split entire payload without considering partition limits.
                // This will produce equal size packets for the whole frame.
  kNumModes,
};

// Packetizer for VP8.
class RtpPacketizerVp8 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder and fragmentation info.
  // The payload_data must be exactly one encoded VP8 frame.
  RtpPacketizerVp8(const RTPVideoHeaderVP8& hdr_info,
                   size_t max_payload_len,
                   VP8PacketizerMode mode);

  // Initialize without fragmentation info. Mode kEqualSize will be used.
  // The payload_data must be exactly one encoded VP8 frame.
  RtpPacketizerVp8(const RTPVideoHeaderVP8& hdr_info, size_t max_payload_len);

  virtual ~RtpPacketizerVp8();

  void SetPayloadData(const uint8_t* payload_data,
                      size_t payload_size,
                      const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with VP8 payload header.
  // max_payload_len limits the sum length of payload and VP8 payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. Parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // For the kStrict and kAggregate mode: returns the partition index from which
  // the first payload byte in the packet is taken, with the first partition
  // having index 0; returns negative on error.
  // For the kEqualSize mode: returns 0 on success, return negative on error.
  bool NextPacket(uint8_t* buffer,
                  size_t* bytes_to_send,
                  bool* last_packet) override;

  ProtectionType GetProtectionType() override;

  StorageType GetStorageType(uint32_t retransmission_settings) override;

  std::string ToString() override;

 private:
  typedef struct {
    size_t payload_start_pos;
    size_t size;
    bool first_fragment;
    size_t first_partition_ix;
  } InfoStruct;
  typedef std::queue<InfoStruct> InfoQueue;
  enum AggregationMode {
    kAggrNone = 0,    // No aggregation.
    kAggrPartitions,  // Aggregate intact partitions.
    kAggrFragments    // Aggregate intact and fragmented partitions.
  };

  static const AggregationMode aggr_modes_[kNumModes];
  static const bool balance_modes_[kNumModes];
  static const bool separate_first_modes_[kNumModes];
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

  // Calculate size of next chunk to send. Returns 0 if none can be sent.
  size_t CalcNextSize(size_t max_payload_len,
                      size_t remaining_bytes,
                      bool split_payload) const;

  // Calculate all packet sizes and load to packet info queue.
  int GeneratePackets();

  // Calculate all packet sizes using Vp8PartitionAggregator and load to packet
  // info queue.
  int GeneratePacketsBalancedAggregates();

  // Helper function to GeneratePacketsBalancedAggregates(). Find all
  // continuous sets of partitions smaller than the max payload size (not
  // max_size), and aggregate them into balanced packets. The result is written
  // to partition_vec, which is of the same length as the number of partitions.
  // A value of -1 indicates that the partition is too large and must be split.
  // Aggregates are numbered 0, 1, 2, etc. For each set of small partitions,
  // the aggregate numbers restart at 0. Output values min_size and max_size
  // will hold the smallest and largest resulting aggregates (i.e., not counting
  // those that must be split).
  void AggregateSmallPartitions(std::vector<int>* partition_vec,
                                int* min_size,
                                int* max_size);

  // Insert packet into packet queue.
  void QueuePacket(size_t start_pos,
                   size_t packet_size,
                   size_t first_partition_in_packet,
                   bool start_on_new_fragment);

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
  RTPFragmentationHeader part_info_;
  const size_t vp8_fixed_payload_descriptor_bytes_;  // Length of VP8 payload
                                                     // descriptors' fixed part.
  const AggregationMode aggr_mode_;
  const bool balance_;
  const bool separate_first_;
  const RTPVideoHeaderVP8 hdr_info_;
  size_t num_partitions_;
  const size_t max_payload_len_;
  InfoQueue packets_;
  bool packets_calculated_;

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
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VP8_H_
