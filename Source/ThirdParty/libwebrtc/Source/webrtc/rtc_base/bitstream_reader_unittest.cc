/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bitstream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <limits>

#include "absl/numeric/bits.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(BitstreamReaderTest, InDebugModeRequiresToCheckOkStatusBeforeDestruction) {
  const uint8_t bytes[32] = {};
  absl::optional<BitstreamReader> reader(absl::in_place, bytes);

  EXPECT_GE(reader->ReadBits(7), 0u);
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(OS_ANDROID)
  EXPECT_DEATH(reader = absl::nullopt, "");
#endif
  EXPECT_TRUE(reader->Ok());
  reader = absl::nullopt;
}

TEST(BitstreamReaderTest, InDebugModeMayCheckRemainingBitsInsteadOfOkStatus) {
  const uint8_t bytes[32] = {};
  absl::optional<BitstreamReader> reader(absl::in_place, bytes);

  EXPECT_GE(reader->ReadBit(), 0);
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(OS_ANDROID)
  EXPECT_DEATH(reader = absl::nullopt, "");
#endif
  EXPECT_GE(reader->RemainingBitCount(), 0);
  reader = absl::nullopt;
}

TEST(BitstreamReaderTest, ConsumeBits) {
  const uint8_t bytes[32] = {};
  BitstreamReader reader(bytes);

  int total_bits = 32 * 8;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(3);
  total_bits -= 3;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(3);
  total_bits -= 3;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(15);
  total_bits -= 15;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  reader.ConsumeBits(67);
  total_bits -= 67;
  EXPECT_EQ(reader.RemainingBitCount(), total_bits);
  EXPECT_TRUE(reader.Ok());

  reader.ConsumeBits(32 * 8);
  EXPECT_FALSE(reader.Ok());
  EXPECT_LT(reader.RemainingBitCount(), 0);
}

TEST(BitstreamReaderTest, ConsumeLotsOfBits) {
  const uint8_t bytes[1] = {};
  BitstreamReader reader(bytes);

  reader.ConsumeBits(std::numeric_limits<int>::max());
  reader.ConsumeBits(std::numeric_limits<int>::max());
  EXPECT_GE(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBit) {
  const uint8_t bytes[] = {0b0100'0001, 0b1011'0001};
  BitstreamReader reader(bytes);
  // First byte.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);

  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);

  // Second byte.
  EXPECT_EQ(reader.ReadBit(), 1);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);
  EXPECT_EQ(reader.ReadBit(), 1);

  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_EQ(reader.ReadBit(), 1);

  EXPECT_TRUE(reader.Ok());
  // Try to read beyound the buffer.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBoolConsumesSingleBit) {
  const uint8_t bytes[] = {0b1010'1010};
  BitstreamReader reader(bytes);
  ASSERT_EQ(reader.RemainingBitCount(), 8);
  EXPECT_TRUE(reader.Read<bool>());
  EXPECT_EQ(reader.RemainingBitCount(), 7);
}

