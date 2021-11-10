/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_
#define NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/retransmission_timeout.h"
#include "net/dcsctp/tx/send_queue.h"

namespace dcsctp {

// The RetransmissionQueue manages all DATA/I-DATA chunks that are in-flight and
// schedules them to be retransmitted if necessary. Chunks are retransmitted
// when they have been lost for a number of consecutive SACKs, or when the
// retransmission timer, `t3_rtx` expires.
//
// As congestion control is tightly connected with the state of transmitted
// packets, that's also managed here to limit the amount of data that is
// in-flight (sent, but not yet acknowledged).
class RetransmissionQueue {
 public:
  static constexpr size_t kMinimumFragmentedPayload = 10;
  // State for DATA chunks (message fragments) in the queue - used in tests.
  enum class State {
    // The chunk has been sent but not received yet (from the sender's point of
    // view, as no SACK has been received yet that reference this chunk).
    kInFlight,
    // A SACK has been received which explicitly marked this chunk as missing -
    // it's now NACKED and may be retransmitted if NACKED enough times.
    kNacked,
    // A chunk that will be retransmitted when possible.
    kToBeRetransmitted,
    // A SACK has been received which explicitly marked this chunk as received.
    kAcked,
    // A chunk whose message has expired or has been retransmitted too many
    // times (RFC3758). It will not be retransmitted anymore.
    kAbandoned,
  };

  // Creates a RetransmissionQueue which will send data using `my_initial_tsn`
  // (or a value from `DcSctpSocketHandoverState` if given) as the first TSN
  // to use for sent fragments. It will poll data from `send_queue`. When SACKs
  // are received, it will estimate the RTT, and call `on_new_rtt`. When an
  // outstanding chunk has been ACKed, it will call
  // `on_clear_retransmission_counter` and will also use `t3_rtx`, which is the
  // SCTP retransmission timer to manage retransmissions.
  RetransmissionQueue(
      absl::string_view log_prefix,
      TSN my_initial_tsn,
      size_t a_rwnd,
      SendQueue& send_queue,
      std::function<void(DurationMs rtt)> on_new_rtt,
      std::function<void()> on_clear_retransmission_counter,
      Timer& t3_rtx,
      const DcSctpOptions& options,
      bool supports_partial_reliability = true,
      bool use_message_interleaving = false,
      const DcSctpSocketHandoverState* handover_state = nullptr);

  // Handles a received SACK. Returns true if the `sack` was processed and
  // false if it was discarded due to received out-of-order and not relevant.
  bool HandleSack(TimeMs now, const SackChunk& sack);

  // Handles an expired retransmission timer.
  void HandleT3RtxTimerExpiry();

  // Returns a list of chunks to send that would fit in one SCTP packet with
  // `bytes_remaining_in_packet` bytes available. This may be further limited by
  // the congestion control windows. Note that `ShouldSendForwardTSN` must be
  // called prior to this method, to abandon expired chunks, as this method will
  // not expire any chunks.
  std::vector<std::pair<TSN, Data>> GetChunksToSend(
      TimeMs now,
      size_t bytes_remaining_in_packet);

  // Returns the internal state of all queued chunks. This is only used in
  // unit-tests.
  std::vector<std::pair<TSN, State>> GetChunkStatesForTesting() const;

  // Returns the next TSN that will be allocated for sent DATA chunks.
  TSN next_tsn() const { return next_tsn_.Wrap(); }

  // Returns the size of the congestion window, in bytes. This is the number of
  // bytes that may be in-flight.
  size_t cwnd() const { return cwnd_; }

  // Overrides the current congestion window size.
  void set_cwnd(size_t cwnd) { cwnd_ = cwnd; }

  // Returns the current receiver window size.
  size_t rwnd() const { return rwnd_; }

  // Returns the number of bytes of packets that are in-flight.
  size_t outstanding_bytes() const { return outstanding_bytes_; }

  // Returns the number of DATA chunks that are in-flight.
  size_t outstanding_items() const { return outstanding_items_; }

