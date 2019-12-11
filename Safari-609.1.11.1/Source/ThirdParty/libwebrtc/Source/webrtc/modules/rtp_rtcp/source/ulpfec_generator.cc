/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/ulpfec_generator.h"

#include <string.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr size_t kRedForFecHeaderLength = 1;

// This controls the maximum amount of excess overhead (actual - target)
// allowed in order to trigger EncodeFec(), before |params_.max_fec_frames|
// is reached. Overhead here is defined as relative to number of media packets.
constexpr int kMaxExcessOverhead = 50;  // Q8.

// This is the minimum number of media packets required (above some protection
// level) in order to trigger EncodeFec(), before |params_.max_fec_frames| is
// reached.
constexpr size_t kMinMediaPackets = 4;

// Threshold on the received FEC protection level, above which we enforce at
// least |kMinMediaPackets| packets for the FEC code. Below this
// threshold |kMinMediaPackets| is set to default value of 1.
//
// The range is between 0 and 255, where 255 corresponds to 100% overhead
// (relative to the number of protected media packets).
constexpr uint8_t kHighProtectionThreshold = 80;

// This threshold is used to adapt the |kMinMediaPackets| threshold, based
// on the average number of packets per frame seen so far. When there are few
// packets per frame (as given by this threshold), at least
// |kMinMediaPackets| + 1 packets are sent to the FEC code.
constexpr float kMinMediaPacketsAdaptationThreshold = 2.0f;

// At construction time, we don't know the SSRC that is used for the generated
// FEC packets, but we still need to give it to the ForwardErrorCorrection ctor
// to be used in the decoding.
// TODO(brandtr): Get rid of this awkwardness by splitting
// ForwardErrorCorrection in two objects -- one encoder and one decoder.
constexpr uint32_t kUnknownSsrc = 0;

}  // namespace

RedPacket::RedPacket(size_t length)
    : data_(new uint8_t[length]), length_(length), header_length_(0) {}

RedPacket::~RedPacket() = default;

void RedPacket::CreateHeader(const uint8_t* rtp_header,
                             size_t header_length,
                             int red_payload_type,
                             int payload_type) {
  RTC_DCHECK_LE(header_length + kRedForFecHeaderLength, length_);
  memcpy(data_.get(), rtp_header, header_length);
  // Replace payload type.
  data_[1] &= 0x80;
  data_[1] += red_payload_type;
  // Add RED header
  // f-bit always 0
  data_[header_length] = static_cast<uint8_t>(payload_type);
  header_length_ = header_length + kRedForFecHeaderLength;
}

void RedPacket::SetSeqNum(int seq_num) {
  RTC_DCHECK_GE(seq_num, 0);
  RTC_DCHECK_LT(seq_num, 1 << 16);

  ByteWriter<uint16_t>::WriteBigEndian(&data_[2], seq_num);
}

void RedPacket::AssignPayload(const uint8_t* payload, size_t length) {
  RTC_DCHECK_LE(header_length_ + length, length_);
  memcpy(data_.get() + header_length_, payload, length);
}

void RedPacket::ClearMarkerBit() {
  data_[1] &= 0x7F;
}

uint8_t* RedPacket::data() const {
  return data_.get();
}

size_t RedPacket::length() const {
  return length_;
}

UlpfecGenerator::UlpfecGenerator()
    : UlpfecGenerator(ForwardErrorCorrection::CreateUlpfec(kUnknownSsrc)) {}

UlpfecGenerator::UlpfecGenerator(std::unique_ptr<ForwardErrorCorrection> fec)
    : fec_(std::move(fec)),
      last_media_packet_rtp_header_length_(0),
      num_protected_frames_(0),
      min_num_media_packets_(1) {
  memset(&params_, 0, sizeof(params_));
  memset(&new_params_, 0, sizeof(new_params_));
}

UlpfecGenerator::~UlpfecGenerator() = default;

void UlpfecGenerator::SetFecParameters(const FecProtectionParams& params) {
  RTC_DCHECK_GE(params.fec_rate, 0);
  RTC_DCHECK_LE(params.fec_rate, 255);
  // Store the new params and apply them for the next set of FEC packets being
  // produced.
  new_params_ = params;
  if (params.fec_rate > kHighProtectionThreshold) {
    min_num_media_packets_ = kMinMediaPackets;
  } else {
    min_num_media_packets_ = 1;
  }
}

