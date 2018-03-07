/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet.h"

#include <cstring>
#include <utility>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kOneByteExtensionId = 0xBEDE;
constexpr size_t kOneByteHeaderSize = 1;
constexpr size_t kDefaultPacketSize = 1500;
}  // namespace

constexpr int RtpPacket::kMaxExtensionHeaders;
constexpr int RtpPacket::kMinExtensionId;
constexpr int RtpPacket::kMaxExtensionId;

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            Contributing source (CSRC) identifiers             |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |One-byte eXtensions id = 0xbede|       length in 32bits        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          Extensions                           |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                           Payload                             |
// |             ....              :  padding...                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               padding         | Padding size  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
RtpPacket::RtpPacket() : RtpPacket(nullptr, kDefaultPacketSize) {}

RtpPacket::RtpPacket(const ExtensionManager* extensions)
    : RtpPacket(extensions, kDefaultPacketSize) {}

RtpPacket::RtpPacket(const RtpPacket&) = default;

RtpPacket::RtpPacket(const ExtensionManager* extensions, size_t capacity)
    : buffer_(capacity) {
  RTC_DCHECK_GE(capacity, kFixedHeaderSize);
  Clear();
  if (extensions) {
    IdentifyExtensions(*extensions);
  } else {
    for (size_t i = 0; i < kMaxExtensionHeaders; ++i)
      extension_entries_[i].type = ExtensionManager::kInvalidType;
  }
}

RtpPacket::~RtpPacket() {}

void RtpPacket::IdentifyExtensions(const ExtensionManager& extensions) {
  for (int i = 0; i < kMaxExtensionHeaders; ++i)
    extension_entries_[i].type = extensions.GetType(i + 1);
}

bool RtpPacket::Parse(const uint8_t* buffer, size_t buffer_size) {
  if (!ParseBuffer(buffer, buffer_size)) {
    Clear();
    return false;
  }
  buffer_.SetData(buffer, buffer_size);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool RtpPacket::Parse(rtc::ArrayView<const uint8_t> packet) {
  return Parse(packet.data(), packet.size());
}

bool RtpPacket::Parse(rtc::CopyOnWriteBuffer buffer) {
  if (!ParseBuffer(buffer.cdata(), buffer.size())) {
    Clear();
    return false;
  }
  size_t buffer_size = buffer.size();
  buffer_ = std::move(buffer);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool RtpPacket::Marker() const {
  RTC_DCHECK_EQ(marker_, (data()[1] & 0x80) != 0);
  return marker_;
}

uint8_t RtpPacket::PayloadType() const {
  RTC_DCHECK_EQ(payload_type_, data()[1] & 0x7f);
  return payload_type_;
}

uint16_t RtpPacket::SequenceNumber() const {
  RTC_DCHECK_EQ(sequence_number_,
                ByteReader<uint16_t>::ReadBigEndian(data() + 2));
  return sequence_number_;
}

uint32_t RtpPacket::Timestamp() const {
  RTC_DCHECK_EQ(timestamp_, ByteReader<uint32_t>::ReadBigEndian(data() + 4));
  return timestamp_;
}

uint32_t RtpPacket::Ssrc() const {
  RTC_DCHECK_EQ(ssrc_, ByteReader<uint32_t>::ReadBigEndian(data() + 8));
  return ssrc_;
}

std::vector<uint32_t> RtpPacket::Csrcs() const {
  size_t num_csrc = data()[0] & 0x0F;
  RTC_DCHECK_GE(capacity(), kFixedHeaderSize + num_csrc * 4);
  std::vector<uint32_t> csrcs(num_csrc);
  for (size_t i = 0; i < num_csrc; ++i) {
    csrcs[i] =
        ByteReader<uint32_t>::ReadBigEndian(&data()[kFixedHeaderSize + i * 4]);
  }
  return csrcs;
}

size_t RtpPacket::headers_size() const {
  return payload_offset_;
}

size_t RtpPacket::payload_size() const {
  return payload_size_;
}

size_t RtpPacket::padding_size() const {
  return padding_size_;
}

rtc::ArrayView<const uint8_t> RtpPacket::payload() const {
  return rtc::MakeArrayView(data() + payload_offset_, payload_size_);
}

rtc::CopyOnWriteBuffer RtpPacket::Buffer() const {
  return buffer_;
}

size_t RtpPacket::capacity() const {
  return buffer_.capacity();
}

size_t RtpPacket::size() const {
  size_t ret = payload_offset_ + payload_size_ + padding_size_;
  RTC_DCHECK_EQ(buffer_.size(), ret);
  return ret;
}

const uint8_t* RtpPacket::data() const {
  return buffer_.cdata();
}

size_t RtpPacket::FreeCapacity() const {
  return capacity() - size();
}

size_t RtpPacket::MaxPayloadSize() const {
  return capacity() - payload_offset_;
}

void RtpPacket::CopyHeaderFrom(const RtpPacket& packet) {
  RTC_DCHECK_GE(capacity(), packet.headers_size());

  marker_ = packet.marker_;
  payload_type_ = packet.payload_type_;
  sequence_number_ = packet.sequence_number_;
  timestamp_ = packet.timestamp_;
  ssrc_ = packet.ssrc_;
  payload_offset_ = packet.payload_offset_;
  for (size_t i = 0; i < kMaxExtensionHeaders; ++i) {
    extension_entries_[i] = packet.extension_entries_[i];
  }
  extensions_size_ = packet.extensions_size_;
  buffer_.SetData(packet.data(), packet.headers_size());
  // Reset payload and padding.
  payload_size_ = 0;
  padding_size_ = 0;
}

void RtpPacket::SetMarker(bool marker_bit) {
  marker_ = marker_bit;
  if (marker_) {
    WriteAt(1, data()[1] | 0x80);
  } else {
    WriteAt(1, data()[1] & 0x7F);
  }
}

void RtpPacket::SetPayloadType(uint8_t payload_type) {
  RTC_DCHECK_LE(payload_type, 0x7Fu);
  payload_type_ = payload_type;
  WriteAt(1, (data()[1] & 0x80) | payload_type);
}

void RtpPacket::SetSequenceNumber(uint16_t seq_no) {
  sequence_number_ = seq_no;
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(2), seq_no);
}

void RtpPacket::SetTimestamp(uint32_t timestamp) {
  timestamp_ = timestamp;
  ByteWriter<uint32_t>::WriteBigEndian(WriteAt(4), timestamp);
}

void RtpPacket::SetSsrc(uint32_t ssrc) {
  ssrc_ = ssrc;
  ByteWriter<uint32_t>::WriteBigEndian(WriteAt(8), ssrc);
}

void RtpPacket::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  RTC_DCHECK_EQ(extensions_size_, 0);
  RTC_DCHECK_EQ(payload_size_, 0);
  RTC_DCHECK_EQ(padding_size_, 0);
  RTC_DCHECK_LE(csrcs.size(), 0x0fu);
  RTC_DCHECK_LE(kFixedHeaderSize + 4 * csrcs.size(), capacity());
  payload_offset_ = kFixedHeaderSize + 4 * csrcs.size();
  WriteAt(0, (data()[0] & 0xF0) | rtc::dchecked_cast<uint8_t>(csrcs.size()));
  size_t offset = kFixedHeaderSize;
  for (uint32_t csrc : csrcs) {
    ByteWriter<uint32_t>::WriteBigEndian(WriteAt(offset), csrc);
    offset += 4;
  }
  buffer_.SetSize(payload_offset_);
}

