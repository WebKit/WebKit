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

#include <limits>

#include "rtc_base/arraysize.h"
#include "rtc_base/byte_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {

using ::testing::ElementsAre;

TEST(BitBufferTest, ConsumeBits) {
  const uint8_t bytes[64] = {0};
  BitBuffer buffer(bytes, 32);
  uint64_t total_bits = 32 * 8;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(3));
  total_bits -= 3;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(3));
  total_bits -= 3;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(15));
  total_bits -= 15;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
  EXPECT_TRUE(buffer.ConsumeBits(37));
  total_bits -= 37;
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());

  EXPECT_FALSE(buffer.ConsumeBits(32 * 8));
  EXPECT_EQ(total_bits, buffer.RemainingBitCount());
}

TEST(BitBufferTest, ReadBytesAligned) {
  const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89};
  uint8_t val8;
  uint16_t val16;
  uint32_t val32;
  BitBuffer buffer(bytes, 8);
  EXPECT_TRUE(buffer.ReadUInt8(val8));
  EXPECT_EQ(0x0Au, val8);
  EXPECT_TRUE(buffer.ReadUInt8(val8));
  EXPECT_EQ(0xBCu, val8);
  EXPECT_TRUE(buffer.ReadUInt16(val16));
  EXPECT_EQ(0xDEF1u, val16);
  EXPECT_TRUE(buffer.ReadUInt32(val32));
  EXPECT_EQ(0x23456789u, val32);
}

TEST(BitBufferTest, ReadBytesOffset4) {
  const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23,
                           0x45, 0x67, 0x89, 0x0A};
  uint8_t val8;
  uint16_t val16;
  uint32_t val32;
  BitBuffer buffer(bytes, 9);
  EXPECT_TRUE(buffer.ConsumeBits(4));

  EXPECT_TRUE(buffer.ReadUInt8(val8));
  EXPECT_EQ(0xABu, val8);
  EXPECT_TRUE(buffer.ReadUInt8(val8));
  EXPECT_EQ(0xCDu, val8);
  EXPECT_TRUE(buffer.ReadUInt16(val16));
  EXPECT_EQ(0xEF12u, val16);
  EXPECT_TRUE(buffer.ReadUInt32(val32));
  EXPECT_EQ(0x34567890u, val32);
}

TEST(BitBufferTest, ReadBytesOffset3) {
  // The pattern we'll check against is counting down from 0b1111. It looks
  // weird here because it's all offset by 3.
  // Byte pattern is:
  //    56701234
  //  0b00011111,
  //  0b11011011,
  //  0b10010111,
  //  0b01010011,
  //  0b00001110,
  //  0b11001010,
  //  0b10000110,
  //  0b01000010
  //       xxxxx <-- last 5 bits unused.

  // The bytes. It almost looks like counting down by two at a time, except the
  // jump at 5->3->0, since that's when the high bit is turned off.
  const uint8_t bytes[] = {0x1F, 0xDB, 0x97, 0x53, 0x0E, 0xCA, 0x86, 0x42};

  uint8_t val8;
  uint16_t val16;
  uint32_t val32;
  BitBuffer buffer(bytes, 8);
  EXPECT_TRUE(buffer.ConsumeBits(3));
  EXPECT_TRUE(buffer.ReadUInt8(val8));
  EXPECT_EQ(0xFEu, val8);
  EXPECT_TRUE(buffer.ReadUInt16(val16));
  EXPECT_EQ(0xDCBAu, val16);
  EXPECT_TRUE(buffer.ReadUInt32(val32));
  EXPECT_EQ(0x98765432u, val32);
  // 5 bits left unread. Not enough to read a uint8_t.
  EXPECT_EQ(5u, buffer.RemainingBitCount());
  EXPECT_FALSE(buffer.ReadUInt8(val8));
}