int UlpfecGenerator::AddRtpPacketAndGenerateFec(const uint8_t* data_buffer,
                                                size_t payload_length,
                                                size_t rtp_header_length) {
  RTC_DCHECK(generated_fec_packets_.empty());
  if (media_packets_.empty()) {
    params_ = new_params_;
  }
  bool complete_frame = false;
  const bool marker_bit = (data_buffer[1] & kRtpMarkerBitMask) ? true : false;
  if (media_packets_.size() < kUlpfecMaxMediaPackets) {
    // Our packet masks can only protect up to |kUlpfecMaxMediaPackets| packets.
    std::unique_ptr<ForwardErrorCorrection::Packet> packet(
        new ForwardErrorCorrection::Packet());
    packet->length = payload_length + rtp_header_length;
    memcpy(packet->data, data_buffer, packet->length);
    media_packets_.push_back(std::move(packet));
    // Keep track of the RTP header length, so we can copy the RTP header
    // from |packet| to newly generated ULPFEC+RED packets.
    RTC_DCHECK_GE(rtp_header_length, kRtpHeaderSize);
    last_media_packet_rtp_header_length_ = rtp_header_length;
  }
  if (marker_bit) {
    ++num_protected_frames_;
    complete_frame = true;
  }
  // Produce FEC over at most |params_.max_fec_frames| frames, or as soon as:
  // (1) the excess overhead (actual overhead - requested/target overhead) is
  // less than |kMaxExcessOverhead|, and
  // (2) at least |min_num_media_packets_| media packets is reached.
  if (complete_frame &&
      (num_protected_frames_ == params_.max_fec_frames ||
       (ExcessOverheadBelowMax() && MinimumMediaPacketsReached()))) {
    // We are not using Unequal Protection feature of the parity erasure code.
    constexpr int kNumImportantPackets = 0;
    constexpr bool kUseUnequalProtection = false;
    int ret = fec_->EncodeFec(media_packets_, params_.fec_rate,
                              kNumImportantPackets, kUseUnequalProtection,
                              params_.fec_mask_type, &generated_fec_packets_);
    if (generated_fec_packets_.empty()) {
      ResetState();
    }
    return ret;
  }
  return 0;
}

bool UlpfecGenerator::ExcessOverheadBelowMax() const {
  return ((Overhead() - params_.fec_rate) < kMaxExcessOverhead);
}

bool UlpfecGenerator::MinimumMediaPacketsReached() const {
  float average_num_packets_per_frame =
      static_cast<float>(media_packets_.size()) / num_protected_frames_;
  int num_media_packets = static_cast<int>(media_packets_.size());
  if (average_num_packets_per_frame < kMinMediaPacketsAdaptationThreshold) {
    return num_media_packets >= min_num_media_packets_;
  } else {
    // For larger rates (more packets/frame), increase the threshold.
    // TODO(brandtr): Investigate what impact this adaptation has.
    return num_media_packets >= min_num_media_packets_ + 1;
  }
}

bool UlpfecGenerator::FecAvailable() const {
  return !generated_fec_packets_.empty();
}

size_t UlpfecGenerator::NumAvailableFecPackets() const {
  return generated_fec_packets_.size();
}

size_t UlpfecGenerator::MaxPacketOverhead() const {
  return fec_->MaxPacketOverhead();
}

std::vector<std::unique_ptr<RedPacket>> UlpfecGenerator::GetUlpfecPacketsAsRed(
    int red_payload_type,
    int ulpfec_payload_type,
    uint16_t first_seq_num) {
  std::vector<std::unique_ptr<RedPacket>> red_packets;
  red_packets.reserve(generated_fec_packets_.size());
  RTC_DCHECK(!media_packets_.empty());
  ForwardErrorCorrection::Packet* last_media_packet =
      media_packets_.back().get();
  uint16_t seq_num = first_seq_num;
  for (const auto* fec_packet : generated_fec_packets_) {
    // Wrap FEC packet (including FEC headers) in a RED packet. Since the
    // FEC packets in |generated_fec_packets_| don't have RTP headers, we
    // reuse the header from the last media packet.
    RTC_DCHECK_GT(last_media_packet_rtp_header_length_, 0);
    std::unique_ptr<RedPacket> red_packet(
        new RedPacket(last_media_packet_rtp_header_length_ +
                      kRedForFecHeaderLength + fec_packet->length));
    red_packet->CreateHeader(last_media_packet->data,
                             last_media_packet_rtp_header_length_,
                             red_payload_type, ulpfec_payload_type);
    red_packet->SetSeqNum(seq_num++);
    red_packet->ClearMarkerBit();
    red_packet->AssignPayload(fec_packet->data, fec_packet->length);
    red_packets.push_back(std::move(red_packet));
  }

  ResetState();

  return red_packets;
}

int UlpfecGenerator::Overhead() const {
  RTC_DCHECK(!media_packets_.empty());
  int num_fec_packets =
      fec_->NumFecPackets(media_packets_.size(), params_.fec_rate);
  // Return the overhead in Q8.
  return (num_fec_packets << 8) / media_packets_.size();
}

void UlpfecGenerator::ResetState() {
  media_packets_.clear();
  last_media_packet_rtp_header_length_ = 0;
  generated_fec_packets_.clear();
  num_protected_frames_ = 0;
}

}  // namespace webrtc
