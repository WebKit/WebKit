// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "test_utils/limited_reader.h"

#include <array>
#include <cstdint>
#include <memory>

#include "gtest/gtest.h"

#include "webm/buffer_reader.h"
#include "webm/reader.h"
#include "webm/status.h"

using webm::BufferReader;
using webm::LimitedReader;
using webm::Reader;
using webm::Status;

namespace {

class LimitedReaderTest : public testing::Test {};

TEST_F(LimitedReaderTest, UnlimitedRead) {
  std::array<std::uint8_t, 4> buffer = {{0, 0, 0, 0}};
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(4), count);

  std::array<std::uint8_t, 4> expected_buffer = {{1, 2, 3, 4}};
  EXPECT_EQ(expected_buffer, buffer);
}

TEST_F(LimitedReaderTest, UnlimitedSkip) {
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  status = reader.Skip(4, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(4), count);
}

TEST_F(LimitedReaderTest, Position) {
  std::array<std::uint8_t, 4> buffer = {{0, 0, 0, 0}};
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  EXPECT_EQ(static_cast<std::uint64_t>(0), reader.Position());

  status = reader.Read(2, buffer.data(), &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);

  status = reader.Skip(2, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);
  EXPECT_EQ(static_cast<std::uint64_t>(4), reader.Position());

  std::array<std::uint8_t, 4> expected_buffer = {{1, 2, 0, 0}};
  EXPECT_EQ(expected_buffer, buffer);
}

TEST_F(LimitedReaderTest, LimitIndividualRead) {
  std::array<std::uint8_t, 4> buffer = {{0, 0, 0, 0}};
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  reader.set_single_read_limit(1);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  status = reader.Read(buffer.size() + 1, buffer.data() + 1, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  reader.set_single_read_limit(2);
  status = reader.Read(buffer.size() - 2, buffer.data() + 2, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);

  std::array<std::uint8_t, 4> expected_buffer = {{1, 2, 3, 4}};
  EXPECT_EQ(expected_buffer, buffer);
}

TEST_F(LimitedReaderTest, LimitIndividualSkip) {
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  reader.set_single_skip_limit(1);
  status = reader.Skip(4, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  status = reader.Skip(3, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  reader.set_single_skip_limit(2);
  status = reader.Skip(2, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);
}

TEST_F(LimitedReaderTest, LimitRepeatedRead) {
  std::array<std::uint8_t, 4> buffer = {{0, 0, 0, 0}};
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  reader.set_total_read_limit(1);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kWouldBlock, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), count);

  reader.set_total_read_limit(2);
  status = reader.Read(buffer.size() - 1, buffer.data() + 1, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);

  reader.set_total_read_limit(1);
  status = reader.Read(buffer.size() - 3, buffer.data() + 3, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  std::array<std::uint8_t, 4> expected_buffer = {{1, 2, 3, 4}};
  EXPECT_EQ(expected_buffer, buffer);
}

TEST_F(LimitedReaderTest, LimitRepeatedSkip) {
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  reader.set_total_skip_limit(1);
  status = reader.Skip(4, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  status = reader.Skip(4, &count);
  EXPECT_EQ(Status::kWouldBlock, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), count);

  reader.set_total_skip_limit(2);
  status = reader.Skip(3, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);

  reader.set_total_skip_limit(1);
  status = reader.Skip(1, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);
}

TEST_F(LimitedReaderTest, LimitReadsAndSkips) {
  std::array<std::uint8_t, 4> buffer = {{0, 0, 0, 0}};
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;

  reader.set_total_read_skip_limit(1);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  status = reader.Skip(buffer.size(), &count);
  EXPECT_EQ(Status::kWouldBlock, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), count);

  reader.set_total_read_skip_limit(2);
  status = reader.Skip(buffer.size() - 1, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);

  reader.set_total_read_skip_limit(1);
  status = reader.Read(buffer.size() - 3, buffer.data() + 3, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), count);

  std::array<std::uint8_t, 4> expected = {{1, 0, 0, 4}};
  EXPECT_EQ(expected, buffer);
}

TEST_F(LimitedReaderTest, CustomStatusWhenBlocked) {
  std::array<std::uint8_t, 4> buffer = {{0, 0, 0, 0}};
  LimitedReader reader(std::unique_ptr<Reader>(new BufferReader({1, 2, 3, 4})));
  std::uint64_t count;
  Status status;
  Status expected_status = Status(-123);
  const std::uint64_t kZeroCount = 0;

  reader.set_single_read_limit(0);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kWouldBlock, status.code);
  EXPECT_EQ(kZeroCount, count);

  reader.set_return_status_when_blocked(expected_status);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(expected_status.code, status.code);
  EXPECT_EQ(kZeroCount, count);

  reader.set_single_read_limit(std::numeric_limits<std::size_t>::max());
  reader.set_total_read_limit(0);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(expected_status.code, status.code);
  EXPECT_EQ(kZeroCount, count);

  reader.set_total_read_limit(std::numeric_limits<std::size_t>::max());
  reader.set_single_skip_limit(0);
  status = reader.Skip(buffer.size(), &count);
  EXPECT_EQ(expected_status.code, status.code);
  EXPECT_EQ(kZeroCount, count);

  reader.set_single_skip_limit(std::numeric_limits<std::uint64_t>::max());
  reader.set_total_skip_limit(0);
  status = reader.Skip(buffer.size(), &count);
  EXPECT_EQ(expected_status.code, status.code);
  EXPECT_EQ(kZeroCount, count);

  reader.set_total_skip_limit(std::numeric_limits<std::uint64_t>::max());
  reader.set_total_read_skip_limit(0);
  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(expected_status.code, status.code);
  EXPECT_EQ(kZeroCount, count);
  status = reader.Skip(buffer.size(), &count);
  EXPECT_EQ(expected_status.code, status.code);
  EXPECT_EQ(kZeroCount, count);

  std::array<std::uint8_t, 4> expected_buffer = {{0, 0, 0, 0}};
  EXPECT_EQ(expected_buffer, buffer);
}

}  // namespace
