// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/parser_utils.h"

#include <cstdint>
#include <limits>
#include <memory>

#include "gtest/gtest.h"

#include "test_utils/limited_reader.h"
#include "webm/buffer_reader.h"
#include "webm/reader.h"
#include "webm/status.h"

using webm::BufferReader;
using webm::LimitedReader;
using webm::Reader;
using webm::Status;

namespace {

class ParserUtilsTest : public testing::Test {};

TEST_F(ParserUtilsTest, ReadByte) {
  LimitedReader reader(
      std::unique_ptr<Reader>(new BufferReader({0x12, 0x34, 0x56, 0x78})));

  Status status;
  std::uint8_t byte;

  status = ReadByte(&reader, &byte);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(0x12, byte);

  status = ReadByte(&reader, &byte);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(0x34, byte);

  status = ReadByte(&reader, &byte);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(0x56, byte);

  reader.set_total_read_limit(0);
  status = ReadByte(&reader, &byte);
  EXPECT_EQ(Status::kWouldBlock, status.code);
  EXPECT_EQ(0x56, byte);

  reader.set_total_read_limit(std::numeric_limits<std::size_t>::max());
  status = ReadByte(&reader, &byte);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(0x78, byte);

  status = ReadByte(&reader, &byte);
  EXPECT_EQ(Status::kEndOfFile, status.code);
  EXPECT_EQ(0x78, byte);
}

TEST_F(ParserUtilsTest, AccumulateIntegerBytes) {
  LimitedReader reader(
      std::unique_ptr<Reader>(new BufferReader({0x12, 0x34, 0x56, 0x78})));

  std::uint32_t integer = 0;
  Status status;
  std::uint64_t num_bytes_actually_read;

  reader.set_total_read_limit(1);
  status =
      AccumulateIntegerBytes(4, &reader, &integer, &num_bytes_actually_read);
  EXPECT_EQ(Status::kWouldBlock, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), num_bytes_actually_read);
  EXPECT_EQ(static_cast<std::uint32_t>(0x12), integer);

  reader.set_total_read_limit(std::numeric_limits<std::size_t>::max());
  status =
      AccumulateIntegerBytes(3, &reader, &integer, &num_bytes_actually_read);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(3), num_bytes_actually_read);
  EXPECT_EQ(static_cast<std::uint32_t>(0x12345678), integer);

  // Make sure calling with num_bytes_remaining == 0 is a no-op.
  status =
      AccumulateIntegerBytes(0, &reader, &integer, &num_bytes_actually_read);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), num_bytes_actually_read);
  EXPECT_EQ(static_cast<std::uint32_t>(0x12345678), integer);

  status =
      AccumulateIntegerBytes(4, &reader, &integer, &num_bytes_actually_read);
  EXPECT_EQ(Status::kEndOfFile, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), num_bytes_actually_read);
  EXPECT_EQ(static_cast<std::uint32_t>(0x12345678), integer);
}

}  // namespace
