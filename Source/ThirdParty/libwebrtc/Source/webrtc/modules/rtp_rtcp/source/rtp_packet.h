/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H_

#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {
struct RTPHeader;
class RtpHeaderExtensionMap;
class Random;

namespace rtp {
class Packet {
 public:
  using ExtensionType = RTPExtensionType;
  using ExtensionManager = RtpHeaderExtensionMap;
  static constexpr size_t kMaxExtensionHeaders = 14;
  static constexpr int kMinExtensionId = 1;
  static constexpr int kMaxExtensionId = 14;

  // Parse and copy given buffer into Packet.
  bool Parse(const uint8_t* buffer, size_t size);
  bool Parse(rtc::ArrayView<const uint8_t> packet);

  // Parse and move given buffer into Packet.
  bool Parse(rtc::CopyOnWriteBuffer packet);

  // Maps extensions id to their types.
  void IdentifyExtensions(const ExtensionManager& extensions);

  // Header.
  bool Marker() const;
  uint8_t PayloadType() const;
  uint16_t SequenceNumber() const;
  uint32_t Timestamp() const;
  uint32_t Ssrc() const;
  std::vector<uint32_t> Csrcs() const;

  // TODO(danilchap): Remove this function when all code update to use RtpPacket
  // directly. Function is there just for easier backward compatibilty.
  void GetHeader(RTPHeader* header) const;

  size_t headers_size() const;

  // Payload.
  size_t payload_size() const;
  size_t padding_size() const;
  rtc::ArrayView<const uint8_t> payload() const;

  // Buffer.
  rtc::CopyOnWriteBuffer Buffer() const;
  size_t capacity() const;
  size_t size() const;
  const uint8_t* data() const;
  size_t FreeCapacity() const;
  size_t MaxPayloadSize() const;

  // Reset fields and buffer.
  void Clear();

  // Header setters.
  void CopyHeaderFrom(const Packet& packet);
  void SetMarker(bool marker_bit);
  void SetPayloadType(uint8_t payload_type);
  void SetSequenceNumber(uint16_t seq_no);
  void SetTimestamp(uint32_t timestamp);
  void SetSsrc(uint32_t ssrc);

  // Writes csrc list. Assumes:
  // a) There is enough room left in buffer.
  // b) Extension headers, payload or padding data has not already been added.
  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  // Header extensions.
  template <typename Extension>
  bool HasExtension() const;

  template <typename Extension, typename... Values>
  bool GetExtension(Values...) const;

  template <typename Extension, typename... Values>
  bool SetExtension(Values...);

  template <typename Extension>
  bool ReserveExtension();

  // Following 4 helpers identify rtp header extension by |id| negotiated with
  // remote peer and written in an rtp packet.
  bool HasRawExtension(int id) const;

  // Returns place where extension with |id| is stored.
  // Returns empty arrayview if extension is not present.
  rtc::ArrayView<const uint8_t> GetRawExtension(int id) const;

  // Allocates and store header extension. Returns true on success.
  bool SetRawExtension(int id, rtc::ArrayView<const uint8_t> data);

  // Allocates and returns place to store rtp header extension.
  // Returns empty arrayview on failure.
  rtc::ArrayView<uint8_t> AllocateRawExtension(int id, size_t length);

  // Reserve size_bytes for payload. Returns nullptr on failure.
  uint8_t* SetPayloadSize(size_t size_bytes);
  // Same as SetPayloadSize but doesn't guarantee to keep current payload.
  uint8_t* AllocatePayload(size_t size_bytes);
  bool SetPadding(uint8_t size_bytes, Random* random);

 protected:
  // |extensions| required for SetExtension/ReserveExtension functions during
  // packet creating and used if available in Parse function.
  // Adding and getting extensions will fail until |extensions| is
  // provided via constructor or IdentifyExtensions function.
  Packet();
  explicit Packet(const ExtensionManager* extensions);
  Packet(const Packet&);
  Packet(const ExtensionManager* extensions, size_t capacity);
  virtual ~Packet();

  Packet& operator=(const Packet&) = default;

 private:
  struct ExtensionInfo {
    ExtensionType type;
    uint16_t offset;
    uint8_t length;
  };

  // Helper function for Parse. Fill header fields using data in given buffer,
  // but does not touch packet own buffer, leaving packet in invalid state.
  bool ParseBuffer(const uint8_t* buffer, size_t size);

  // Find an extension |type|.
  // Returns view of the raw extension or empty view on failure.
  rtc::ArrayView<const uint8_t> FindExtension(ExtensionType type) const;

  // Find or allocate an extension |type|. Returns view of size |length|
  // to write raw extension to or an empty view on failure.
  rtc::ArrayView<uint8_t> AllocateExtension(ExtensionType type, size_t length);

  uint8_t* WriteAt(size_t offset);
  void WriteAt(size_t offset, uint8_t byte);

  // Header.
  bool marker_;
  uint8_t payload_type_;
  uint8_t padding_size_;
  uint16_t sequence_number_;
  uint32_t timestamp_;
  uint32_t ssrc_;
  size_t payload_offset_;  // Match header size with csrcs and extensions.
  size_t payload_size_;

  ExtensionInfo extension_entries_[kMaxExtensionHeaders];
  uint16_t extensions_size_ = 0;  // Unaligned.
  rtc::CopyOnWriteBuffer buffer_;
};

template <typename Extension>
bool Packet::HasExtension() const {
  return !FindExtension(Extension::kId).empty();
}

template <typename Extension, typename... Values>
bool Packet::GetExtension(Values... values) const {
  auto raw = FindExtension(Extension::kId);
  if (raw.empty())
    return false;
  return Extension::Parse(raw, values...);
}

template <typename Extension, typename... Values>
bool Packet::SetExtension(Values... values) {
  const size_t value_size = Extension::ValueSize(values...);
  if (value_size == 0 || value_size > 16)
    return false;
  auto buffer = AllocateExtension(Extension::kId, value_size);
  if (buffer.empty())
    return false;
  return Extension::Write(buffer.data(), values...);
}

template <typename Extension>
bool Packet::ReserveExtension() {
  auto buffer = AllocateExtension(Extension::kId, Extension::kValueSizeBytes);
  if (buffer.empty())
    return false;
  memset(buffer.data(), 0, Extension::kValueSizeBytes);
  return true;
}
}  // namespace rtp
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H_
