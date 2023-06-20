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

#include "api/array_view.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/byte_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {

using ::testing::ElementsAre;
using ::webrtc::BitstreamReader;

TEST(BitBufferWriterTest, ConsumeBits) {
  uint8_t bytes[64] = {0};
  BitBufferWriter buffer(bytes, 32);
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

TEST(BitBufferWriterDeathTest, SetOffsetValues) {
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

  BitstreamReader reader(bytes);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/6), 0u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/6), 1u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/6), 2u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/6), 3u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/6), 4u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/6), 5u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitBufferWriterTest, WriteNonSymmetricOnlyValueConsumesNoBits) {
  uint8_t bytes[2] = {};
  BitBufferWriter writer(bytes, 2);
  ASSERT_EQ(writer.RemainingBitCount(), 16u);

  EXPECT_TRUE(writer.WriteNonSymmetric(0, /*num_values=*/1));

  EXPECT_EQ(writer.RemainingBitCount(), 16u);
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

  BitstreamReader reader(rtc::MakeArrayView(bytes, 4));
  EXPECT_EQ(reader.ReadBits(3), 0x2u);
  EXPECT_EQ(reader.ReadBits(2), 0x1u);
  EXPECT_EQ(reader.ReadBits(7), 0x53u);
  EXPECT_EQ(reader.ReadBits(2), 0x0u);
  EXPECT_EQ(reader.ReadBits(1), 0x1u);
  EXPECT_EQ(reader.ReadBits(17), 0x1ABCDu);
  // And there should be nothing left.
  EXPECT_EQ(reader.RemainingBitCount(), 0);
}

TEST(BitBufferWriterTest, SymmetricBytesMisaligned) {
  uint8_t bytes[16] = {0};
  BitBufferWriter buffer(bytes, 16);

  // Offset 3, to get things misaligned.
  EXPECT_TRUE(buffer.ConsumeBits(3));
  EXPECT_TRUE(buffer.WriteUInt8(0x12u));
  EXPECT_TRUE(buffer.WriteUInt16(0x3456u));
  EXPECT_TRUE(buffer.WriteUInt32(0x789ABCDEu));

  BitstreamReader reader(bytes);
  reader.ConsumeBits(3);
  EXPECT_EQ(reader.Read<uint8_t>(), 0x12u);
  EXPECT_EQ(reader.Read<uint16_t>(), 0x3456u);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x789ABCDEu);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitBufferWriterTest, SymmetricGolomb) {
  char test_string[] = "my precious";
  uint8_t bytes[64] = {0};
  BitBufferWriter buffer(bytes, 64);
  for (size_t i = 0; i < arraysize(test_string); ++i) {
    EXPECT_TRUE(buffer.WriteExponentialGolomb(test_string[i]));
  }
  BitstreamReader reader(bytes);
  for (size_t i = 0; i < arraysize(test_string); ++i) {
    EXPECT_EQ(int64_t{reader.ReadExponentialGolomb()}, int64_t{test_string[i]});
  }
  EXPECT_TRUE(reader.Ok());
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

TEST(BitBufferWriterTest, WriteLeb128) {
  uint8_t small_number[2];
  BitBufferWriter small_buffer(small_number, sizeof(small_number));
  EXPECT_TRUE(small_buffer.WriteLeb128(129));
  EXPECT_THAT(small_number, ElementsAre(0x81, 0x01));

  uint8_t large_number[10];
  BitBufferWriter large_buffer(large_number, sizeof(large_number));
  EXPECT_TRUE(large_buffer.WriteLeb128(std::numeric_limits<uint64_t>::max()));
  EXPECT_THAT(large_number, ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0x01));
}

TEST(BitBufferWriterTest, WriteLeb128TooSmallBuffer) {
  uint8_t bytes[1];
  BitBufferWriter buffer(bytes, sizeof(bytes));
  EXPECT_FALSE(buffer.WriteLeb128(12345));
}

TEST(BitBufferWriterTest, WriteString) {
  uint8_t buffer[2];
  BitBufferWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WriteString("ab"));
  EXPECT_THAT(buffer, ElementsAre('a', 'b'));
}

TEST(BitBufferWriterTest, WriteStringTooSmallBuffer) {
  uint8_t buffer[2];
  BitBufferWriter writer(buffer, sizeof(buffer));
  EXPECT_FALSE(writer.WriteString("abc"));
}

}  // namespace rtc