  // Indicates if the congestion control algorithm allows data to be sent.
  bool can_send_data() const;

  // Given the current time `now`, it will evaluate if there are chunks that
  // have expired and that need to be discarded. It returns true if a
  // FORWARD-TSN should be sent.
  bool ShouldSendForwardTsn(TimeMs now);

  // Creates a FORWARD-TSN chunk.
  ForwardTsnChunk CreateForwardTsn() const;

  // Creates an I-FORWARD-TSN chunk.
  IForwardTsnChunk CreateIForwardTsn() const;

  // See the SendQueue for a longer description of these methods related
  // to stream resetting.
  void PrepareResetStreams(rtc::ArrayView<const StreamID> streams);
  bool CanResetStreams() const;
  void CommitResetStreams();
  void RollbackResetStreams();

  HandoverReadinessStatus GetHandoverReadiness() const;

  void AddHandoverState(DcSctpSocketHandoverState& state);

 private:
  enum class CongestionAlgorithmPhase {
    kSlowStart,
    kCongestionAvoidance,
  };

  // A fragmented message's DATA chunk while in the retransmission queue, and
  // its associated metadata.
  class TxData {
   public:
    enum class NackAction {
      kNothing,
      kRetransmit,
      kAbandon,
    };

    explicit TxData(Data data,
                    absl::optional<size_t> max_retransmissions,
                    TimeMs time_sent,
                    absl::optional<TimeMs> expires_at)
        : max_retransmissions_(max_retransmissions),
          time_sent_(time_sent),
          expires_at_(expires_at),
          data_(std::move(data)) {}

    TimeMs time_sent() const { return time_sent_; }

    const Data& data() const { return data_; }

    // Acks an item.
    void Ack();

    // Nacks an item. If it has been nacked enough times, or if `retransmit_now`
    // is set, it might be marked for retransmission. If the item has reached
    // its max retransmission value, it will instead be abandoned. The action
    // performed is indicated as return value.
    NackAction Nack(bool retransmit_now = false);

    // Prepares the item to be retransmitted. Sets it as outstanding and
    // clears all nack counters.
    void Retransmit();

    // Marks this item as abandoned.
    void Abandon();

    bool is_outstanding() const { return ack_state_ == AckState::kUnacked; }
    bool is_acked() const { return ack_state_ == AckState::kAcked; }
    bool is_nacked() const { return ack_state_ == AckState::kNacked; }
    bool is_abandoned() const { return is_abandoned_; }

    // Indicates if this chunk should be retransmitted.
    bool should_be_retransmitted() const { return should_be_retransmitted_; }
    // Indicates if this chunk has ever been retransmitted.
    bool has_been_retransmitted() const { return num_retransmissions_ > 0; }

    // Given the current time, and the current state of this DATA chunk, it will
    // indicate if it has expired (SCTP Partial Reliability Extension).
    bool has_expired(TimeMs now) const;

   private:
    enum class AckState {
      kUnacked,
      kAcked,
      kNacked,
    };
    // Indicates the presence of this chunk, if it's in flight (Unacked), has
    // been received (Acked) or is lost (Nacked).
    AckState ack_state_ = AckState::kUnacked;
    // Indicates if this chunk has been abandoned, which is a terminal state.
    bool is_abandoned_ = false;
    // Indicates if this chunk should be retransmitted.
    bool should_be_retransmitted_ = false;

    // The number of times the DATA chunk has been nacked (by having received a
    // SACK which doesn't include it). Will be cleared on retransmissions.
    size_t nack_count_ = 0;
    // The number of times the DATA chunk has been retransmitted.
    size_t num_retransmissions_ = 0;
    // If the message was sent with a maximum number of retransmissions, this is
    // set to that number. The value zero (0) means that it will never be
    // retransmitted.
    const absl::optional<size_t> max_retransmissions_;
    // When the packet was sent, and placed in this queue.
    const TimeMs time_sent_;
    // If the message was sent with an expiration time, this is set.
    const absl::optional<TimeMs> expires_at_;
    // The actual data to send/retransmit.
    Data data_;
  };