bool RtpPacket::HasRawExtension(int id) const {
  if (id == ExtensionManager::kInvalidId)
    return false;
  RTC_DCHECK_GE(id, kMinExtensionId);
  RTC_DCHECK_LE(id, kMaxExtensionId);
  return extension_entries_[id - 1].offset != 0;
}

rtc::ArrayView<const uint8_t> RtpPacket::GetRawExtension(int id) const {
  if (id == ExtensionManager::kInvalidId)
    return nullptr;
  RTC_DCHECK_GE(id, kMinExtensionId);
  RTC_DCHECK_LE(id, kMaxExtensionId);
  const ExtensionInfo& extension = extension_entries_[id - 1];
  if (extension.offset == 0)
    return nullptr;
  return rtc::MakeArrayView(data() + extension.offset, extension.length);
}

bool RtpPacket::SetRawExtension(int id, rtc::ArrayView<const uint8_t> data) {
  auto buffer = AllocateRawExtension(id, data.size());
  if (buffer.empty())
    return false;
  RTC_DCHECK_EQ(buffer.size(), data.size());
  memcpy(buffer.data(), data.data(), data.size());
  return true;
}

rtc::ArrayView<uint8_t> RtpPacket::AllocateRawExtension(int id, size_t length) {
  if (id == ExtensionManager::kInvalidId)
    return nullptr;
  RTC_DCHECK_GE(id, kMinExtensionId);
  RTC_DCHECK_LE(id, kMaxExtensionId);
  RTC_DCHECK_GE(length, 1);
  RTC_DCHECK_LE(length, 16);

  ExtensionInfo* extension_entry = &extension_entries_[id - 1];
  if (extension_entry->offset != 0) {
    // Extension already reserved. Check if same length is used.
    if (extension_entry->length == length)
      return rtc::MakeArrayView(WriteAt(extension_entry->offset), length);

    RTC_LOG(LS_ERROR) << "Length mismatch for extension id " << id << " type "
                      << static_cast<int>(extension_entry->type)
                      << ": expected "
                      << static_cast<int>(extension_entry->length)
                      << ". received " << length;
    return nullptr;
  }
  if (payload_size_ > 0) {
    RTC_LOG(LS_ERROR) << "Can't add new extension id " << id
                      << " after payload was set.";
    return nullptr;
  }
  if (padding_size_ > 0) {
    RTC_LOG(LS_ERROR) << "Can't add new extension id " << id
                      << " after padding was set.";
    return nullptr;
  }

  size_t num_csrc = data()[0] & 0x0F;
  size_t extensions_offset = kFixedHeaderSize + (num_csrc * 4) + 4;
  size_t new_extensions_size = extensions_size_ + kOneByteHeaderSize + length;
  if (extensions_offset + new_extensions_size > capacity()) {
    RTC_LOG(LS_ERROR)
        << "Extension cannot be registered: Not enough space left in buffer.";
    return nullptr;
  }

  // All checks passed, write down the extension headers.
  if (extensions_size_ == 0) {
    RTC_DCHECK_EQ(payload_offset_, kFixedHeaderSize + (num_csrc * 4));
    WriteAt(0, data()[0] | 0x10);  // Set extension bit.
    // Profile specific ID always set to OneByteExtensionHeader.
    ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 4),
                                         kOneByteExtensionId);
  }

  uint8_t one_byte_header = rtc::dchecked_cast<uint8_t>(id) << 4;
  one_byte_header |= rtc::dchecked_cast<uint8_t>(length - 1);
  WriteAt(extensions_offset + extensions_size_, one_byte_header);

  extension_entry->offset = rtc::dchecked_cast<uint16_t>(
      extensions_offset + extensions_size_ + kOneByteHeaderSize);
  extension_entry->length = rtc::dchecked_cast<uint8_t>(length);
  extensions_size_ = rtc::dchecked_cast<uint16_t>(new_extensions_size);

  // Update header length field.
  uint16_t extensions_words = (extensions_size_ + 3) / 4;  // Wrap up to 32bit.
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 2),
                                       extensions_words);
  // Fill extension padding place with zeroes.
  size_t extension_padding_size = 4 * extensions_words - extensions_size_;
  memset(WriteAt(extensions_offset + extensions_size_), 0,
         extension_padding_size);
  payload_offset_ = extensions_offset + 4 * extensions_words;
  buffer_.SetSize(payload_offset_);
  return rtc::MakeArrayView(WriteAt(extension_entry->offset), length);
}

