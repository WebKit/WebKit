// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm/istream_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <type_traits>

#include "gtest/gtest.h"

using webm::IstreamReader;
using webm::Status;

namespace {

// Creates a std::array from a string literal (and omits the trailing
// NUL-character).
template <std::size_t N>
std::array<std::uint8_t, N - 1> ArrayFromString(const char (&string)[N]) {
  std::array<std::uint8_t, N - 1> array;
  std::copy_n(string, N - 1, array.begin());
  return array;
}

class IstreamReaderTest : public testing::Test {};

TEST_F(IstreamReaderTest, Read) {
  std::array<std::uint8_t, 15> buffer{};
  std::uint64_t count;
  Status status;

  IstreamReader reader =
      IstreamReader::Emplace<std::istringstream>("abcdefghij");

  status = reader.Read(5, buffer.data(), &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(5), count);

  status = reader.Read(10, buffer.data() + 5, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(5), count);

  std::array<std::uint8_t, 15> expected =
      ArrayFromString("abcdefghij\0\0\0\0\0");
  EXPECT_EQ(expected, buffer);

  status = reader.Read(buffer.size(), buffer.data(), &count);
  EXPECT_EQ(Status::kEndOfFile, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), count);
}

TEST_F(IstreamReaderTest, Skip) {
  std::uint64_t count;
  Status status;

  IstreamReader reader =
      IstreamReader::Emplace<std::istringstream>("abcdefghij");

  status = reader.Skip(3, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(3), count);

  status = reader.Skip(10, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(7), count);

  status = reader.Skip(1, &count);
  EXPECT_EQ(Status::kEndOfFile, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), count);
}

TEST_F(IstreamReaderTest, ReadAndSkip) {
  std::array<std::uint8_t, 10> buffer = {};
  std::uint64_t count;
  Status status;

  IstreamReader reader =
      IstreamReader::Emplace<std::istringstream>("AaBbCcDdEe");

  status = reader.Read(5, buffer.data(), &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(5), count);

  status = reader.Skip(3, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(3), count);

  status = reader.Read(5, buffer.data() + 5, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);

  std::array<std::uint8_t, 10> expected = ArrayFromString("AaBbCEe\0\0\0");
  EXPECT_EQ(expected, buffer);
}

TEST_F(IstreamReaderTest, Position) {
  std::array<std::uint8_t, 10> buffer = {};
  std::uint64_t count;
  Status status;

  IstreamReader reader =
      IstreamReader::Emplace<std::istringstream>("AaBbCcDdEe");
  EXPECT_EQ(static_cast<std::uint64_t>(0), reader.Position());

  status = reader.Read(5, buffer.data(), &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(5), count);
  EXPECT_EQ(static_cast<std::uint64_t>(5), reader.Position());

  status = reader.Skip(3, &count);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(3), count);
  EXPECT_EQ(static_cast<std::uint64_t>(8), reader.Position());

  status = reader.Read(5, buffer.data() + 5, &count);
  EXPECT_EQ(Status::kOkPartial, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(2), count);
  EXPECT_EQ(static_cast<std::uint64_t>(10), reader.Position());

  std::array<std::uint8_t, 10> expected = ArrayFromString("AaBbCEe\0\0\0");
  EXPECT_EQ(expected, buffer);
}

}  // namespace
