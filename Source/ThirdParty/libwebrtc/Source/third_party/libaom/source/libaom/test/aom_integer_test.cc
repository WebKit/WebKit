/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom/aom_integer.h"
#include "gtest/gtest.h"

namespace {
const uint64_t kMaximumLeb128CodedSize = 8;
const uint8_t kLeb128PadByte = 0x80;  // Binary: 10000000
const uint64_t kMaximumLeb128Value = UINT32_MAX;
const uint32_t kSizeTestNumValues = 6;
const uint32_t kSizeTestExpectedSizes[kSizeTestNumValues] = {
  1, 1, 2, 3, 4, 5
};
const uint64_t kSizeTestInputs[kSizeTestNumValues] = { 0,        0x7f,
                                                       0x3fff,   0x1fffff,
                                                       0xffffff, 0x10000000 };

const uint8_t kOutOfRangeLeb128Value[5] = { 0x80, 0x80, 0x80, 0x80,
                                            0x10 };  // UINT32_MAX + 1
}  // namespace

TEST(AomLeb128, DecodeTest) {
  const size_t num_leb128_bytes = 3;
  const uint8_t leb128_bytes[num_leb128_bytes] = { 0xE5, 0x8E, 0x26 };
  const uint64_t expected_value = 0x98765;  // 624485
  const size_t expected_length = 3;
  uint64_t value = ~0ULL;  // make sure value is cleared by the function
  size_t length;
  ASSERT_EQ(
      aom_uleb_decode(&leb128_bytes[0], num_leb128_bytes, &value, &length), 0);
  ASSERT_EQ(expected_value, value);
  ASSERT_EQ(expected_length, length);

  // Make sure the decoder stops on the last marked LEB128 byte.
  aom_uleb_decode(&leb128_bytes[0], num_leb128_bytes + 1, &value, &length);
  ASSERT_EQ(expected_value, value);
  ASSERT_EQ(expected_length, length);
}

TEST(AomLeb128, EncodeTest) {
  const uint32_t test_value = 0x98765;  // 624485
  const uint8_t expected_bytes[3] = { 0xE5, 0x8E, 0x26 };
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(aom_uleb_encode(test_value, kWriteBufferSize, &write_buffer[0],
                            &bytes_written),
            0);
  ASSERT_EQ(bytes_written, 3u);
  for (size_t i = 0; i < bytes_written; ++i) {
    ASSERT_EQ(write_buffer[i], expected_bytes[i]);
  }
}

TEST(AomLeb128, EncodeDecodeTest) {
  const uint32_t value = 0x98765;  // 624485
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(aom_uleb_encode(value, kWriteBufferSize, &write_buffer[0],
                            &bytes_written),
            0);
  ASSERT_EQ(bytes_written, 3u);
  uint64_t decoded_value;
  size_t decoded_length;
  aom_uleb_decode(&write_buffer[0], bytes_written, &decoded_value,
                  &decoded_length);
  ASSERT_EQ(value, decoded_value);
  ASSERT_EQ(bytes_written, decoded_length);
}

TEST(AomLeb128, FixedSizeEncodeTest) {
  const uint32_t test_value = 0x123;
  const uint8_t expected_bytes[4] = { 0xa3, 0x82, 0x80, 0x00 };
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(0, aom_uleb_encode_fixed_size(test_value, kWriteBufferSize,
                                          kWriteBufferSize, &write_buffer[0],
                                          &bytes_written));
  ASSERT_EQ(kWriteBufferSize, bytes_written);
  for (size_t i = 0; i < bytes_written; ++i) {
    ASSERT_EQ(write_buffer[i], expected_bytes[i]);
  }
}

TEST(AomLeb128, FixedSizeEncodeDecodeTest) {
  const uint32_t value = 0x1;
  const size_t kWriteBufferSize = 4;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t bytes_written = 0;
  ASSERT_EQ(
      aom_uleb_encode_fixed_size(value, kWriteBufferSize, kWriteBufferSize,
                                 &write_buffer[0], &bytes_written),
      0);
  ASSERT_EQ(bytes_written, 4u);
  uint64_t decoded_value;
  size_t decoded_length;
  aom_uleb_decode(&write_buffer[0], bytes_written, &decoded_value,
                  &decoded_length);
  ASSERT_EQ(value, decoded_value);
  ASSERT_EQ(bytes_written, decoded_length);
}

TEST(AomLeb128, SizeTest) {
  for (size_t i = 0; i < kSizeTestNumValues; ++i) {
    ASSERT_EQ(kSizeTestExpectedSizes[i],
              aom_uleb_size_in_bytes(kSizeTestInputs[i]));
  }
}

TEST(AomLeb128, DecodeFailTest) {
  // Input buffer containing what would be a valid 9 byte LEB128 encoded
  // unsigned integer.
  const uint8_t kAllPadBytesBuffer[kMaximumLeb128CodedSize + 1] = {
    kLeb128PadByte, kLeb128PadByte, kLeb128PadByte,
    kLeb128PadByte, kLeb128PadByte, kLeb128PadByte,
    kLeb128PadByte, kLeb128PadByte, 0
  };
  uint64_t decoded_value;

  // Test that decode fails when result would be valid 9 byte integer.
  ASSERT_EQ(aom_uleb_decode(&kAllPadBytesBuffer[0], kMaximumLeb128CodedSize + 1,
                            &decoded_value, nullptr),
            -1);

  // Test that encoded value missing terminator byte within available buffer
  // range causes decode error.
  ASSERT_EQ(aom_uleb_decode(&kAllPadBytesBuffer[0], kMaximumLeb128CodedSize,
                            &decoded_value, nullptr),
            -1);

  // Test that LEB128 input that decodes to a value larger than 32-bits fails.
  size_t value_size = 0;
  ASSERT_EQ(aom_uleb_decode(&kOutOfRangeLeb128Value[0],
                            sizeof(kOutOfRangeLeb128Value), &decoded_value,
                            &value_size),
            -1);
}

TEST(AomLeb128, EncodeFailTest) {
  const size_t kWriteBufferSize = 4;
  const uint32_t kValidTestValue = 1;
  uint8_t write_buffer[kWriteBufferSize] = { 0 };
  size_t coded_size = 0;
  ASSERT_EQ(
      aom_uleb_encode(kValidTestValue, kWriteBufferSize, nullptr, &coded_size),
      -1);
  ASSERT_EQ(aom_uleb_encode(kValidTestValue, kWriteBufferSize, &write_buffer[0],
                            nullptr),
            -1);

  const uint32_t kValueOutOfRangeForBuffer = 0xFFFFFFFF;
  ASSERT_EQ(aom_uleb_encode(kValueOutOfRangeForBuffer, kWriteBufferSize,
                            &write_buffer[0], &coded_size),
            -1);

  const uint64_t kValueOutOfRange = kMaximumLeb128Value + 1;
  ASSERT_EQ(aom_uleb_encode(kValueOutOfRange, kWriteBufferSize,
                            &write_buffer[0], &coded_size),
            -1);

  const size_t kPadSizeOutOfRange = 5;
  ASSERT_EQ(aom_uleb_encode_fixed_size(kValidTestValue, kWriteBufferSize,
                                       kPadSizeOutOfRange, &write_buffer[0],
                                       &coded_size),
            -1);
}
