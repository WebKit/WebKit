/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
#define P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This class is not thread safe; all methods must be called on the same thread
// as the constructor.
class DtlsStunPiggybackController {
 public:
  // Never ack more than 4 packets.
  static constexpr unsigned kMaxAckSize = 4;

  // dtls_data_callback will be called with any DTLS packets received
  // piggybacked.
  DtlsStunPiggybackController(
      absl::AnyInvocable<void(ArrayView<const uint8_t>)> dtls_data_callback);
  ~DtlsStunPiggybackController();

  enum class State {
    // We don't know if peer support DTLS piggybacked in STUN.
    // We will piggyback DTLS until we get a piggybacked response
    // or a STUN response with piggyback support.
    TENTATIVE = 0,
    // The peer supports DTLS in STUN and we continue the handshake.
    CONFIRMED = 1,
    // We are waiting for the final ack. Semantic differs depending
    // on DTLS role.
    PENDING = 2,
    // We successfully completed the DTLS handshake in STUN.
    COMPLETE = 3,
    // The peer does not support piggybacking DTLS in STUN.
    OFF = 4,
  };

  State state() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return state_;
  }

  // Called by DtlsTransport when the handshake is complete.
  void SetDtlsHandshakeComplete(bool is_dtls_client, bool is_dtls13);
  // Called by DtlsTransport when DTLS failed.
  void SetDtlsFailed();

  // Intercepts DTLS packets which should go into the STUN packets during the
  // handshake.
  void CapturePacket(ArrayView<const uint8_t> data);
  void ClearCachedPacketForTesting();

  // Inform piggybackcontroller that a flight is complete.
  void Flush();

  // Called by Connection, when sending a STUN BINDING { REQUEST / RESPONSE }
  // to obtain optional DTLS data or ACKs.
  std::optional<absl::string_view> GetDataToPiggyback(
      StunMessageType stun_message_type);
  std::optional<const std::vector<uint32_t>> GetAckToPiggyback(
      StunMessageType stun_message_type);
  std::vector<ArrayView<const uint8_t>> GetPending();

  // Called by Connection when receiving a STUN BINDING { REQUEST / RESPONSE }.
  void ReportDataPiggybacked(std::optional<ArrayView<uint8_t>> data,
                             std::optional<std::vector<uint32_t>> acks);

  // Called by
  // * DTLSTransport when receiving a DTLS packet (possibly after the packet
  //   was emitted by this class).
  // * This class when processing a DTLS packet.
  void ReportDtlsPacket(ArrayView<const uint8_t> data);

  int GetCountOfReceivedData() const { return data_recv_count_; }

 private:
  State state_ RTC_GUARDED_BY(sequence_checker_) = State::TENTATIVE;
  bool writing_packets_ RTC_GUARDED_BY(sequence_checker_) = false;
  PacketStash pending_packets_ RTC_GUARDED_BY(sequence_checker_);
  absl::AnyInvocable<void(ArrayView<const uint8_t>)> dtls_data_callback_;
  absl::AnyInvocable<void()> disable_piggybacking_callback_;

  std::vector<uint32_t> handshake_messages_received_
      RTC_GUARDED_BY(sequence_checker_);

  // Count of embedded data attributes received.
  int data_recv_count_ = 0;

  // In practice this will be the network thread.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

}  //  namespace webrtc


#endif  // P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
