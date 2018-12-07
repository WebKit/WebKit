/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_
#define MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>
#include <list>
#include <memory>
#include <vector>

#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"

namespace webrtc {

class FlexfecSender;

class RedPacket {
 public:
  explicit RedPacket(size_t length);
  ~RedPacket();

  void CreateHeader(const uint8_t* rtp_header,
                    size_t header_length,
                    int red_payload_type,
                    int payload_type);
  void SetSeqNum(int seq_num);
  void AssignPayload(const uint8_t* payload, size_t length);
  void ClearMarkerBit();
  uint8_t* data() const;
  size_t length() const;

 private:
  std::unique_ptr<uint8_t[]> data_;
  size_t length_;
  size_t header_length_;
};

class UlpfecGenerator {
  friend class FlexfecSender;

 public:
  UlpfecGenerator();
  ~UlpfecGenerator();

  void SetFecParameters(const FecProtectionParams& params);

  // Adds a media packet to the internal buffer. When enough media packets
  // have been added, the FEC packets are generated and stored internally.
  // These FEC packets are then obtained by calling GetFecPacketsAsRed().
  int AddRtpPacketAndGenerateFec(const uint8_t* data_buffer,
                                 size_t payload_length,
                                 size_t rtp_header_length);

  // Returns true if there are generated FEC packets available.
  bool FecAvailable() const;

  size_t NumAvailableFecPackets() const;

  // Returns the overhead, per packet, for FEC (and possibly RED).
  size_t MaxPacketOverhead() const;

  // Returns generated FEC packets with RED headers added.
  std::vector<std::unique_ptr<RedPacket>> GetUlpfecPacketsAsRed(
      int red_payload_type,
      int ulpfec_payload_type,
      uint16_t first_seq_num);

 private:
  explicit UlpfecGenerator(std::unique_ptr<ForwardErrorCorrection> fec);

  // Overhead is defined as relative to the number of media packets, and not
  // relative to total number of packets. This definition is inherited from the
  // protection factor produced by video_coding module and how the FEC
  // generation is implemented.
  int Overhead() const;

  // Returns true if the excess overhead (actual - target) for the FEC is below
  // the amount |kMaxExcessOverhead|. This effects the lower protection level
  // cases and low number of media packets/frame. The target overhead is given
  // by |params_.fec_rate|, and is only achievable in the limit of large number
  // of media packets.
  bool ExcessOverheadBelowMax() const;

  // Returns true if the number of added media packets is at least
  // |min_num_media_packets_|. This condition tries to capture the effect
  // that, for the same amount of protection/overhead, longer codes
  // (e.g. (2k,2m) vs (k,m)) are generally more effective at recovering losses.
  bool MinimumMediaPacketsReached() const;

  void ResetState();

  std::unique_ptr<ForwardErrorCorrection> fec_;
  ForwardErrorCorrection::PacketList media_packets_;
  size_t last_media_packet_rtp_header_length_;
  std::list<ForwardErrorCorrection::Packet*> generated_fec_packets_;
  int num_protected_frames_;
  int min_num_media_packets_;
  FecProtectionParams params_;
  FecProtectionParams new_params_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_