TEST(BitBufferTest, ReadBits) {
  // Bit values are:
  //  0b01001101,
  //  0b00110010
  const uint8_t bytes[] = {0x4D, 0x32};
  uint32_t val;
  BitBuffer buffer(bytes, 2);
  EXPECT_TRUE(buffer.ReadBits(3, val));
  // 0b010
  EXPECT_EQ(0x2u, val);
  EXPECT_TRUE(buffer.ReadBits(2, val));
  // 0b01
  EXPECT_EQ(0x1u, val);
  EXPECT_TRUE(buffer.ReadBits(7, val));
  // 0b1010011
  EXPECT_EQ(0x53u, val);
  EXPECT_TRUE(buffer.ReadBits(2, val));
  // 0b00
  EXPECT_EQ(0x0u, val);
  EXPECT_TRUE(buffer.ReadBits(1, val));
  // 0b1
  EXPECT_EQ(0x1u, val);
  EXPECT_TRUE(buffer.ReadBits(1, val));
  // 0b0
  EXPECT_EQ(0x0u, val);

  EXPECT_FALSE(buffer.ReadBits(1, val));
}

TEST(BitBufferTest, ReadBits64) {
  const uint8_t bytes[] = {0x4D, 0x32, 0xAB, 0x54, 0x00, 0xFF, 0xFE, 0x01,
                           0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};
  BitBuffer buffer(bytes, 16);
  uint64_t val;

  // Peek and read first 33 bits.
  EXPECT_TRUE(buffer.PeekBits(33, val));
  EXPECT_EQ(0x4D32AB5400FFFE01ull >> (64 - 33), val);
  val = 0;
  EXPECT_TRUE(buffer.ReadBits(33, val));
  EXPECT_EQ(0x4D32AB5400FFFE01ull >> (64 - 33), val);

  // Peek and read next 31 bits.
  constexpr uint64_t kMask31Bits = (1ull << 32) - 1;
  EXPECT_TRUE(buffer.PeekBits(31, val));
  EXPECT_EQ(0x4D32AB5400FFFE01ull & kMask31Bits, val);
  val = 0;
  EXPECT_TRUE(buffer.ReadBits(31, val));
  EXPECT_EQ(0x4D32AB5400FFFE01ull & kMask31Bits, val);

  // Peek and read remaining 64 bits.
  EXPECT_TRUE(buffer.PeekBits(64, val));
  EXPECT_EQ(0xABCDEF0123456789ull, val);
  val = 0;
  EXPECT_TRUE(buffer.ReadBits(64, val));
  EXPECT_EQ(0xABCDEF0123456789ull, val);

  // Nothing more to read.
  EXPECT_FALSE(buffer.ReadBits(1, val));
}

TEST(BitBufferDeathTest, SetOffsetValues) {
  uint8_t bytes[4] = {0};
  BitBufferWriter buffer(bytes, 4);

  size_t byte_offset, bit_offset;
  // Bit offsets are [0,7].
  EXPECT_TRUE(buffer.Seek(0, 0));
  EXPECT_TRUE(buffer.Seek(0, 7));
  buffer.GetCurrentOffset(&byte_offset, &bit_offset);
  EXPECT_EQ(0u, byte_offset);
  EXPECT_EQ(7u, bit_offset);
  EXPECT_FALSE(buffer.Seek(0, 8));
  buffer.GetCurrentOffset(&byte_offset, &bit_offset);
  EXPECT_EQ(0u, byte_offset);
  EXPECT_EQ(7u, bit_offset);
  // Byte offsets are [0,length]. At byte offset length, the bit offset must be
  // 0.
  EXPECT_TRUE(buffer.Seek(0, 0));
  EXPECT_TRUE(buffer.Seek(2, 4));
  buffer.GetCurrentOffset(&byte_offset, &bit_offset);
  EXPECT_EQ(2u, byte_offset);
  EXPECT_EQ(4u, bit_offset);
  EXPECT_TRUE(buffer.Seek(4, 0));
  EXPECT_FALSE(buffer.Seek(5, 0));
  buffer.GetCurrentOffset(&byte_offset, &bit_offset);
  EXPECT_EQ(4u, byte_offset);
  EXPECT_EQ(0u, bit_offset);
  EXPECT_FALSE(buffer.Seek(4, 1));

// Disable death test on Android because it relies on fork() and doesn't play
// nicely.
#if GTEST_HAS_DEATH_TEST
#if !defined(WEBRTC_ANDROID)
  // Passing a null out parameter is death.
  EXPECT_DEATH(buffer.GetCurrentOffset(&byte_offset, nullptr), "");
#endif
#endif
}

