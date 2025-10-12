/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_STUN_PIGGYBACK_CALLBACKS_H_
#define P2P_DTLS_DTLS_STUN_PIGGYBACK_CALLBACKS_H_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/transport/stun.h"
#include "rtc_base/checks.h"

namespace webrtc {

class DtlsStunPiggybackCallbacks {
 public:
  DtlsStunPiggybackCallbacks() : send_data_(nullptr), recv_data_(nullptr) {}

  DtlsStunPiggybackCallbacks(
      // Function invoked when sending a `request-type` (e.g.
      // STUN_BINDING_REQUEST). Returns a pair of data that will be sent:
      // - an optional DTLS_IN_STUN attribute
      // - an optional std::vector<uint32_t> for the DTLS_IN_STUN_ACK attribute
      absl::AnyInvocable<std::pair<std::optional<absl::string_view>,
                                   std::optional<std::vector<uint32_t>>>(
          /* request-type */ StunMessageType)>&& send_data,

      // Function invoked when receiving a STUN_BINDING { REQUEST / RESPONSE }
      // contains the optional ArrayView of the DTLS_IN_STUN attribute and the
      // optional uint32_t vector DTLS_IN_STUN_ACK attribute.
      absl::AnyInvocable<void(
          std::optional<ArrayView<uint8_t>> /* recv_data */,
          std::optional<std::vector<uint32_t>> /* recv_acks */)>&& recv_data)
      : send_data_(std::move(send_data)), recv_data_(std::move(recv_data)) {
    RTC_DCHECK(
        // either all set
        (send_data_ != nullptr && recv_data_ != nullptr) ||
        // or all nullptr
        (send_data_ == nullptr && recv_data_ == nullptr));
  }

  std::pair<std::optional<absl::string_view>,
            std::optional<std::vector<uint32_t>>>
  send_data(StunMessageType request_type) {
    RTC_DCHECK(send_data_);
    return send_data_(request_type);
  }

  void recv_data(std::optional<ArrayView<uint8_t>> data,
                 std::optional<std::vector<uint32_t>> acks) {
    RTC_DCHECK(recv_data_);
    return recv_data_(data, acks);
  }

  bool empty() const { return send_data_ == nullptr; }
  void reset() {
    send_data_ = nullptr;
    recv_data_ = nullptr;
  }

 private:
  absl::AnyInvocable<std::pair<std::optional<absl::string_view>,
                               std::optional<std::vector<uint32_t>>>(
      /* request-type */ StunMessageType)>
      send_data_;
  absl::AnyInvocable<void(std::optional<ArrayView<uint8_t>> /* recv_data */,
                          std::optional<std::vector<uint32_t>> /* recv_acks */)>
      recv_data_;
};

}  //  namespace webrtc


#endif  // P2P_DTLS_DTLS_STUN_PIGGYBACK_CALLBACKS_H_
