// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/info_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::Info;
using webm::InfoParser;

namespace {

class InfoParserTest : public ElementParserTest<InfoParser, Id::kInfo> {};

TEST_F(InfoParserTest, DefaultParse) {
  EXPECT_CALL(callback_, OnInfo(metadata_, Info{})).Times(1);

  ParseAndVerify();
}

TEST_F(InfoParserTest, DefaultValues) {
  SetReaderData({
      0x2A, 0xD7, 0xB1,  // ID = 0x2AD7B1 (TimecodeScale).
      0x80,  // Size = 0.

      0x44, 0x89,  // ID = 0x4489 (Duration).
      0x20, 0x00, 0x00,  // Size = 0.

      0x44, 0x61,  // ID = 0x4461 (DateUTC).
      0x20, 0x00, 0x00,  // Size = 0.

      0x7B, 0xA9,  // ID = 0x7BA9 (Title).
      0x20, 0x00, 0x00,  // Size = 0.

      0x4D, 0x80,  // ID = 0x4D80 (MuxingApp).
      0x20, 0x00, 0x00,  // Size = 0.

      0x57, 0x41,  // ID = 0x5741 (WritingApp).
      0x20, 0x00, 0x00,  // Size = 0.
  });

  Info info;
  info.timecode_scale.Set(1000000, true);
  info.duration.Set(0, true);
  info.date_utc.Set(0, true);
  info.title.Set("", true);
  info.muxing_app.Set("", true);
  info.writing_app.Set("", true);

  EXPECT_CALL(callback_, OnInfo(metadata_, info)).Times(1);

  ParseAndVerify();
}

TEST_F(InfoParserTest, CustomValues) {
  SetReaderData({
      0x2A, 0xD7, 0xB1,  // ID = 0x2AD7B1 (TimecodeScale).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0x44, 0x89,  // ID = 0x4489 (Duration).
      0x84,  // Size = 4.
      0x4D, 0x8E, 0xF3, 0xC2,  // Body (value = 299792448.0f).

      0x44, 0x61,  // ID = 0x4461 (DateUTC).
      0x88,  // Size = 8.
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Body (value = -1).

      0x7B, 0xA9,  // ID = 0x7BA9 (Title).
      0x10, 0x00, 0x00, 0x03,  // Size = 3.
      0x66, 0x6F, 0x6F,  // Body (value = "foo").

      0x4D, 0x80,  // ID = 0x4D80 (MuxingApp).
      0x10, 0x00, 0x00, 0x03,  // Size = 3.
      0x62, 0x61, 0x72,  // Body (value = "bar").

      0x57, 0x41,  // ID = 0x5741 (WritingApp).
      0x10, 0x00, 0x00, 0x03,  // Size = 3.
      0x62, 0x61, 0x7A,  // Body (value = "baz").
  });

  Info info;
  info.timecode_scale.Set(1, true);
  info.duration.Set(299792448.0f, true);
  info.date_utc.Set(-1, true);
  info.title.Set("foo", true);
  info.muxing_app.Set("bar", true);
  info.writing_app.Set("baz", true);

  EXPECT_CALL(callback_, OnInfo(metadata_, info)).Times(1);

  ParseAndVerify();
}

}  // namespace
