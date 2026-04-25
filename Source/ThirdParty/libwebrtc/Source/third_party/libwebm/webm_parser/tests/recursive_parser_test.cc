// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/recursive_parser.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "src/byte_parser.h"
#include "src/element_parser.h"
#include "test_utils/element_parser_test.h"
#include "webm/element.h"
#include "webm/status.h"

using webm::Callback;
using webm::ElementMetadata;
using webm::ElementParser;
using webm::ElementParserTest;
using webm::Reader;
using webm::RecursiveParser;
using webm::Status;
using webm::StringParser;

namespace {

class FailParser : public ElementParser {
 public:
  explicit FailParser(std::size_t /* max_recursion_depth */) { ADD_FAILURE(); }

  Status Init(const ElementMetadata& /* metadata */,
              std::uint64_t /* max_size */) override {
    ADD_FAILURE();
    return Status(Status::kInvalidElementSize);
  }

  Status Feed(Callback* /* callback */, Reader* /* reader */,
              std::uint64_t* num_bytes_read) override {
    ADD_FAILURE();
    *num_bytes_read = 0;
    return Status(Status::kInvalidElementSize);
  }

  int value() const {
    ADD_FAILURE();
    return 0;
  }

  int* mutable_value() {
    ADD_FAILURE();
    return nullptr;
  }
};

class StringParserWrapper : public StringParser {
 public:
  explicit StringParserWrapper(std::size_t max_recursion_depth) {
    EXPECT_EQ(max_recursion_depth, 24);
  }
};

class RecursiveFailParserTest
    : public ElementParserTest<RecursiveParser<FailParser>> {};

TEST_F(RecursiveFailParserTest, NoConstruction) {
  RecursiveParser<FailParser> parser;
}

class RecursiveStringParserTest
    : public ElementParserTest<RecursiveParser<StringParserWrapper>> {};

TEST_F(RecursiveStringParserTest, ParsesOkay) {
  ParseAndVerify();
  EXPECT_EQ("", parser_.value());

  SetReaderData({0x48, 0x69});  // "Hi".
  ParseAndVerify();
  EXPECT_EQ("Hi", parser_.value());
}

TEST_F(RecursiveStringParserTest, ExceedMaxRecursionDepth) {
  ResetParser(0);
  TestInit(0, Status::kExceededRecursionDepthLimit);
}

}  // namespace
