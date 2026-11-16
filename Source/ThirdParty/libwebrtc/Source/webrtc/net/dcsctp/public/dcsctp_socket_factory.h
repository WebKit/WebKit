/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_FACTORY_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_FACTORY_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/packet_observer.h"

namespace dcsctp {
class DcSctpSocketFactory {
 public:
  virtual ~DcSctpSocketFactory();
  virtual std::unique_ptr<DcSctpSocketInterface> Create(
      absl::string_view log_prefix,
      DcSctpSocketCallbacks& callbacks,
      std::unique_ptr<PacketObserver> packet_observer,
      const DcSctpOptions& options);
  std::vector<uint8_t> GenerateConnectionToken(
      const DcSctpOptions& options,
      std::function<uint32_t(uint32_t low, uint32_t high)> get_random_uint32);
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_FACTORY_H_
