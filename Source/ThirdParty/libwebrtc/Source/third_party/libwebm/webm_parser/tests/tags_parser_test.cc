// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/tags_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/buffer_reader.h"

using webm::ElementParserTest;
using webm::Id;
using webm::TagsParser;

namespace {

class TagsParserTest : public ElementParserTest<TagsParser, Id::kTags> {};

// TODO(mjbshaw): validate results via Callback.

TEST_F(TagsParserTest, DefaultValues) {
  ParseAndVerify();

  SetReaderData({
      0x73, 0x73,  // ID = 0x7373 (Tag).
      0x80,  // Size = 0.
  });
  ParseAndVerify();
}

TEST_F(TagsParserTest, RepeatedValues) {
  SetReaderData({
      0x73, 0x73,  // ID = 0x7373 (Tag).
      0x87,  // Size = 7.

      0x63, 0xC0,  //   ID = 0x63C0 (Targets).
      0x84,  //   Size = 4.

      0x68, 0xCA,  //     ID = 0x68CA (TargetTypeValue).
      0x81,  //     Size = 1.
      0x01,  //     Body (value = 1).

      0x73, 0x73,  // ID = 0x7373 (Tag).
      0x87,  // Size = 7.

      0x63, 0xC0,  //   ID = 0x63C0 (Targets).
      0x84,  //   Size = 4.

      0x68, 0xCA,  //     ID = 0x68CA (TargetTypeValue).
      0x81,  //     Size = 1.
      0x02,  //     Body (value = 2).
  });

  ParseAndVerify();
}

}  // namespace
