/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_
#define AUDIO_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_

#include <map>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

namespace rtcp {
class TransportFeedback;
}

struct PacketFeedback;

class TransportFeedbackPacketLossTracker final {
 public:
  // * We count up to |max_window_size_ms| from the sent
  //   time of the latest acked packet for the calculation of the metrics.
  // * PLR (packet-loss-rate) is reliably computable once the statuses of
  //   |plr_min_num_acked_packets| packets are known.
  // * RPLR (recoverable-packet-loss-rate) is reliably computable once the
  //   statuses of |rplr_min_num_acked_pairs| pairs are known.
  TransportFeedbackPacketLossTracker(int64_t max_window_size_ms,
                                     size_t plr_min_num_acked_packets,
                                     size_t rplr_min_num_acked_pairs);
  ~TransportFeedbackPacketLossTracker();

  void OnPacketAdded(uint16_t seq_num, int64_t send_time_ms);

  void OnPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedbacks_vector);

  // Returns the packet loss rate, if the window has enough packet statuses to
  // reliably compute it. Otherwise, returns empty.
  absl::optional<float> GetPacketLossRate() const;

  // Returns the first-order-FEC recoverable packet loss rate, if the window has
  // enough status pairs to reliably compute it. Otherwise, returns empty.
  absl::optional<float> GetRecoverablePacketLossRate() const;

  // Verifies that the internal states are correct. Only used for tests.
  void Validate() const;

 private:
  // When a packet is sent, we memorize its association with the stream by
  // marking it as (sent-but-so-far-) unacked. If we ever receive a feedback
  // that reports it as received/lost, we update the state and
  // metrics accordingly.

  enum class PacketStatus { Unacked = 0, Received = 1, Lost = 2 };
  struct SentPacket {
    SentPacket(int64_t send_time_ms, PacketStatus status)
        : send_time_ms(send_time_ms), status(status) {}
    int64_t send_time_ms;
    PacketStatus status;
  };
  typedef std::map<uint16_t, SentPacket> SentPacketStatusMap;
  typedef SentPacketStatusMap::const_iterator ConstPacketStatusIterator;

  void Reset();

  // ReferenceSequenceNumber() provides a sequence number that defines the
  // order of packet reception info stored in |packet_status_window_|. In
  // particular, given any sequence number |x|,
  // (2^16 + x - ref_seq_num_) % 2^16 defines its actual position in
  // |packet_status_window_|.
  uint16_t ReferenceSequenceNumber() const;
  uint16_t NewestSequenceNumber() const;
  void UpdatePacketStatus(SentPacketStatusMap::iterator it,
                          PacketStatus new_status);
  void RemoveOldestPacketStatus();

  void UpdateMetrics(ConstPacketStatusIterator it,
                     bool apply /* false = undo */);
  void UpdatePlr(ConstPacketStatusIterator it, bool apply /* false = undo */);
  void UpdateRplr(ConstPacketStatusIterator it, bool apply /* false = undo */);

  ConstPacketStatusIterator PreviousPacketStatus(
      ConstPacketStatusIterator it) const;
  ConstPacketStatusIterator NextPacketStatus(
      ConstPacketStatusIterator it) const;

  const int64_t max_window_size_ms_;
  size_t acked_packets_;

  SentPacketStatusMap packet_status_window_;
  // |ref_packet_status_| points to the oldest item in |packet_status_window_|.
  ConstPacketStatusIterator ref_packet_status_;

  // Packet-loss-rate calculation (lost / all-known-packets).
  struct PlrState {
    explicit PlrState(size_t min_num_acked_packets)
        : min_num_acked_packets_(min_num_acked_packets) {
      Reset();
    }
    void Reset() {
      num_received_packets_ = 0;
      num_lost_packets_ = 0;
    }
    absl::optional<float> GetMetric() const;
    const size_t min_num_acked_packets_;
    size_t num_received_packets_;
    size_t num_lost_packets_;
  } plr_state_;

  // Recoverable packet loss calculation (first-order-FEC recoverable).
  struct RplrState {
    explicit RplrState(size_t min_num_acked_pairs)
        : min_num_acked_pairs_(min_num_acked_pairs) {
      Reset();
    }
    void Reset() {
      num_acked_pairs_ = 0;
      num_recoverable_losses_ = 0;
    }
    absl::optional<float> GetMetric() const;
    // Recoverable packets are those which were lost, but immediately followed
    // by a properly received packet. If that second packet carried FEC,
    // the data from the former (lost) packet could be recovered.
    // The RPLR is calculated as the fraction of such pairs (lost-received) out
    // of all pairs of consecutive acked packets.
    const size_t min_num_acked_pairs_;
    size_t num_acked_pairs_;
    size_t num_recoverable_losses_;
  } rplr_state_;
};

}  // namespace webrtc

#endif  // AUDIO_TRANSPORT_FEEDBACK_PACKET_LOSS_TRACKER_H_