uint8_t* RtpPacket::AllocatePayload(size_t size_bytes) {
  // Reset payload size to 0. If CopyOnWrite buffer_ was shared, this will cause
  // reallocation and memcpy. Keeping just header reduces memcpy size.
  SetPayloadSize(0);
  return SetPayloadSize(size_bytes);
}

uint8_t* RtpPacket::SetPayloadSize(size_t size_bytes) {
  RTC_DCHECK_EQ(padding_size_, 0);
  if (payload_offset_ + size_bytes > capacity()) {
    RTC_LOG(LS_WARNING) << "Cannot set payload, not enough space in buffer.";
    return nullptr;
  }
  payload_size_ = size_bytes;
  buffer_.SetSize(payload_offset_ + payload_size_);
  return WriteAt(payload_offset_);
}

bool RtpPacket::SetPadding(uint8_t size_bytes, Random* random) {
  RTC_DCHECK(random);
  if (payload_offset_ + payload_size_ + size_bytes > capacity()) {
    RTC_LOG(LS_WARNING) << "Cannot set padding size " << size_bytes << ", only "
                        << (capacity() - payload_offset_ - payload_size_)
                        << " bytes left in buffer.";
    return false;
  }
  padding_size_ = size_bytes;
  buffer_.SetSize(payload_offset_ + payload_size_ + padding_size_);
  if (padding_size_ > 0) {
    size_t padding_offset = payload_offset_ + payload_size_;
    size_t padding_end = padding_offset + padding_size_;
    for (size_t offset = padding_offset; offset < padding_end - 1; ++offset) {
      WriteAt(offset, random->Rand<uint8_t>());
    }
    WriteAt(padding_end - 1, padding_size_);
    WriteAt(0, data()[0] | 0x20);  // Set padding bit.
  } else {
    WriteAt(0, data()[0] & ~0x20);  // Clear padding bit.
  }
  return true;
}

