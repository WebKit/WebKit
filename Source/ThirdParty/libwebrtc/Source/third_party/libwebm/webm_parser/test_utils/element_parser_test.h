// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef TEST_UTILS_ELEMENT_PARSER_TEST_H_
#define TEST_UTILS_ELEMENT_PARSER_TEST_H_

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

#include "test_utils/limited_reader.h"
#include "test_utils/parser_test.h"
#include "webm/buffer_reader.h"
#include "webm/reader.h"
#include "webm/status.h"

namespace webm {

// Base class for unit tests that test an instance of the ElementParser
// inteface. The template parameter T is the parser class being tested, and the
// optional id is the element ID associated with elements from the parser.
template <typename T, Id id = static_cast<Id>(0)>
class ElementParserTest : public ParserTest<T> {
 public:
  // Sets the reader's internal buffer to the given buffer and metadata_ to
  // data.size().
  void SetReaderData(std::vector<std::uint8_t> data) override {
    metadata_.size = data.size();
    ParserTest<T>::SetReaderData(std::move(data));
  }

  // Sets metadata_.size to size and then calls Init() on the parser, ensuring
  // that it returns the expected status code.
  void TestInit(std::uint64_t size, Status::Code expected) {
    metadata_.size = size;
    const Status status = parser_.Init(metadata_, metadata_.size);
    ASSERT_EQ(expected, status.code);
  }

  // Similar to the base class implementation, but with the difference that
  // Init() is also called (after setting metadata_.size to size).
  void ParseAndVerify(std::uint64_t size) override {
    TestInit(size, Status::kOkCompleted);

    std::uint64_t num_bytes_read = 0;
    const Status status = parser_.Feed(&callback_, &reader_, &num_bytes_read);
    ASSERT_EQ(Status::kOkCompleted, status.code);

    if (size != kUnknownElementSize) {
      ASSERT_EQ(size, num_bytes_read);
    }
  }

  void ParseAndVerify() override { ParseAndVerify(metadata_.size); }

  void IncrementalParseAndVerify() override {
    TestInit(metadata_.size, Status::kOkCompleted);

    webm::LimitedReader limited_reader(
        std::unique_ptr<webm::Reader>(new BufferReader(std::move(reader_))));

    Status status;
    std::uint64_t num_bytes_read = 0;
    do {
      limited_reader.set_total_read_skip_limit(1);
      std::uint64_t local_num_bytes_read = 0;
      status = parser_.Feed(&callback_, &limited_reader, &local_num_bytes_read);
      num_bytes_read += local_num_bytes_read;
      ASSERT_GE(static_cast<std::uint64_t>(1), local_num_bytes_read);
    } while (status.code == Status::kWouldBlock ||
             status.code == Status::kOkPartial);

    ASSERT_EQ(Status::kOkCompleted, status.code);

    if (metadata_.size != kUnknownElementSize) {
      ASSERT_EQ(metadata_.size, num_bytes_read);
    }
  }

  // Initializes the parser (after setting metadata_.size to size), ensures it
  // succeeds, and then calls Feed() on the parser, making sure it returns the
  // expected status code.
  void ParseAndExpectResult(Status::Code expected, std::uint64_t size) {
    TestInit(size, Status::kOkCompleted);

    std::uint64_t num_bytes_read = 0;
    const Status status = parser_.Feed(&callback_, &reader_, &num_bytes_read);
    ASSERT_EQ(expected, status.code);
  }

  // Initializes the parser, ensures it succeeds, and then calls Feed() on the
  // parser, making sure it returns the expected status code.
  void ParseAndExpectResult(Status::Code expected) override {
    ParseAndExpectResult(expected, metadata_.size);
  }

 protected:
  using ParserTest<T>::callback_;
  using ParserTest<T>::parser_;
  using ParserTest<T>::reader_;

  // Element metadata associated with the element parsed by parser_. This is
  // passed to Init() when initializing the parser.
  ElementMetadata metadata_ = {id, 0, 0, 0};
};

}  // namespace webm

#endif  // TEST_UTILS_ELEMENT_PARSER_TEST_H_
