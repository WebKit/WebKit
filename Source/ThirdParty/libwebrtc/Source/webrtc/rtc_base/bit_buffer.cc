/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bit_buffer.h"

#include <algorithm>
#include <limits>

#include "rtc_base/checks.h"

namespace {

// Returns the lowest (right-most) |bit_count| bits in |byte|.
uint8_t LowestBits(uint8_t byte, size_t bit_count) {
  RTC_DCHECK_LE(bit_count, 8);
  return byte & ((1 << bit_count) - 1);
}

// Returns the highest (left-most) |bit_count| bits in |byte|, shifted to the
// lowest bits (to the right).
uint8_t HighestBits(uint8_t byte, size_t bit_count) {
  RTC_DCHECK_LE(bit_count, 8);
  uint8_t shift = 8 - static_cast<uint8_t>(bit_count);
  uint8_t mask = 0xFF << shift;
  return (byte & mask) >> shift;
}

// Returns the highest byte of |val| in a uint8_t.
uint8_t HighestByte(uint64_t val) {
  return static_cast<uint8_t>(val >> 56);
}

// Returns the result of writing partial data from |source|, of
// |source_bit_count| size in the highest bits, to |target| at
// |target_bit_offset| from the highest bit.
uint8_t WritePartialByte(uint8_t source,
                         size_t source_bit_count,
                         uint8_t target,
                         size_t target_bit_offset) {
  RTC_DCHECK(target_bit_offset < 8);
  RTC_DCHECK(source_bit_count < 9);
  RTC_DCHECK(source_bit_count <= (8 - target_bit_offset));
  // Generate a mask for just the bits we're going to overwrite, so:
  uint8_t mask =
      // The number of bits we want, in the most significant bits...
      static_cast<uint8_t>(0xFF << (8 - source_bit_count))
      // ...shifted over to the target offset from the most signficant bit.
      >> target_bit_offset;

  // We want the target, with the bits we'll overwrite masked off, or'ed with
  // the bits from the source we want.
  return (target & ~mask) | (source >> target_bit_offset);
}

// Counts the number of bits used in the binary representation of val.
size_t CountBits(uint64_t val) {
  size_t bit_count = 0;
  while (val != 0) {
    bit_count++;
    val >>= 1;
  }
  return bit_count;
}

}  // namespace

