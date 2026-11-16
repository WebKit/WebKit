// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef TEST_UTILS_PARSER_TEST_H_
#define TEST_UTILS_PARSER_TEST_H_

#include <cstdint>
#include <new>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/limited_reader.h"
#include "test_utils/mock_callback.h"
#include "webm/buffer_reader.h"
#include "webm/reader.h"
#include "webm/status.h"

namespace webm {

// Base class for unit tests that test an instance of the Parser inteface. The
// template parameter T is the parser class being tested.
template <typename T>
class ParserTest : public testing::Test {
 public:
  // Sets the reader's internal buffer to the given buffer.
  virtual void SetReaderData(std::vector<std::uint8_t> data) {
    reader_ = BufferReader(std::move(data));
  }

  // Destroys and recreates the parser, forwarding the arguments to the
  // constructor. This is primarily useful for tests that require the parser to
  // have different constructor parameters.
  template <typename... Args>
  void ResetParser(Args&&... args) {
    parser_.~T();
    new (&parser_) T(std::forward<Args>(args)...);
  }

  // Calls Feed() on the parser, making sure it completes successfully and reads
  // size number of bytes.
  virtual void ParseAndVerify(std::uint64_t size) {
    std::uint64_t num_bytes_read = 0;
    const Status status = parser_.Feed(&callback_, &reader_, &num_bytes_read);
    ASSERT_EQ(Status::kOkCompleted, status.code);
    ASSERT_EQ(size, num_bytes_read);
  }

  // Calls Feed() on the parser, making sure it completes successfully and reads
  // all the data available in the reader.
  virtual void ParseAndVerify() { ParseAndVerify(reader_.size()); }

  // Similar to ParseAndVerify(), but instead artificially limits the reader to
  // providing one byte per call to Feed(). If Feed() returns
  // Status::kWouldBlock or Status::kOkPartial, Feed() will be called again
  // (feeding it another byte).
  virtual void IncrementalParseAndVerify() {
    const std::uint64_t expected_num_bytes_read = reader_.size();

    webm::LimitedReader limited_reader(
        std::unique_ptr<webm::Reader>(new BufferReader(std::move(reader_))));

    Status status;
    std::uint64_t num_bytes_read = 0;
    do {
      limited_reader.set_total_read_skip_limit(1);
      std::uint64_t local_num_bytes_read = 0;
      status = parser_.Feed(&callback_, &limited_reader, &local_num_bytes_read);
      num_bytes_read += local_num_bytes_read;
      const std::uint64_t kMinBytesRead = 1;
      ASSERT_GE(kMinBytesRead, local_num_bytes_read);
    } while (status.code == Status::kWouldBlock ||
             status.code == Status::kOkPartial);

    ASSERT_EQ(Status::kOkCompleted, status.code);
    ASSERT_EQ(expected_num_bytes_read, num_bytes_read);
  }

  // Calls Feed() on the parser, making sure it returns the expected status
  // code.
  virtual void ParseAndExpectResult(Status::Code expected) {
    std::uint64_t num_bytes_read = 0;
    const Status status = parser_.Feed(&callback_, &reader_, &num_bytes_read);
    ASSERT_EQ(expected, status.code);
  }

 protected:
  // These members are protected (not private) so unit tests have access to
  // them. This is intentional.

  // The parser that the unit tests will be testing.
  T parser_;

  // The callback that is used during parsing.
  testing::NiceMock<MockCallback> callback_;

  // The reader used for feeding data into the parser.
  BufferReader reader_;
};

}  // namespace webm

#endif  // TEST_UTILS_PARSER_TEST_H_