TEST(BitBufferTest, ReadNonSymmetricSameNumberOfBitsWhenNumValuesPowerOf2) {
  const uint8_t bytes[2] = {0xf3, 0xa0};
  BitBuffer reader(bytes, 2);

  uint32_t values[4];
  ASSERT_EQ(reader.RemainingBitCount(), 16u);
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/1 << 4, values[0]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/1 << 4, values[1]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/1 << 4, values[2]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/1 << 4, values[3]));
  ASSERT_EQ(reader.RemainingBitCount(), 0u);

  EXPECT_THAT(values, ElementsAre(0xf, 0x3, 0xa, 0x0));
}

TEST(BitBufferWriterTest,
     WriteNonSymmetricSameNumberOfBitsWhenNumValuesPowerOf2) {
  uint8_t bytes[2] = {};
  BitBufferWriter writer(bytes, 2);

  ASSERT_EQ(writer.RemainingBitCount(), 16u);
  EXPECT_TRUE(writer.WriteNonSymmetric(0xf, /*num_values=*/1 << 4));
  ASSERT_EQ(writer.RemainingBitCount(), 12u);
  EXPECT_TRUE(writer.WriteNonSymmetric(0x3, /*num_values=*/1 << 4));
  ASSERT_EQ(writer.RemainingBitCount(), 8u);
  EXPECT_TRUE(writer.WriteNonSymmetric(0xa, /*num_values=*/1 << 4));
  ASSERT_EQ(writer.RemainingBitCount(), 4u);
  EXPECT_TRUE(writer.WriteNonSymmetric(0x0, /*num_values=*/1 << 4));
  ASSERT_EQ(writer.RemainingBitCount(), 0u);

  EXPECT_THAT(bytes, ElementsAre(0xf3, 0xa0));
}

TEST(BitBufferWriterTest, NonSymmetricReadsMatchesWrites) {
  uint8_t bytes[2] = {};
  BitBufferWriter writer(bytes, 2);

  EXPECT_EQ(BitBufferWriter::SizeNonSymmetricBits(/*val=*/1, /*num_values=*/6),
            2u);
  EXPECT_EQ(BitBufferWriter::SizeNonSymmetricBits(/*val=*/2, /*num_values=*/6),
            3u);
  // Values [0, 1] can fit into two bit.
  ASSERT_EQ(writer.RemainingBitCount(), 16u);
  EXPECT_TRUE(writer.WriteNonSymmetric(/*val=*/0, /*num_values=*/6));
  ASSERT_EQ(writer.RemainingBitCount(), 14u);
  EXPECT_TRUE(writer.WriteNonSymmetric(/*val=*/1, /*num_values=*/6));
  ASSERT_EQ(writer.RemainingBitCount(), 12u);
  // Values [2, 5] require 3 bits.
  EXPECT_TRUE(writer.WriteNonSymmetric(/*val=*/2, /*num_values=*/6));
  ASSERT_EQ(writer.RemainingBitCount(), 9u);
  EXPECT_TRUE(writer.WriteNonSymmetric(/*val=*/3, /*num_values=*/6));
  ASSERT_EQ(writer.RemainingBitCount(), 6u);
  EXPECT_TRUE(writer.WriteNonSymmetric(/*val=*/4, /*num_values=*/6));
  ASSERT_EQ(writer.RemainingBitCount(), 3u);
  EXPECT_TRUE(writer.WriteNonSymmetric(/*val=*/5, /*num_values=*/6));
  ASSERT_EQ(writer.RemainingBitCount(), 0u);

  // Bit values are
  // 00.01.100.101.110.111 = 00011001|01110111 = 0x19|77
  EXPECT_THAT(bytes, ElementsAre(0x19, 0x77));

  rtc::BitBuffer reader(bytes, 2);
  uint32_t values[6];
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/6, values[0]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/6, values[1]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/6, values[2]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/6, values[3]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/6, values[4]));
  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/6, values[5]));

  EXPECT_THAT(values, ElementsAre(0, 1, 2, 3, 4, 5));
}

