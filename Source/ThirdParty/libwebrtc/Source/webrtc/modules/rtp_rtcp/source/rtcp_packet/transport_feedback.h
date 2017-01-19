/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TRANSPORT_FEEDBACK_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TRANSPORT_FEEDBACK_H_

#include <deque>
#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/deprecation.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

class TransportFeedback : public Rtpfb {
 public:
  class PacketStatusChunk;
  // TODO(sprang): IANA reg?
  static constexpr uint8_t kFeedbackMessageType = 15;
  // Convert to multiples of 0.25ms.
  static constexpr int kDeltaScaleFactor = 250;

  TransportFeedback();
  ~TransportFeedback() override;

  void SetBase(uint16_t base_sequence,     // Seq# of first packet in this msg.
               int64_t ref_timestamp_us);  // Reference timestamp for this msg.
  void SetFeedbackSequenceNumber(uint8_t feedback_sequence);
  // NOTE: This method requires increasing sequence numbers (excepting wraps).
  bool AddReceivedPacket(uint16_t sequence_number, int64_t timestamp_us);

  enum class StatusSymbol {
    kNotReceived,
    kReceivedSmallDelta,
    kReceivedLargeDelta,
  };

  uint16_t GetBaseSequence() const;
  std::vector<TransportFeedback::StatusSymbol> GetStatusVector() const;
  std::vector<int16_t> GetReceiveDeltas() const;

  // Get the reference time in microseconds, including any precision loss.
  int64_t GetBaseTimeUs() const;
  // Convenience method for getting all deltas as microseconds. The first delta
  // is relative the base time.
  std::vector<int64_t> GetReceiveDeltasUs() const;

  bool Parse(const CommonHeader& packet);
  static std::unique_ptr<TransportFeedback> ParseFrom(const uint8_t* buffer,
                                                      size_t length);

  RTC_DEPRECATED
  void WithPacketSenderSsrc(uint32_t ssrc) { SetSenderSsrc(ssrc); }
  RTC_DEPRECATED
  void WithMediaSourceSsrc(uint32_t ssrc) { SetMediaSsrc(ssrc); }
  RTC_DEPRECATED
  void WithBase(uint16_t base_sequence, int64_t ref_timestamp_us) {
    SetBase(base_sequence, ref_timestamp_us);
  }
  RTC_DEPRECATED
  void WithFeedbackSequenceNumber(uint8_t feedback_sequence) {
    SetFeedbackSequenceNumber(feedback_sequence);
  }
  RTC_DEPRECATED
  bool WithReceivedPacket(uint16_t sequence_number, int64_t timestamp_us) {
    return AddReceivedPacket(sequence_number, timestamp_us);
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* position,
              size_t max_length,
              PacketReadyCallback* callback) const override;

  size_t BlockLength() const override;

 private:
  static PacketStatusChunk* ParseChunk(const uint8_t* buffer, size_t max_size);

  int64_t Unwrap(uint16_t sequence_number);
  bool AddSymbol(StatusSymbol symbol, int64_t seq);
  bool Encode(StatusSymbol symbol);
  bool HandleRleCandidate(StatusSymbol symbol,
                          int current_capacity,
                          int delta_size);
  void EmitRemaining();
  void EmitVectorChunk();
  void EmitRunLengthChunk();

  int32_t base_seq_;
  int64_t base_time_;
  uint8_t feedback_seq_;
  std::vector<PacketStatusChunk*> status_chunks_;
  std::vector<int16_t> receive_deltas_;

  int64_t last_seq_;
  int64_t last_timestamp_;
  std::deque<StatusSymbol> symbol_vec_;
  uint16_t first_symbol_cardinality_;
  bool vec_needs_two_bit_symbols_;
  uint32_t size_bytes_;

  RTC_DISALLOW_COPY_AND_ASSIGN(TransportFeedback);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TRANSPORT_FEEDBACK_H_