TEST(BitstreamReaderTest, ReadBytesAligned) {
  const uint8_t bytes[] = {0x0A,        //
                           0xBC,        //
                           0xDE, 0xF1,  //
                           0x23, 0x45, 0x67, 0x89};
  BitstreamReader reader(bytes);
  EXPECT_EQ(reader.Read<uint8_t>(), 0x0Au);
  EXPECT_EQ(reader.Read<uint8_t>(), 0xBCu);
  EXPECT_EQ(reader.Read<uint16_t>(), 0xDEF1u);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x23456789u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBytesOffset4) {
  const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23,
                           0x45, 0x67, 0x89, 0x0A};
  BitstreamReader reader(bytes);
  reader.ConsumeBits(4);

  EXPECT_EQ(reader.Read<uint8_t>(), 0xABu);
  EXPECT_EQ(reader.Read<uint8_t>(), 0xCDu);
  EXPECT_EQ(reader.Read<uint16_t>(), 0xEF12u);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x34567890u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBytesOffset3) {
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

  BitstreamReader reader(bytes);
  reader.ConsumeBits(3);
  EXPECT_EQ(reader.Read<uint8_t>(), 0xFEu);
  EXPECT_EQ(reader.Read<uint16_t>(), 0xDCBAu);
  EXPECT_EQ(reader.Read<uint32_t>(), 0x98765432u);
  EXPECT_TRUE(reader.Ok());

  // 5 bits left unread. Not enough to read a uint8_t.
  EXPECT_EQ(reader.RemainingBitCount(), 5);
  EXPECT_EQ(reader.Read<uint8_t>(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBits) {
  const uint8_t bytes[] = {0b010'01'101, 0b0011'00'1'0};
  BitstreamReader reader(bytes);
  EXPECT_EQ(reader.ReadBits(3), 0b010u);
  EXPECT_EQ(reader.ReadBits(2), 0b01u);
  EXPECT_EQ(reader.ReadBits(7), 0b101'0011u);
  EXPECT_EQ(reader.ReadBits(2), 0b00u);
  EXPECT_EQ(reader.ReadBits(1), 0b1u);
  EXPECT_EQ(reader.ReadBits(1), 0b0u);
  EXPECT_TRUE(reader.Ok());

  EXPECT_EQ(reader.ReadBits(1), 0u);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadZeroBits) {
  BitstreamReader reader(rtc::ArrayView<const uint8_t>(nullptr, 0));

  EXPECT_EQ(reader.ReadBits(0), 0u);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBitFromEmptyArray) {
  BitstreamReader reader(rtc::ArrayView<const uint8_t>(nullptr, 0));

  // Trying to read from the empty array shouldn't dereference the pointer,
  // i.e. shouldn't crash.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBitsFromEmptyArray) {
  BitstreamReader reader(rtc::ArrayView<const uint8_t>(nullptr, 0));

  // Trying to read from the empty array shouldn't dereference the pointer,
  // i.e. shouldn't crash.
  EXPECT_EQ(reader.ReadBits(1), 0u);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadBits64) {
  const uint8_t bytes[] = {0x4D, 0x32, 0xAB, 0x54, 0x00, 0xFF, 0xFE, 0x01,
                           0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};
  BitstreamReader reader(bytes);

  EXPECT_EQ(reader.ReadBits(33), 0x4D32AB5400FFFE01u >> (64 - 33));

  constexpr uint64_t kMask31Bits = (1ull << 32) - 1;
  EXPECT_EQ(reader.ReadBits(31), 0x4D32AB5400FFFE01ull & kMask31Bits);

  EXPECT_EQ(reader.ReadBits(64), 0xABCDEF0123456789ull);
  EXPECT_TRUE(reader.Ok());

  // Nothing more to read.
  EXPECT_EQ(reader.ReadBit(), 0);
  EXPECT_FALSE(reader.Ok());
}

TEST(BitstreamReaderTest, CanPeekBitsUsingCopyConstructor) {
  // BitstreamReader doesn't have peek function. To simulate it, user may use
  // cheap BitstreamReader copy constructor.
  const uint8_t bytes[] = {0x0A, 0xBC};
  BitstreamReader reader(bytes);
  reader.ConsumeBits(4);
  ASSERT_EQ(reader.RemainingBitCount(), 12);

  BitstreamReader peeker = reader;
  EXPECT_EQ(peeker.ReadBits(8), 0xABu);
  EXPECT_EQ(peeker.RemainingBitCount(), 4);

  EXPECT_EQ(reader.RemainingBitCount(), 12);
  // Can resume reading from before peeker was created.
  EXPECT_EQ(reader.ReadBits(4), 0xAu);
  EXPECT_EQ(reader.RemainingBitCount(), 8);
}

TEST(BitstreamReaderTest,
     ReadNonSymmetricSameNumberOfBitsWhenNumValuesPowerOf2) {
  const uint8_t bytes[2] = {0xf3, 0xa0};
  BitstreamReader reader(bytes);

  ASSERT_EQ(reader.RemainingBitCount(), 16);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0xfu);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0x3u);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0xau);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1 << 4), 0x0u);
  EXPECT_EQ(reader.RemainingBitCount(), 0);
  EXPECT_TRUE(reader.Ok());
}