  // Contains variables scoped to a processing of an incoming SACK.
  struct AckInfo {
    explicit AckInfo(UnwrappedTSN cumulative_tsn_ack)
        : highest_tsn_acked(cumulative_tsn_ack) {}

    // All TSNs that have been acked (for the first time) in this SACK.
    std::vector<TSN> acked_tsns;

    // Bytes acked by increasing cumulative_tsn_ack and gap_ack_blocks.
    size_t bytes_acked = 0;

    // Indicates if this SACK indicates that packet loss has occurred. Just
    // because a packet is missing in the SACK doesn't necessarily mean that
    // there is packet loss as that packet might be in-flight and received
    // out-of-order. But when it has been reported missing consecutive times, it
    // will eventually be considered "lost" and this will be set.
    bool has_packet_loss = false;

    // Highest TSN Newly Acknowledged, an SCTP variable.
    UnwrappedTSN highest_tsn_acked;
  };

  bool IsConsistent() const;

  // Returns how large a chunk will be, serialized, carrying the data
  size_t GetSerializedChunkSize(const Data& data) const;

  // Indicates if the congestion control algorithm is in "fast recovery".
  bool is_in_fast_recovery() const {
    return fast_recovery_exit_tsn_.has_value();
  }

  // Indicates if the congestion control algorithm is in "fast retransmit".
  bool is_in_fast_retransmit() const { return is_in_fast_retransmit_; }

  // Indicates if the provided SACK is valid given what has previously been
  // received. If it returns false, the SACK is most likely a duplicate of
  // something already seen, so this returning false doesn't necessarily mean
  // that the SACK is illegal.
  bool IsSackValid(const SackChunk& sack) const;

  // Given a `cumulative_tsn_ack` from an incoming SACK, will remove those items
  // in the retransmission queue up until this value and will update `ack_info`
  // by setting `bytes_acked_by_cumulative_tsn_ack` and `acked_tsns`.
  void RemoveAcked(UnwrappedTSN cumulative_tsn_ack, AckInfo& ack_info);

  // Helper method to nack an item and perform the correct operations given the
  // action indicated when nacking an item (e.g. retransmitting or abandoning).
  // The return value indicate if an action was performed, meaning that packet
  // loss was detected and acted upon.
  bool NackItem(UnwrappedTSN tsn, TxData& item, bool retransmit_now);

  // Will mark the chunks covered by the `gap_ack_blocks` from an incoming SACK
  // as "acked" and update `ack_info` by adding new TSNs to `added_tsns`.
  void AckGapBlocks(UnwrappedTSN cumulative_tsn_ack,
                    rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks,
                    AckInfo& ack_info);

  // Acks the chunk referenced by `iter` and updates state in `ack_info` and the
  // object's state.
  void AckChunk(AckInfo& ack_info,
                std::map<UnwrappedTSN, TxData>::iterator iter);

  // Mark chunks reported as "missing", as "nacked" or "to be retransmitted"
  // depending how many times this has happened. Only packets up until
  // `ack_info.highest_tsn_acked` (highest TSN newly acknowledged) are
  // nacked/retransmitted. The method will set `ack_info.has_packet_loss`.
  void NackBetweenAckBlocks(
      UnwrappedTSN cumulative_tsn_ack,
      rtc::ArrayView<const SackChunk::GapAckBlock> gap_ack_blocks,
      AckInfo& ack_info);

  // When a SACK chunk is received, this method will be called which _may_ call
  // into the `RetransmissionTimeout` to update the RTO.
  void UpdateRTT(TimeMs now, UnwrappedTSN cumulative_tsn_ack);

  // If the congestion control is in "fast recovery mode", this may be exited
  // now.
  void MaybeExitFastRecovery(UnwrappedTSN cumulative_tsn_ack);

  // If chunks have been ACKed, stop the retransmission timer.
  void StopT3RtxTimerOnIncreasedCumulativeTsnAck(
      UnwrappedTSN cumulative_tsn_ack);