TEST(BitBufferTest, ReadNonSymmetricOnlyValueConsumesNoBits) {
  const uint8_t bytes[2] = {};
  BitBuffer reader(bytes, 2);
  uint32_t value = 0xFFFFFFFF;
  ASSERT_EQ(reader.RemainingBitCount(), 16u);

  EXPECT_TRUE(reader.ReadNonSymmetric(/*num_values=*/1, value));

  EXPECT_EQ(value, 0u);
  EXPECT_EQ(reader.RemainingBitCount(), 16u);
}

TEST(BitBufferWriterTest, WriteNonSymmetricOnlyValueConsumesNoBits) {
  uint8_t bytes[2] = {};
  BitBufferWriter writer(bytes, 2);
  ASSERT_EQ(writer.RemainingBitCount(), 16u);

  EXPECT_TRUE(writer.WriteNonSymmetric(0, /*num_values=*/1));

  EXPECT_EQ(writer.RemainingBitCount(), 16u);
}

uint64_t GolombEncoded(uint32_t val) {
  val++;
  uint32_t bit_counter = val;
  uint64_t bit_count = 0;
  while (bit_counter > 0) {
    bit_count++;
    bit_counter >>= 1;
  }
  return static_cast<uint64_t>(val) << (64 - (bit_count * 2 - 1));
}

TEST(BitBufferTest, GolombUint32Values) {
  ByteBufferWriter byteBuffer;
  byteBuffer.Resize(16);
  BitBuffer buffer(reinterpret_cast<const uint8_t*>(byteBuffer.Data()),
                   byteBuffer.Capacity());
  // Test over the uint32_t range with a large enough step that the test doesn't
  // take forever. Around 20,000 iterations should do.
  const int kStep = std::numeric_limits<uint32_t>::max() / 20000;
  for (uint32_t i = 0; i < std::numeric_limits<uint32_t>::max() - kStep;
       i += kStep) {
    uint64_t encoded_val = GolombEncoded(i);
    byteBuffer.Clear();
    byteBuffer.WriteUInt64(encoded_val);
    uint32_t decoded_val;
    EXPECT_TRUE(buffer.Seek(0, 0));
    EXPECT_TRUE(buffer.ReadExponentialGolomb(decoded_val));
    EXPECT_EQ(i, decoded_val);
  }
}

TEST(BitBufferTest, SignedGolombValues) {
  uint8_t golomb_bits[] = {
      0x80,  // 1
      0x40,  // 010
      0x60,  // 011
      0x20,  // 00100
      0x38,  // 00111
  };
  int32_t expected[] = {0, 1, -1, 2, -3};
  for (size_t i = 0; i < sizeof(golomb_bits); ++i) {
    BitBuffer buffer(&golomb_bits[i], 1);
    int32_t decoded_val;
    ASSERT_TRUE(buffer.ReadSignedExponentialGolomb(decoded_val));
    EXPECT_EQ(expected[i], decoded_val)
        << "Mismatch in expected/decoded value for golomb_bits[" << i
        << "]: " << static_cast<int>(golomb_bits[i]);
  }
}

TEST(BitBufferTest, NoGolombOverread) {
  const uint8_t bytes[] = {0x00, 0xFF, 0xFF};
  // Make sure the bit buffer correctly enforces byte length on golomb reads.
  // If it didn't, the above buffer would be valid at 3 bytes.
  BitBuffer buffer(bytes, 1);
  uint32_t decoded_val;
  EXPECT_FALSE(buffer.ReadExponentialGolomb(decoded_val));

  BitBuffer longer_buffer(bytes, 2);
  EXPECT_FALSE(longer_buffer.ReadExponentialGolomb(decoded_val));

  BitBuffer longest_buffer(bytes, 3);
  EXPECT_TRUE(longest_buffer.ReadExponentialGolomb(decoded_val));
  // Golomb should have read 9 bits, so 0x01FF, and since it is golomb, the
  // result is 0x01FF - 1 = 0x01FE.
  EXPECT_EQ(0x01FEu, decoded_val);
}

