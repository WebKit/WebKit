// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/unknown_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/element.h"
#include "webm/status.h"

using testing::NotNull;

using webm::ElementParserTest;
using webm::kUnknownElementSize;
using webm::Status;
using webm::UnknownParser;

namespace {

class UnknownParserTest : public ElementParserTest<UnknownParser> {};

TEST_F(UnknownParserTest, InvalidSize) {
  TestInit(kUnknownElementSize, Status::kIndefiniteUnknownElement);
}

TEST_F(UnknownParserTest, Empty) {
  EXPECT_CALL(callback_, OnUnknownElement(metadata_, NotNull(), NotNull()))
      .Times(1);

  ParseAndVerify();
}

TEST_F(UnknownParserTest, Valid) {
  SetReaderData({0x00, 0x01, 0x02, 0x04});

  EXPECT_CALL(callback_, OnUnknownElement(metadata_, NotNull(), NotNull()))
      .Times(1);

  ParseAndVerify();
}

TEST_F(UnknownParserTest, IncrementalSkip) {
  SetReaderData({0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10});

  EXPECT_CALL(callback_, OnUnknownElement(metadata_, NotNull(), NotNull()))
      .Times(8);

  IncrementalParseAndVerify();
}

}  // namespace