  // Update the congestion control algorithm given as the cumulative ack TSN
  // value has increased, as reported in an incoming SACK chunk.
  void HandleIncreasedCumulativeTsnAck(size_t outstanding_bytes,
                                       size_t total_bytes_acked);
  // Update the congestion control algorithm, given as packet loss has been
  // detected, as reported in an incoming SACK chunk.
  void HandlePacketLoss(UnwrappedTSN highest_tsn_acked);
  // Update the view of the receiver window size.
  void UpdateReceiverWindow(uint32_t a_rwnd);
  // Given `max_size` of space left in a packet, which chunks can be added to
  // it?
  std::vector<std::pair<TSN, Data>> GetChunksToBeRetransmitted(size_t max_size);
  // If there is data sent and not ACKED, ensure that the retransmission timer
  // is running.
  void StartT3RtxTimerIfOutstandingData();

  // Given the current time `now_ms`, expire and abandon outstanding (sent at
  // least once) chunks that have a limited lifetime.
  void ExpireOutstandingChunks(TimeMs now);
  // Given that a message fragment, `item` has been abandoned, abandon all other
  // fragments that share the same message - both never-before-sent fragments
  // that are still in the SendQueue and outstanding chunks.
  void AbandonAllFor(const RetransmissionQueue::TxData& item);

  // Returns the current congestion control algorithm phase.
  CongestionAlgorithmPhase phase() const {
    return (cwnd_ <= ssthresh_)
               ? CongestionAlgorithmPhase::kSlowStart
               : CongestionAlgorithmPhase::kCongestionAvoidance;
  }

  // Returns the number of bytes that may be sent in a single packet according
  // to the congestion control algorithm.
  size_t max_bytes_to_send() const;

  const DcSctpOptions options_;
  // The minimum bytes required to be available in the congestion window to
  // allow packets to be sent - to avoid sending too small packets.
  const size_t min_bytes_required_to_send_;
  // If the peer supports RFC3758 - SCTP Partial Reliability Extension.
  const bool partial_reliability_;
  const std::string log_prefix_;
  // The size of the data chunk (DATA/I-DATA) header that is used.
  const size_t data_chunk_header_size_;
  // Called when a new RTT measurement has been done
  const std::function<void(DurationMs rtt)> on_new_rtt_;
  // Called when a SACK has been seen that cleared the retransmission counter.
  const std::function<void()> on_clear_retransmission_counter_;
  // The retransmission counter.
  Timer& t3_rtx_;
  // Unwraps TSNs
  UnwrappedTSN::Unwrapper tsn_unwrapper_;

  // Congestion Window. Number of bytes that may be in-flight (sent, not acked).
  size_t cwnd_;
  // Receive Window. Number of bytes available in the receiver's RX buffer.
  size_t rwnd_;
  // Slow Start Threshold. See RFC4960.
  size_t ssthresh_;
  // Partial Bytes Acked. See RFC4960.
  size_t partial_bytes_acked_;
  // If set, fast recovery is enabled until this TSN has been cumulative
  // acked.
  absl::optional<UnwrappedTSN> fast_recovery_exit_tsn_ = absl::nullopt;
  // Indicates if the congestion algorithm is in fast retransmit.
  bool is_in_fast_retransmit_ = false;

  // Next TSN to used.
  UnwrappedTSN next_tsn_;
  // The last cumulative TSN ack number
  UnwrappedTSN last_cumulative_tsn_ack_;
  // The send queue.
  SendQueue& send_queue_;
  // All the outstanding data chunks that are in-flight and that have not been
  // cumulative acked. Note that it also contains chunks that have been acked in
  // gap ack blocks.
  std::map<UnwrappedTSN, TxData> outstanding_data_;
  // Data chunks that are to be retransmitted.
  std::set<UnwrappedTSN> to_be_retransmitted_;
  // The number of bytes that are in-flight (sent but not yet acked or nacked).
  size_t outstanding_bytes_ = 0;
  // The number of DATA chunks that are in-flight (sent but not yet acked or
  // nacked).
  size_t outstanding_items_ = 0;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_
