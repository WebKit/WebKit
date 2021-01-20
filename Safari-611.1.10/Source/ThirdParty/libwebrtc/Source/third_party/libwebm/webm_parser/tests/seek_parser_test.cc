// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/seek_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::Seek;
using webm::SeekParser;

namespace {

class SeekParserTest : public ElementParserTest<SeekParser, Id::kSeek> {};

TEST_F(SeekParserTest, DefaultParse) {
  EXPECT_CALL(callback_, OnSeek(metadata_, Seek{})).Times(1);

  ParseAndVerify();
}

TEST_F(SeekParserTest, DefaultValues) {
  SetReaderData({
      // A SeekID element with a length of 0 isn't valid, so we don't test it.

      0x53, 0xAC,  // ID = 0x53AC (SeekPosition).
      0x80,  // Size = 0.
  });

  Seek seek;
  seek.position.Set(0, true);

  EXPECT_CALL(callback_, OnSeek(metadata_, seek)).Times(1);

  ParseAndVerify();
}

TEST_F(SeekParserTest, CustomValues) {
  SetReaderData({
      0x53, 0xAB,  // ID = 0x53AB (SeekID).
      0x81,  // Size = 1.
      0x01,  // Body.

      0x53, 0xAC,  // ID = 0x53AC (SeekPosition).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).
  });

  Seek seek;
  seek.id.Set(static_cast<Id>(0x01), true);
  seek.position.Set(2, true);

  EXPECT_CALL(callback_, OnSeek(metadata_, seek)).Times(1);

  ParseAndVerify();
}

}  // namespace