TEST(BitstreamReaderTest, ReadNonSymmetricOnlyValueConsumesZeroBits) {
  const uint8_t bytes[2] = {};
  BitstreamReader reader(bytes);

  ASSERT_EQ(reader.RemainingBitCount(), 16);
  EXPECT_EQ(reader.ReadNonSymmetric(/*num_values=*/1), 0u);
  EXPECT_EQ(reader.RemainingBitCount(), 16);
}

std::array<uint8_t, 8> GolombEncoded(uint32_t val) {
  int val_width = absl::bit_width(val + 1);
  int total_width = 2 * val_width - 1;
  uint64_t representation = (uint64_t{val} + 1) << (64 - total_width);
  std::array<uint8_t, 8> result;
  for (int i = 0; i < 8; ++i) {
    result[i] = representation >> (7 - i) * 8;
  }
  return result;
}

TEST(BitstreamReaderTest, GolombUint32Values) {
  // Test over the uint32_t range with a large enough step that the test doesn't
  // take forever. Around 20,000 iterations should do.
  const int kStep = std::numeric_limits<uint32_t>::max() / 20000;
  for (uint32_t i = 0; i < std::numeric_limits<uint32_t>::max() - kStep;
       i += kStep) {
    std::array<uint8_t, 8> buffer = GolombEncoded(i);
    BitstreamReader reader(buffer);
    // Use assert instead of EXPECT to avoid spamming thousands of failed
    // expectation when this test fails.
    ASSERT_EQ(reader.ReadExponentialGolomb(), i);
    EXPECT_TRUE(reader.Ok());
  }
}

TEST(BitstreamReaderTest, SignedGolombValues) {
  uint8_t golomb_bits[][1] = {
      {0b1'0000000}, {0b010'00000}, {0b011'00000}, {0b00100'000}, {0b00111'000},
  };
  int expected[] = {0, 1, -1, 2, -3};
  for (size_t i = 0; i < sizeof(golomb_bits); ++i) {
    BitstreamReader reader(golomb_bits[i]);
    EXPECT_EQ(reader.ReadSignedExponentialGolomb(), expected[i])
        << "Mismatch in expected/decoded value for golomb_bits[" << i
        << "]: " << static_cast<int>(golomb_bits[i][0]);
    EXPECT_TRUE(reader.Ok());
  }
}

TEST(BitstreamReaderTest, NoGolombOverread) {
  const uint8_t bytes[] = {0x00, 0xFF, 0xFF};
  // Make sure the bit buffer correctly enforces byte length on golomb reads.
  // If it didn't, the above buffer would be valid at 3 bytes.
  BitstreamReader reader1(rtc::MakeArrayView(bytes, 1));
  // When parse fails, `ReadExponentialGolomb` may return any number.
  reader1.ReadExponentialGolomb();
  EXPECT_FALSE(reader1.Ok());

  BitstreamReader reader2(rtc::MakeArrayView(bytes, 2));
  reader2.ReadExponentialGolomb();
  EXPECT_FALSE(reader2.Ok());

  BitstreamReader reader3(bytes);
  // Golomb should have read 9 bits, so 0x01FF, and since it is golomb, the
  // result is 0x01FF - 1 = 0x01FE.
  EXPECT_EQ(reader3.ReadExponentialGolomb(), 0x01FEu);
  EXPECT_TRUE(reader3.Ok());
}

}  // namespace
}  // namespace webrtc
