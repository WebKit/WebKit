/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/crc32c.h"

#include "test/gmock.h"

namespace dcsctp {
namespace {

constexpr std::array<const uint8_t, 0> kEmpty = {};
constexpr std::array<const uint8_t, 1> kZero = {0};
constexpr std::array<const uint8_t, 4> kManyZeros = {0, 0, 0, 0};
constexpr std::array<const uint8_t, 4> kShort = {1, 2, 3, 4};
constexpr std::array<const uint8_t, 8> kLong = {1, 2, 3, 4, 5, 6, 7, 8};
// https://tools.ietf.org/html/rfc3720#appendix-B.4
constexpr std::array<const uint8_t, 32> k32Zeros = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr std::array<const uint8_t, 32> k32Ones = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr std::array<const uint8_t, 32> k32Incrementing = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
constexpr std::array<const uint8_t, 32> k32Decrementing = {
    31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
    15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
constexpr std::array<const uint8_t, 48> kISCSICommandPDU = {
    0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TEST(Crc32Test, TestVectors) {
  EXPECT_EQ(GenerateCrc32C(kEmpty), 0U);
  EXPECT_EQ(GenerateCrc32C(kZero), 0x51537d52U);
  EXPECT_EQ(GenerateCrc32C(kManyZeros), 0xc74b6748U);
  EXPECT_EQ(GenerateCrc32C(kShort), 0xf48c3029U);
  EXPECT_EQ(GenerateCrc32C(kLong), 0x811f8946U);
  // https://tools.ietf.org/html/rfc3720#appendix-B.4
  EXPECT_EQ(GenerateCrc32C(k32Zeros), 0xaa36918aU);
  EXPECT_EQ(GenerateCrc32C(k32Ones), 0x43aba862U);
  EXPECT_EQ(GenerateCrc32C(k32Incrementing), 0x4e79dd46U);
  EXPECT_EQ(GenerateCrc32C(k32Decrementing), 0x5cdb3f11U);
  EXPECT_EQ(GenerateCrc32C(kISCSICommandPDU), 0x563a96d9U);
}

}  // namespace
}  // namespace dcsctp
