/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_utils.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/crc32.h"

namespace {
// https://datatracker.ietf.org/doc/html/rfc5246#appendix-A.1
const uint8_t kDtlsChangeCipherSpecRecord = 20;
const uint8_t kDtlsHandshakeRecord = 22;

}  // namespace

namespace webrtc {

bool IsDtlsPacket(ArrayView<const uint8_t> payload) {
  const uint8_t* u = payload.data();
  return (payload.size() >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}

bool IsDtlsClientHelloPacket(ArrayView<const uint8_t> payload) {
  if (!IsDtlsPacket(payload)) {
    return false;
  }
  const uint8_t* u = payload.data();
  return payload.size() > 17 && u[0] == kDtlsHandshakeRecord && u[13] == 1;
}

bool IsDtlsHandshakePacket(ArrayView<const uint8_t> payload) {
  if (!IsDtlsPacket(payload)) {
    return false;
  }
  // change cipher spec is not a handshake packet. This used
  // to work because it was aggregated with the session ticket
  // which is no more. It is followed by the encrypted handshake
  // message which starts with a handshake record (22) again.
  return payload.size() > 17 && (payload[0] == kDtlsHandshakeRecord ||
                                 payload[0] == kDtlsChangeCipherSpecRecord);
}

uint32_t ComputeDtlsPacketHash(ArrayView<const uint8_t> dtls_packet) {
  return ComputeCrc32(dtls_packet.data(), dtls_packet.size());
}

bool PacketStash::AddIfUnique(ArrayView<const uint8_t> packet) {
  uint32_t h = ComputeDtlsPacketHash(packet);
  for (const auto& [hash, p] : packets_) {
    if (h == hash) {
      return false;
    }
  }
  packets_.push_back(
      {.hash = h,
       .buffer = std::make_unique<Buffer>(packet.data(), packet.size())});
  return true;
}

void PacketStash::Add(ArrayView<const uint8_t> packet) {
  packets_.push_back(
      {.hash = ComputeDtlsPacketHash(packet),
       .buffer = std::make_unique<Buffer>(packet.data(), packet.size())});
}

void PacketStash::Prune(const absl::flat_hash_set<uint32_t>& hashes) {
  if (hashes.empty()) {
    return;
  }
  uint32_t before = packets_.size();
  std::erase_if(packets_,
                [&](const auto& val) { return hashes.contains(val.hash); });
  uint32_t after = packets_.size();
  uint32_t removed = before - after;
  if (pos_ >= removed) {
    pos_ -= removed;
  }
  if (pos_ >= packets_.size()) {
    pos_ = packets_.empty() ? 0 : packets_.size() - 1;
  }
}

void PacketStash::Prune(uint32_t max_size) {
  auto size = packets_.size();
  if (size <= max_size) {
    return;
  }
  auto removed = size - max_size;
  packets_.erase(packets_.begin(), packets_.begin() + removed);
  if (pos_ <= removed) {
    pos_ = 0;
  } else {
    pos_ -= removed;
  }
}

ArrayView<const uint8_t> PacketStash::GetNext() {
  RTC_DCHECK(!packets_.empty());
  auto pos = pos_;
  pos_ = (pos + 1) % packets_.size();
  const auto& buffer = packets_[pos].buffer;
  return ArrayView<const uint8_t>(buffer->data(), buffer->size());
}

std::vector<ArrayView<const uint8_t>> PacketStash::GetAll() const {
  std::vector<ArrayView<const uint8_t>> ret;
  ret.reserve(packets_.size());
  for (const auto& buffer : packets_) {
    const uint8_t* ptr = buffer.buffer->data();
    ret.push_back(ArrayView<const uint8_t>(ptr, buffer.buffer->size()));
  }
  return ret;
}

}  // namespace webrtc
