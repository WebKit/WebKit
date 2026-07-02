/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_UTILS_H_
#define P2P_DTLS_DTLS_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "rtc_base/buffer.h"

namespace webrtc {

const size_t kDtlsRecordHeaderLen = 13;
const size_t kMaxDtlsPacketLen = 2048;

bool IsDtlsPacket(std::span<const uint8_t> payload);
bool IsDtlsClientHelloPacket(std::span<const uint8_t> payload);
bool IsDtlsHandshakePacket(std::span<const uint8_t> payload);

uint32_t ComputeDtlsPacketHash(std::span<const uint8_t> dtls_packet);

class PacketStash {
 public:
  PacketStash() {}

  void Add(std::span<const uint8_t> packet);
  bool AddIfUnique(std::span<const uint8_t> packet);
  // Returns number of elements that were removed.
  size_t Prune(const absl::flat_hash_set<uint32_t>& packet_hashes);
  void Prune(uint32_t max_size);
  std::span<const uint8_t> GetNext();
  std::vector<std::span<const uint8_t>> GetAll() const;

  void clear() {
    packets_.clear();
    pos_ = 0;
  }
  bool empty() const { return packets_.empty(); }
  int size() const { return packets_.size(); }

  static uint32_t Hash(std::span<const uint8_t> packet) {
    return ComputeDtlsPacketHash(packet);
  }

 private:
  struct StashedPacket {
    uint32_t hash;
    std::unique_ptr<Buffer> buffer;
  };

  // This vector will only contain very few items,
  // so it is appropriate to use a vector rather than
  // e.g. a hash map.
  uint32_t pos_ = 0;
  std::vector<StashedPacket> packets_;
};

}  //  namespace webrtc


#endif  // P2P_DTLS_DTLS_UTILS_H_