void RtpPacket::Clear() {
  marker_ = false;
  payload_type_ = 0;
  sequence_number_ = 0;
  timestamp_ = 0;
  ssrc_ = 0;
  payload_offset_ = kFixedHeaderSize;
  payload_size_ = 0;
  padding_size_ = 0;
  extensions_size_ = 0;
  for (ExtensionInfo& location : extension_entries_) {
    location.offset = 0;
    location.length = 0;
  }

  memset(WriteAt(0), 0, kFixedHeaderSize);
  buffer_.SetSize(kFixedHeaderSize);
  WriteAt(0, kRtpVersion << 6);
}

bool RtpPacket::ParseBuffer(const uint8_t* buffer, size_t size) {
  if (size < kFixedHeaderSize) {
    return false;
  }
  const uint8_t version = buffer[0] >> 6;
  if (version != kRtpVersion) {
    return false;
  }
  const bool has_padding = (buffer[0] & 0x20) != 0;
  const bool has_extension = (buffer[0] & 0x10) != 0;
  const uint8_t number_of_crcs = buffer[0] & 0x0f;
  marker_ = (buffer[1] & 0x80) != 0;
  payload_type_ = buffer[1] & 0x7f;

  sequence_number_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
  timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
  if (size < kFixedHeaderSize + number_of_crcs * 4) {
    return false;
  }
  payload_offset_ = kFixedHeaderSize + number_of_crcs * 4;

  if (has_padding) {
    padding_size_ = buffer[size - 1];
    if (padding_size_ == 0) {
      RTC_LOG(LS_WARNING) << "Padding was set, but padding size is zero";
      return false;
    }
  } else {
    padding_size_ = 0;
  }

  extensions_size_ = 0;
  for (ExtensionInfo& location : extension_entries_) {
    location.offset = 0;
    location.length = 0;
  }
  if (has_extension) {
    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */
    size_t extension_offset = payload_offset_ + 4;
    if (extension_offset > size) {
      return false;
    }
    uint16_t profile =
        ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_]);
    size_t extensions_capacity =
        ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_ + 2]);
    extensions_capacity *= 4;
    if (extension_offset + extensions_capacity > size) {
      return false;
    }
    if (profile != kOneByteExtensionId) {
      RTC_LOG(LS_WARNING) << "Unsupported rtp extension " << profile;
    } else {
      constexpr uint8_t kPaddingId = 0;
      constexpr uint8_t kReservedId = 15;
      while (extensions_size_ + kOneByteHeaderSize < extensions_capacity) {
        int id = buffer[extension_offset + extensions_size_] >> 4;
        if (id == kReservedId) {
          break;
        } else if (id == kPaddingId) {
          extensions_size_++;
          continue;
        }
        uint8_t length =
            1 + (buffer[extension_offset + extensions_size_] & 0xf);
        if (extensions_size_ + kOneByteHeaderSize + length >
            extensions_capacity) {
          RTC_LOG(LS_WARNING) << "Oversized rtp header extension.";
          break;
        }

        size_t idx = id - 1;
        if (extension_entries_[idx].length != 0) {
          RTC_LOG(LS_VERBOSE)
              << "Duplicate rtp header extension id " << id << ". Overwriting.";
        }

        extensions_size_ += kOneByteHeaderSize;
        extension_entries_[idx].offset =
            rtc::dchecked_cast<uint16_t>(extension_offset + extensions_size_);
        extension_entries_[idx].length = rtc::dchecked_cast<uint16_t>(length);
        extensions_size_ += length;
      }
    }
    payload_offset_ = extension_offset + extensions_capacity;
  }

  if (payload_offset_ + padding_size_ > size) {
    return false;
  }
  payload_size_ = size - payload_offset_ - padding_size_;
  return true;
}

rtc::ArrayView<const uint8_t> RtpPacket::FindExtension(
    ExtensionType type) const {
  for (const ExtensionInfo& extension : extension_entries_) {
    if (extension.type == type) {
      if (extension.length == 0) {
        // Extension is registered but not set.
        return nullptr;
      }
      return rtc::MakeArrayView(data() + extension.offset, extension.length);
    }
  }
  return nullptr;
}

rtc::ArrayView<uint8_t> RtpPacket::AllocateExtension(ExtensionType type,
                                                     size_t length) {
  for (int i = 0; i < kMaxExtensionHeaders; ++i) {
    if (extension_entries_[i].type == type) {
      int extension_id = i + 1;
      return AllocateRawExtension(extension_id, length);
    }
  }
  // Extension not registered.
  return nullptr;
}

uint8_t* RtpPacket::WriteAt(size_t offset) {
  return buffer_.data() + offset;
}

void RtpPacket::WriteAt(size_t offset, uint8_t byte) {
  buffer_.data()[offset] = byte;
}

}  // namespace webrtc
