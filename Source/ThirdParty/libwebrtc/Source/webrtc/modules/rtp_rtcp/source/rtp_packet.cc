/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_packet.h"

#include <cstring>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/random.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {
namespace rtp {
namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kOneByteExtensionId = 0xBEDE;
constexpr size_t kOneByteHeaderSize = 1;
constexpr size_t kDefaultPacketSize = 1500;
}  // namespace
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
Packet::Packet() : Packet(nullptr, kDefaultPacketSize) {}

Packet::Packet(const ExtensionManager* extensions)
    : Packet(extensions, kDefaultPacketSize) {}

Packet::Packet(const ExtensionManager* extensions, size_t capacity)
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

Packet::~Packet() {}

void Packet::IdentifyExtensions(const ExtensionManager& extensions) {
  for (size_t i = 0; i < kMaxExtensionHeaders; ++i)
    extension_entries_[i].type = extensions.GetType(i + 1);
}

bool Packet::Parse(const uint8_t* buffer, size_t buffer_size) {
  if (!ParseBuffer(buffer, buffer_size)) {
    Clear();
    return false;
  }
  buffer_.SetData(buffer, buffer_size);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool Packet::Parse(rtc::ArrayView<const uint8_t> packet) {
  return Parse(packet.data(), packet.size());
}

bool Packet::Parse(rtc::CopyOnWriteBuffer buffer) {
  if (!ParseBuffer(buffer.cdata(), buffer.size())) {
    Clear();
    return false;
  }
  size_t buffer_size = buffer.size();
  buffer_ = std::move(buffer);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool Packet::Marker() const {
  RTC_DCHECK_EQ(marker_, (data()[1] & 0x80) != 0);
  return marker_;
}

uint8_t Packet::PayloadType() const {
  RTC_DCHECK_EQ(payload_type_, data()[1] & 0x7f);
  return payload_type_;
}

uint16_t Packet::SequenceNumber() const {
  RTC_DCHECK_EQ(sequence_number_,
                ByteReader<uint16_t>::ReadBigEndian(data() + 2));
  return sequence_number_;
}

uint32_t Packet::Timestamp() const {
  RTC_DCHECK_EQ(timestamp_, ByteReader<uint32_t>::ReadBigEndian(data() + 4));
  return timestamp_;
}

uint32_t Packet::Ssrc() const {
  RTC_DCHECK_EQ(ssrc_, ByteReader<uint32_t>::ReadBigEndian(data() + 8));
  return ssrc_;
}

std::vector<uint32_t> Packet::Csrcs() const {
  size_t num_csrc = data()[0] & 0x0F;
  RTC_DCHECK_GE(capacity(), kFixedHeaderSize + num_csrc * 4);
  std::vector<uint32_t> csrcs(num_csrc);
  for (size_t i = 0; i < num_csrc; ++i) {
    csrcs[i] =
        ByteReader<uint32_t>::ReadBigEndian(&data()[kFixedHeaderSize + i * 4]);
  }
  return csrcs;
}

void Packet::GetHeader(RTPHeader* header) const {
  header->markerBit = Marker();
  header->payloadType = PayloadType();
  header->sequenceNumber = SequenceNumber();
  header->timestamp = Timestamp();
  header->ssrc = Ssrc();
  std::vector<uint32_t> csrcs = Csrcs();
  header->numCSRCs = csrcs.size();
  for (size_t i = 0; i < csrcs.size(); ++i) {
    header->arrOfCSRCs[i] = csrcs[i];
  }
  header->paddingLength = padding_size();
  header->headerLength = headers_size();
  header->payload_type_frequency = 0;
  header->extension.hasTransmissionTimeOffset =
      GetExtension<TransmissionOffset>(
          &header->extension.transmissionTimeOffset);
  header->extension.hasAbsoluteSendTime =
      GetExtension<AbsoluteSendTime>(&header->extension.absoluteSendTime);
  header->extension.hasTransportSequenceNumber =
      GetExtension<TransportSequenceNumber>(
          &header->extension.transportSequenceNumber);
  header->extension.hasAudioLevel = GetExtension<AudioLevel>(
      &header->extension.voiceActivity, &header->extension.audioLevel);
  header->extension.hasVideoRotation =
      GetExtension<VideoOrientation>(&header->extension.videoRotation);
}

size_t Packet::headers_size() const {
  return payload_offset_;
}

size_t Packet::payload_size() const {
  return payload_size_;
}

size_t Packet::padding_size() const {
  return padding_size_;
}

rtc::ArrayView<const uint8_t> Packet::payload() const {
  return rtc::MakeArrayView(data() + payload_offset_, payload_size_);
}

rtc::CopyOnWriteBuffer Packet::Buffer() const {
  return buffer_;
}

size_t Packet::capacity() const {
  return buffer_.capacity();
}

size_t Packet::size() const {
  size_t ret = payload_offset_ + payload_size_ + padding_size_;
  RTC_DCHECK_EQ(buffer_.size(), ret);
  return ret;
}

const uint8_t* Packet::data() const {
  return buffer_.cdata();
}

size_t Packet::FreeCapacity() const {
  return capacity() - size();
}

size_t Packet::MaxPayloadSize() const {
  return capacity() - payload_offset_;
}

void Packet::CopyHeaderFrom(const Packet& packet) {
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

void Packet::SetMarker(bool marker_bit) {
  marker_ = marker_bit;
  if (marker_) {
    WriteAt(1, data()[1] | 0x80);
  } else {
    WriteAt(1, data()[1] & 0x7F);
  }
}

void Packet::SetPayloadType(uint8_t payload_type) {
  RTC_DCHECK_LE(payload_type, 0x7Fu);
  payload_type_ = payload_type;
  WriteAt(1, (data()[1] & 0x80) | payload_type);
}

void Packet::SetSequenceNumber(uint16_t seq_no) {
  sequence_number_ = seq_no;
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(2), seq_no);
}

void Packet::SetTimestamp(uint32_t timestamp) {
  timestamp_ = timestamp;
  ByteWriter<uint32_t>::WriteBigEndian(WriteAt(4), timestamp);
}

void Packet::SetSsrc(uint32_t ssrc) {
  ssrc_ = ssrc;
  ByteWriter<uint32_t>::WriteBigEndian(WriteAt(8), ssrc);
}

void Packet::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  RTC_DCHECK_EQ(extensions_size_, 0);
  RTC_DCHECK_EQ(payload_size_, 0);
  RTC_DCHECK_EQ(padding_size_, 0);
  RTC_DCHECK_LE(csrcs.size(), 0x0fu);
  RTC_DCHECK_LE(kFixedHeaderSize + 4 * csrcs.size(), capacity());
  payload_offset_ = kFixedHeaderSize + 4 * csrcs.size();
  WriteAt(0, (data()[0] & 0xF0) | csrcs.size());
  size_t offset = kFixedHeaderSize;
  for (uint32_t csrc : csrcs) {
    ByteWriter<uint32_t>::WriteBigEndian(WriteAt(offset), csrc);
    offset += 4;
  }
  buffer_.SetSize(payload_offset_);
}

uint8_t* Packet::AllocatePayload(size_t size_bytes) {
  RTC_DCHECK_EQ(padding_size_, 0);
  if (payload_offset_ + size_bytes > capacity()) {
    LOG(LS_WARNING) << "Cannot set payload, not enough space in buffer.";
    return nullptr;
  }
  // Reset payload size to 0. If CopyOnWrite buffer_ was shared, this will cause
  // reallocation and memcpy. Setting size to just headers reduces memcpy size.
  buffer_.SetSize(payload_offset_);
  payload_size_ = size_bytes;
  buffer_.SetSize(payload_offset_ + payload_size_);
  return WriteAt(payload_offset_);
}

void Packet::SetPayloadSize(size_t size_bytes) {
  RTC_DCHECK_EQ(padding_size_, 0);
  RTC_DCHECK_LE(size_bytes, payload_size_);
  payload_size_ = size_bytes;
  buffer_.SetSize(payload_offset_ + payload_size_);
}

bool Packet::SetPadding(uint8_t size_bytes, Random* random) {
  RTC_DCHECK(random);
  if (payload_offset_ + payload_size_ + size_bytes > capacity()) {
    LOG(LS_WARNING) << "Cannot set padding size " << size_bytes << ", only "
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

void Packet::Clear() {
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

bool Packet::ParseBuffer(const uint8_t* buffer, size_t size) {
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
      LOG(LS_WARNING) << "Padding was set, but padding size is zero";
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
      LOG(LS_WARNING) << "Unsupported rtp extension " << profile;
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
          LOG(LS_WARNING) << "Oversized rtp header extension.";
          break;
        }

        size_t idx = id - 1;
        if (extension_entries_[idx].length != 0) {
          LOG(LS_VERBOSE) << "Duplicate rtp header extension id " << id
                          << ". Overwriting.";
        }

        extensions_size_ += kOneByteHeaderSize;
        extension_entries_[idx].offset = extension_offset + extensions_size_;
        extension_entries_[idx].length = length;
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

bool Packet::FindExtension(ExtensionType type,
                           uint8_t length,
                           uint16_t* offset) const {
  RTC_DCHECK(offset);
  for (const ExtensionInfo& extension : extension_entries_) {
    if (extension.type == type) {
      if (extension.length == 0) {
        // Extension is registered but not set.
        return false;
      }
      if (length != extension.length) {
        LOG(LS_WARNING) << "Length mismatch for extension '" << type
                        << "': expected " << static_cast<int>(length)
                        << ", received " << static_cast<int>(extension.length);
        return false;
      }
      *offset = extension.offset;
      return true;
    }
  }
  return false;
}

bool Packet::AllocateExtension(ExtensionType type,
                               uint8_t length,
                               uint16_t* offset) {
  uint8_t extension_id = ExtensionManager::kInvalidId;
  ExtensionInfo* extension_entry = nullptr;
  for (size_t i = 0; i < kMaxExtensionHeaders; ++i) {
    if (extension_entries_[i].type == type) {
      extension_id = i + 1;
      extension_entry = &extension_entries_[i];
      break;
    }
  }

  if (!extension_entry)  // Extension not registered.
    return false;

  if (extension_entry->length != 0) {  // Already allocated.
    if (length != extension_entry->length) {
      LOG(LS_WARNING) << "Length mismatch for extension '" << type
                      << "': expected " << static_cast<int>(length)
                      << ", received "
                      << static_cast<int>(extension_entry->length);
      return false;
    }
    *offset = extension_entry->offset;
    return true;
  }

  // Can't add new extension after payload/padding was set.
  if (payload_size_ > 0) {
    return false;
  }
  if (padding_size_ > 0) {
    return false;
  }

  RTC_DCHECK_GT(length, 0);
  RTC_DCHECK_LE(length, 16);

  size_t num_csrc = data()[0] & 0x0F;
  size_t extensions_offset = kFixedHeaderSize + (num_csrc * 4) + 4;
  if (extensions_offset + extensions_size_ + kOneByteHeaderSize + length >
      capacity()) {
    LOG(LS_WARNING) << "Extension cannot be registered: "
                       "Not enough space left in buffer.";
    return false;
  }

  uint16_t new_extensions_size =
      extensions_size_ + kOneByteHeaderSize + length;
  uint16_t extensions_words =
      (new_extensions_size + 3) / 4;  // Wrap up to 32bit.

  // All checks passed, write down the extension.
  if (extensions_size_ == 0) {
    RTC_DCHECK_EQ(payload_offset_, kFixedHeaderSize + (num_csrc * 4));
    WriteAt(0, data()[0] | 0x10);  // Set extension bit.
    // Profile specific ID always set to OneByteExtensionHeader.
    ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 4),
                                         kOneByteExtensionId);
  }

  WriteAt(extensions_offset + extensions_size_,
          (extension_id << 4) | (length - 1));

  extension_entry->length = length;
  *offset = extensions_offset + kOneByteHeaderSize + extensions_size_;
  extension_entry->offset = *offset;
  extensions_size_ = new_extensions_size;

  // Update header length field.
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 2),
                                       extensions_words);
  // Fill extension padding place with zeroes.
  size_t extension_padding_size = 4 * extensions_words - extensions_size_;
  memset(WriteAt(extensions_offset + extensions_size_), 0,
         extension_padding_size);
  payload_offset_ = extensions_offset + 4 * extensions_words;
  buffer_.SetSize(payload_offset_);
  return true;
}

uint8_t* Packet::WriteAt(size_t offset) {
  return buffer_.data() + offset;
}

void Packet::WriteAt(size_t offset, uint8_t byte) {
  buffer_.data()[offset] = byte;
}

}  // namespace rtp
}  // namespace webrtc