namespace rtc {

BitBuffer::BitBuffer(const uint8_t* bytes, size_t byte_count)
    : bytes_(bytes), byte_count_(byte_count), byte_offset_(), bit_offset_() {
  RTC_DCHECK(static_cast<uint64_t>(byte_count_) <=
             std::numeric_limits<uint32_t>::max());
}

uint64_t BitBuffer::RemainingBitCount() const {
  return (static_cast<uint64_t>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

bool BitBuffer::ReadUInt8(uint8_t& val) {
  uint32_t bit_val;
  if (!ReadBits(sizeof(uint8_t) * 8, bit_val)) {
    return false;
  }
  RTC_DCHECK(bit_val <= std::numeric_limits<uint8_t>::max());
  val = static_cast<uint8_t>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt16(uint16_t& val) {
  uint32_t bit_val;
  if (!ReadBits(sizeof(uint16_t) * 8, bit_val)) {
    return false;
  }
  RTC_DCHECK(bit_val <= std::numeric_limits<uint16_t>::max());
  val = static_cast<uint16_t>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt32(uint32_t& val) {
  return ReadBits(sizeof(uint32_t) * 8, val);
}

bool BitBuffer::PeekBits(size_t bit_count, uint32_t& val) {
  // TODO(nisse): Could allow bit_count == 0 and always return success. But
  // current code reads one byte beyond end of buffer in the case that
  // RemainingBitCount() == 0 and bit_count == 0.
  RTC_DCHECK(bit_count > 0);
  if (bit_count > RemainingBitCount() || bit_count > 32) {
    return false;
  }
  const uint8_t* bytes = bytes_ + byte_offset_;
  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  uint32_t bits = LowestBits(*bytes++, remaining_bits_in_current_byte);
  // If we're reading fewer bits than what's left in the current byte, just
  // return the portion of this byte that we need.
  if (bit_count < remaining_bits_in_current_byte) {
    val = HighestBits(bits, bit_offset_ + bit_count);
    return true;
  }
  // Otherwise, subtract what we've read from the bit count and read as many
  // full bytes as we can into bits.
  bit_count -= remaining_bits_in_current_byte;
  while (bit_count >= 8) {
    bits = (bits << 8) | *bytes++;
    bit_count -= 8;
  }
  // Whatever we have left is smaller than a byte, so grab just the bits we need
  // and shift them into the lowest bits.
  if (bit_count > 0) {
    bits <<= bit_count;
    bits |= HighestBits(*bytes, bit_count);
  }
  val = bits;
  return true;
}

bool BitBuffer::PeekBits(size_t bit_count, uint64_t& val) {
  // TODO(nisse): Could allow bit_count == 0 and always return success. But
  // current code reads one byte beyond end of buffer in the case that
  // RemainingBitCount() == 0 and bit_count == 0.
  RTC_DCHECK(bit_count > 0);
  if (bit_count > RemainingBitCount() || bit_count > 64) {
    return false;
  }
  const uint8_t* bytes = bytes_ + byte_offset_;
  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  uint64_t bits = LowestBits(*bytes++, remaining_bits_in_current_byte);
  // If we're reading fewer bits than what's left in the current byte, just
  // return the portion of this byte that we need.
  if (bit_count < remaining_bits_in_current_byte) {
    val = HighestBits(bits, bit_offset_ + bit_count);
    return true;
  }
  // Otherwise, subtract what we've read from the bit count and read as many
  // full bytes as we can into bits.
  bit_count -= remaining_bits_in_current_byte;
  while (bit_count >= 8) {
    bits = (bits << 8) | *bytes++;
    bit_count -= 8;
  }
  // Whatever we have left is smaller than a byte, so grab just the bits we need
  // and shift them into the lowest bits.
  if (bit_count > 0) {
    bits <<= bit_count;
    bits |= HighestBits(*bytes, bit_count);
  }
  val = bits;
  return true;
}

bool BitBuffer::ReadBits(size_t bit_count, uint32_t& val) {
  return PeekBits(bit_count, val) && ConsumeBits(bit_count);
}

bool BitBuffer::ReadBits(size_t bit_count, uint64_t& val) {
  return PeekBits(bit_count, val) && ConsumeBits(bit_count);
}

bool BitBuffer::ConsumeBytes(size_t byte_count) {
  return ConsumeBits(byte_count * 8);
}

bool BitBuffer::ConsumeBits(size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }

  byte_offset_ += (bit_offset_ + bit_count) / 8;
  bit_offset_ = (bit_offset_ + bit_count) % 8;
  return true;
}

bool BitBuffer::ReadNonSymmetric(uint32_t num_values, uint32_t& val) {
  RTC_DCHECK_GT(num_values, 0);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);
  if (num_values == 1) {
    // When there is only one possible value, it requires zero bits to store it.
    // But ReadBits doesn't support reading zero bits.
    val = 0;
    return true;
  }
  size_t count_bits = CountBits(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  if (!ReadBits(count_bits - 1, val)) {
    return false;
  }

  if (val < num_min_bits_values) {
    return true;
  }

  uint32_t extra_bit;
  if (!ReadBits(/*bit_count=*/1, extra_bit)) {
    return false;
  }

  val = (val << 1) + extra_bit - num_min_bits_values;
  return true;
}

bool BitBuffer::ReadExponentialGolomb(uint32_t& val) {
  // Store off the current byte/bit offset, in case we want to restore them due
  // to a failed parse.
  size_t original_byte_offset = byte_offset_;
  size_t original_bit_offset = bit_offset_;

  // Count the number of leading 0 bits by peeking/consuming them one at a time.
  size_t zero_bit_count = 0;
  uint32_t peeked_bit;
  while (PeekBits(1, peeked_bit) && peeked_bit == 0) {
    zero_bit_count++;
    ConsumeBits(1);
  }

  // We should either be at the end of the stream, or the next bit should be 1.
  RTC_DCHECK(!PeekBits(1, peeked_bit) || peeked_bit == 1);

  // The bit count of the value is the number of zeros + 1. Make sure that many
  // bits fits in a uint32_t and that we have enough bits left for it, and then
  // read the value.
  size_t value_bit_count = zero_bit_count + 1;
  if (value_bit_count > 32 || !ReadBits(value_bit_count, val)) {
    RTC_CHECK(Seek(original_byte_offset, original_bit_offset));
    return false;
  }
  val -= 1;
  return true;
}

bool BitBuffer::ReadSignedExponentialGolomb(int32_t& val) {
  uint32_t unsigned_val;
  if (!ReadExponentialGolomb(unsigned_val)) {
    return false;
  }
  if ((unsigned_val & 1) == 0) {
    val = -static_cast<int32_t>(unsigned_val / 2);
  } else {
    val = (unsigned_val + 1) / 2;
  }
  return true;
}

void BitBuffer::GetCurrentOffset(size_t* out_byte_offset,
                                 size_t* out_bit_offset) {
  RTC_CHECK(out_byte_offset != nullptr);
  RTC_CHECK(out_bit_offset != nullptr);
  *out_byte_offset = byte_offset_;
  *out_bit_offset = bit_offset_;
}

bool BitBuffer::Seek(size_t byte_offset, size_t bit_offset) {
  if (byte_offset > byte_count_ || bit_offset > 7 ||
      (byte_offset == byte_count_ && bit_offset > 0)) {
    return false;
  }
  byte_offset_ = byte_offset;
  bit_offset_ = bit_offset;
  return true;
}

BitBufferWriter::BitBufferWriter(uint8_t* bytes, size_t byte_count)
    : BitBuffer(bytes, byte_count), writable_bytes_(bytes) {}

bool BitBufferWriter::WriteUInt8(uint8_t val) {
  return WriteBits(val, sizeof(uint8_t) * 8);
}

bool BitBufferWriter::WriteUInt16(uint16_t val) {
  return WriteBits(val, sizeof(uint16_t) * 8);
}

bool BitBufferWriter::WriteUInt32(uint32_t val) {
  return WriteBits(val, sizeof(uint32_t) * 8);
}

bool BitBufferWriter::WriteBits(uint64_t val, size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }
  size_t total_bits = bit_count;

  // For simplicity, push the bits we want to read from val to the highest bits.
  val <<= (sizeof(uint64_t) * 8 - bit_count);

  uint8_t* bytes = writable_bytes_ + byte_offset_;

  // The first byte is relatively special; the bit offset to write to may put us
  // in the middle of the byte, and the total bit count to write may require we
  // save the bits at the end of the byte.
  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  size_t bits_in_first_byte =
      std::min(bit_count, remaining_bits_in_current_byte);
  *bytes = WritePartialByte(HighestByte(val), bits_in_first_byte, *bytes,
                            bit_offset_);
  if (bit_count <= remaining_bits_in_current_byte) {
    // Nothing left to write, so quit early.
    return ConsumeBits(total_bits);
  }

  // Subtract what we've written from the bit count, shift it off the value, and
  // write the remaining full bytes.
  val <<= bits_in_first_byte;
  bytes++;
  bit_count -= bits_in_first_byte;
  while (bit_count >= 8) {
    *bytes++ = HighestByte(val);
    val <<= 8;
    bit_count -= 8;
  }

  // Last byte may also be partial, so write the remaining bits from the top of
  // val.
  if (bit_count > 0) {
    *bytes = WritePartialByte(HighestByte(val), bit_count, *bytes, 0);
  }

  // All done! Consume the bits we've written.
  return ConsumeBits(total_bits);
}

bool BitBufferWriter::WriteNonSymmetric(uint32_t val, uint32_t num_values) {
  RTC_DCHECK_LT(val, num_values);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);
  if (num_values == 1) {
    // When there is only one possible value, it requires zero bits to store it.
    // But WriteBits doesn't support writing zero bits.
    return true;
  }
  size_t count_bits = CountBits(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  return val < num_min_bits_values
             ? WriteBits(val, count_bits - 1)
             : WriteBits(val + num_min_bits_values, count_bits);
}

size_t BitBufferWriter::SizeNonSymmetricBits(uint32_t val,
                                             uint32_t num_values) {
  RTC_DCHECK_LT(val, num_values);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);
  size_t count_bits = CountBits(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  return val < num_min_bits_values ? (count_bits - 1) : count_bits;
}

bool BitBufferWriter::WriteExponentialGolomb(uint32_t val) {
  // We don't support reading UINT32_MAX, because it doesn't fit in a uint32_t
  // when encoded, so don't support writing it either.
  if (val == std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  uint64_t val_to_encode = static_cast<uint64_t>(val) + 1;

  // We need to write CountBits(val+1) 0s and then val+1. Since val (as a
  // uint64_t) has leading zeros, we can just write the total golomb encoded
  // size worth of bits, knowing the value will appear last.
  return WriteBits(val_to_encode, CountBits(val_to_encode) * 2 - 1);
}

bool BitBufferWriter::WriteSignedExponentialGolomb(int32_t val) {
  if (val == 0) {
    return WriteExponentialGolomb(0);
  } else if (val > 0) {
    uint32_t signed_val = val;
    return WriteExponentialGolomb((signed_val * 2) - 1);
  } else {
    if (val == std::numeric_limits<int32_t>::min())
      return false;  // Not supported, would cause overflow.
    uint32_t signed_val = -val;
    return WriteExponentialGolomb(signed_val * 2);
  }
}

}  // namespace rtc
