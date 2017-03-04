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

  // Reserve size_bytes for payload. Returns nullptr on failure.
  uint8_t* AllocatePayload(size_t size_bytes);
  void SetPayloadSize(size_t size_bytes);
  bool SetPadding(uint8_t size_bytes, Random* random);

 protected:
  // |extensions| required for SetExtension/ReserveExtension functions during
  // packet creating and used if available in Parse function.
  // Adding and getting extensions will fail until |extensions| is
  // provided via constructor or IdentifyExtensions function.
  Packet();
  explicit Packet(const ExtensionManager* extensions);
  Packet(const Packet&) = default;
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

  // Find an extension based on the type field of the parameter.
  // If found, length field would be validated, the offset field will be set
  // and true returned,
  // otherwise the parameter will be unchanged and false is returned.
  bool FindExtension(ExtensionType type,
                     uint8_t length,
                     uint16_t* offset) const;

  // Find or allocate an extension, based on the type field of the parameter.
  // If found, the length field be checked against what is already registered
  // and the offset field will be set, then true is returned. If allocated, the
  // length field will be used for allocation and the offset update to indicate
  // position, the true is returned.
  // If not found and allocations fails, false is returned and parameter remains
  // unchanged.
  bool AllocateExtension(ExtensionType type, uint8_t length, uint16_t* offset);

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
  uint16_t offset = 0;
  return FindExtension(Extension::kId, Extension::kValueSizeBytes, &offset);
}

template <typename Extension, typename... Values>
bool Packet::GetExtension(Values... values) const {
  uint16_t offset = 0;
  if (!FindExtension(Extension::kId, Extension::kValueSizeBytes, &offset))
    return false;
  return Extension::Parse(data() + offset, values...);
}

template <typename Extension, typename... Values>
bool Packet::SetExtension(Values... values) {
  uint16_t offset = 0;
  if (!AllocateExtension(Extension::kId, Extension::kValueSizeBytes, &offset))
    return false;
  return Extension::Write(WriteAt(offset), values...);
}

template <typename Extension>
bool Packet::ReserveExtension() {
  uint16_t offset = 0;
  if (!AllocateExtension(Extension::kId, Extension::kValueSizeBytes, &offset))
    return false;
  memset(WriteAt(offset), 0, Extension::kValueSizeBytes);
  return true;
}
}  // namespace rtp
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H_
