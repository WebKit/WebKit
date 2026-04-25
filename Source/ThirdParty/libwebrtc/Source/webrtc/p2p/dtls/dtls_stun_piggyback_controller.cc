/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_stun_piggyback_controller.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/str_join.h"

namespace webrtc {

DtlsStunPiggybackController::DtlsStunPiggybackController(
    absl::AnyInvocable<void(ArrayView<const uint8_t>)> dtls_data_callback)
    : dtls_data_callback_(std::move(dtls_data_callback)) {}

DtlsStunPiggybackController::~DtlsStunPiggybackController() {}

void DtlsStunPiggybackController::SetDtlsHandshakeComplete(bool is_dtls_client,
                                                           bool is_dtls13) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // As DTLS 1.2 server we need to keep the last flight around until
  // we receive the post-handshake acknowledgment.
  // As DTLS 1.2 client we have nothing more to send at this point
  // but will continue to send ACK attributes until receiving
  // the last flight from the server.
  // For DTLS 1.3 this is reversed since the handshake has one round trip less.
  if ((is_dtls_client && !is_dtls13) || (!is_dtls_client && is_dtls13)) {
    pending_packets_.clear();
  }

  // Peer does not support this so fallback to a normal DTLS handshake
  // happened.
  if (state_ == State::OFF) {
    return;
  }
  state_ = State::PENDING;
}

void DtlsStunPiggybackController::SetDtlsFailed() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == State::TENTATIVE || state_ == State::CONFIRMED ||
      state_ == State::PENDING) {
    RTC_LOG(LS_INFO)
        << "DTLS-STUN piggybacking DTLS failed during negotiation.";
  }
  state_ = State::OFF;
}

void DtlsStunPiggybackController::CapturePacket(ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!IsDtlsPacket(data)) {
    return;
  }

  // BoringSSL writes burst of packets...but the interface
  // is made for 1-packet at a time. Use the writing_packets_ variable to keep
  // track of a full flight. The writing_packets_ is reset in Flush.
  if (!writing_packets_) {
    pending_packets_.clear();
    writing_packets_ = true;
  }

  pending_packets_.Add(data);
}

void DtlsStunPiggybackController::ClearCachedPacketForTesting() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  pending_packets_.clear();
}

void DtlsStunPiggybackController::Flush() {
  // Flush is called by the StreamInterface (and the underlying SSL BIO)
  // after a flight of packets has been sent.
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  writing_packets_ = false;
}

std::optional<absl::string_view>
DtlsStunPiggybackController::GetDataToPiggyback(
    StunMessageType stun_message_type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(stun_message_type == STUN_BINDING_REQUEST ||
             stun_message_type == STUN_BINDING_RESPONSE);

  // No longer writing packets...since we're now about to send them.
  RTC_DCHECK(!writing_packets_);

  if (state_ == State::COMPLETE) {
    return std::nullopt;
  }

  if (state_ == State::OFF) {
    return std::nullopt;
  }

  if (pending_packets_.empty()) {
    return std::nullopt;
  }

  const auto packet = pending_packets_.GetNext();
  return absl::string_view(reinterpret_cast<const char*>(packet.data()),
                           packet.size());
}

std::optional<const std::vector<uint32_t>>
DtlsStunPiggybackController::GetAckToPiggyback(
    StunMessageType stun_message_type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  return handshake_messages_received_;
}

std::vector<ArrayView<const uint8_t>>
DtlsStunPiggybackController::GetPending() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return pending_packets_.GetAll();
}

void DtlsStunPiggybackController::ReportDataPiggybacked(
    std::optional<ArrayView<uint8_t>> data,
    std::optional<std::vector<uint32_t>> acks) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // Drop silently when receiving acked data when the peer previously did not
  // support or we already moved to the complete state.
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return;
  }

  // We sent dtls piggybacked but got nothing in return or
  // we received a stun request with neither attribute set
  // => peer does not support.
  if (state_ == State::TENTATIVE && !data.has_value() && !acks.has_value()) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking not supported by peer.";
    state_ = State::OFF;
    return;
  }

  // In PENDING state the peer may have stopped sending the ack
  // when it moved to the COMPLETE state. Move to the same state.
  if (state_ == State::PENDING && !data.has_value() && !acks.has_value()) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking complete.";
    state_ = State::COMPLETE;
    pending_packets_.clear();
    handshake_messages_received_.clear();
    return;
  }

  // We sent dtls piggybacked and got something in return => peer does support.
  if (state_ == State::TENTATIVE) {
    state_ = State::CONFIRMED;
  }

  if (acks.has_value()) {
    if (!pending_packets_.empty()) {
      // Unpack the ACK attribute (a list of uint32_t)
      absl::flat_hash_set<uint32_t> acked_packets;
      for (const auto& ack : *acks) {
        acked_packets.insert(ack);
      }
      RTC_LOG(LS_VERBOSE) << "DTLS-STUN piggybacking ACK: "
                          << StrJoin(acked_packets, ",");

      // Remove all acked packets from pending_packets_.
      pending_packets_.Prune(acked_packets);
    }
  }

  if (!data.has_value()) {
    // If we receive msg w/o any data, that means that the peer
    // is not retransmitting, so we don't need to ACK anything.
    handshake_messages_received_.clear();
  }

  // The response to the final flight of the handshake will not contain
  // the DTLS data but will contain an ack.
  // Must not happen on the initial server to client packet which
  // has no DTLS data yet.
  if (!data.has_value() && acks.has_value() && state_ == State::PENDING) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking complete.";
    state_ = State::COMPLETE;
    pending_packets_.clear();
    handshake_messages_received_.clear();
    return;
  }

  if (!data.has_value() || data->empty()) {
    return;
  }
  // Drop non-DTLS packets.
  if (!IsDtlsPacket(*data)) {
    RTC_LOG(LS_WARNING) << "Dropping non-DTLS data.";
    return;
  }
  data_recv_count_++;

  ReportDtlsPacket(*data);

  // Forwards the data to the DTLS layer. Note that this will call
  // ProcessDtlsPacket() again which does not change the state.
  dtls_data_callback_(*data);
}

void DtlsStunPiggybackController::ReportDtlsPacket(
    ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return;
  }

  // Extract the received message id of the handshake
  // from the packet and prepare the ack to be sent.
  uint32_t hash = ComputeDtlsPacketHash(data);

  // Check if we already received this packet.
  if (std::find(handshake_messages_received_.begin(),
                handshake_messages_received_.end(),
                hash) == handshake_messages_received_.end()) {
    // If needed, limit size of ack attribute by removing oldest ack.
    while (handshake_messages_received_.size() >= kMaxAckSize) {
      handshake_messages_received_.erase(handshake_messages_received_.begin());
    }
    handshake_messages_received_.push_back(hash);
  }
}

}  // namespace webrtc