TEST(BitBufferWriterTest, SymmetricReadWrite) {
  uint8_t bytes[16] = {0};
  BitBufferWriter buffer(bytes, 4);

  // Write some bit data at various sizes.
  EXPECT_TRUE(buffer.WriteBits(0x2u, 3));
  EXPECT_TRUE(buffer.WriteBits(0x1u, 2));
  EXPECT_TRUE(buffer.WriteBits(0x53u, 7));
  EXPECT_TRUE(buffer.WriteBits(0x0u, 2));
  EXPECT_TRUE(buffer.WriteBits(0x1u, 1));
  EXPECT_TRUE(buffer.WriteBits(0x1ABCDu, 17));
  // That should be all that fits in the buffer.
  EXPECT_FALSE(buffer.WriteBits(1, 1));

  EXPECT_TRUE(buffer.Seek(0, 0));
  uint32_t val;
  EXPECT_TRUE(buffer.ReadBits(3, val));
  EXPECT_EQ(0x2u, val);
  EXPECT_TRUE(buffer.ReadBits(2, val));
  EXPECT_EQ(0x1u, val);
  EXPECT_TRUE(buffer.ReadBits(7, val));
  EXPECT_EQ(0x53u, val);
  EXPECT_TRUE(buffer.ReadBits(2, val));
  EXPECT_EQ(0x0u, val);
  EXPECT_TRUE(buffer.ReadBits(1, val));
  EXPECT_EQ(0x1u, val);
  EXPECT_TRUE(buffer.ReadBits(17, val));
  EXPECT_EQ(0x1ABCDu, val);
  // And there should be nothing left.
  EXPECT_FALSE(buffer.ReadBits(1, val));
}

TEST(BitBufferWriterTest, SymmetricBytesMisaligned) {
  uint8_t bytes[16] = {0};
  BitBufferWriter buffer(bytes, 16);

  // Offset 3, to get things misaligned.
  EXPECT_TRUE(buffer.ConsumeBits(3));
  EXPECT_TRUE(buffer.WriteUInt8(0x12u));
  EXPECT_TRUE(buffer.WriteUInt16(0x3456u));
  EXPECT_TRUE(buffer.WriteUInt32(0x789ABCDEu));

  buffer.Seek(0, 3);
  uint8_t val8;
  uint16_t val16;
  uint32_t val32;
  EXPECT_TRUE(buffer.ReadUInt8(val8));
  EXPECT_EQ(0x12u, val8);
  EXPECT_TRUE(buffer.ReadUInt16(val16));
  EXPECT_EQ(0x3456u, val16);
  EXPECT_TRUE(buffer.ReadUInt32(val32));
  EXPECT_EQ(0x789ABCDEu, val32);
}

TEST(BitBufferWriterTest, SymmetricGolomb) {
  char test_string[] = "my precious";
  uint8_t bytes[64] = {0};
  BitBufferWriter buffer(bytes, 64);
  for (size_t i = 0; i < arraysize(test_string); ++i) {
    EXPECT_TRUE(buffer.WriteExponentialGolomb(test_string[i]));
  }
  buffer.Seek(0, 0);
  for (size_t i = 0; i < arraysize(test_string); ++i) {
    uint32_t val;
    EXPECT_TRUE(buffer.ReadExponentialGolomb(val));
    EXPECT_LE(val, std::numeric_limits<uint8_t>::max());
    EXPECT_EQ(test_string[i], static_cast<char>(val));
  }
}

TEST(BitBufferWriterTest, WriteClearsBits) {
  uint8_t bytes[] = {0xFF, 0xFF};
  BitBufferWriter buffer(bytes, 2);
  EXPECT_TRUE(buffer.ConsumeBits(3));
  EXPECT_TRUE(buffer.WriteBits(0, 1));
  EXPECT_EQ(0xEFu, bytes[0]);
  EXPECT_TRUE(buffer.WriteBits(0, 3));
  EXPECT_EQ(0xE1u, bytes[0]);
  EXPECT_TRUE(buffer.WriteBits(0, 2));
  EXPECT_EQ(0xE0u, bytes[0]);
  EXPECT_EQ(0x7F, bytes[1]);
}

}  // namespace rtc
