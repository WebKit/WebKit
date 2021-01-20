// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/skip_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/element.h"
#include "webm/status.h"

using webm::ElementParserTest;
using webm::kUnknownElementSize;
using webm::SkipParser;
using webm::Status;

namespace {

class SkipParserTest : public ElementParserTest<SkipParser> {};

TEST_F(SkipParserTest, InvalidSize) {
  TestInit(kUnknownElementSize, Status::kInvalidElementSize);
}

TEST_F(SkipParserTest, Skip) {
  ParseAndVerify();

  SetReaderData({0x00, 0x01, 0x02, 0x04});
  ParseAndVerify();
}

TEST_F(SkipParserTest, IncrementalSkip) {
  SetReaderData({0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10});

  IncrementalParseAndVerify();
}

}  // namespace
