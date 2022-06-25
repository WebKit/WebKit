// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/chapter_atom_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"
#include "webm/status.h"

using webm::ChapterAtom;
using webm::ChapterAtomParser;
using webm::ChapterDisplay;
using webm::ElementParserTest;
using webm::Id;
using webm::Status;

namespace {

class ChapterAtomParserTest
    : public ElementParserTest<ChapterAtomParser, Id::kChapterAtom> {};

TEST_F(ChapterAtomParserTest, DefaultParse) {
  ParseAndVerify();

  const ChapterAtom chapter_atom = parser_.value();

  EXPECT_FALSE(chapter_atom.uid.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.uid.value());

  EXPECT_FALSE(chapter_atom.string_uid.is_present());
  EXPECT_EQ("", chapter_atom.string_uid.value());

  EXPECT_FALSE(chapter_atom.time_start.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.time_start.value());

  EXPECT_FALSE(chapter_atom.time_end.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.time_end.value());

  EXPECT_EQ(static_cast<std::size_t>(0), chapter_atom.displays.size());

  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.atoms.size());
}

TEST_F(ChapterAtomParserTest, DefaultValues) {
  SetReaderData({
      0x73, 0xC4,  // ID = 0x73C4 (ChapterUID).
      0x80,  // Size = 0.

      0x56, 0x54,  // ID = 0x73C4 (ChapterStringUID).
      0x80,  // Size = 0.

      0x91,  // ID = 0x91 (ChapterTimeStart).
      0x40, 0x00,  // Size = 0.

      0x92,  // ID = 0x91 (ChapterTimeEnd).
      0x40, 0x00,  // Size = 0.

      0x80,  // ID = 0x80 (ChapterDisplay).
      0x40, 0x00,  // Size = 0.

      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x40, 0x00,  // Size = 0.
  });

  ParseAndVerify();

  const ChapterAtom chapter_atom = parser_.value();

  EXPECT_TRUE(chapter_atom.uid.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.uid.value());

  EXPECT_TRUE(chapter_atom.string_uid.is_present());
  EXPECT_EQ("", chapter_atom.string_uid.value());

  EXPECT_TRUE(chapter_atom.time_start.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.time_start.value());

  EXPECT_TRUE(chapter_atom.time_end.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), chapter_atom.time_end.value());

  ASSERT_EQ(static_cast<std::uint64_t>(1), chapter_atom.displays.size());
  EXPECT_TRUE(chapter_atom.displays[0].is_present());
  EXPECT_EQ(ChapterDisplay{}, chapter_atom.displays[0].value());

  ASSERT_EQ(static_cast<std::uint64_t>(1), chapter_atom.atoms.size());
  EXPECT_TRUE(chapter_atom.atoms[0].is_present());
  EXPECT_EQ(ChapterAtom{}, chapter_atom.atoms[0].value());
}

TEST_F(ChapterAtomParserTest, CustomValues) {
  SetReaderData({
      0x73, 0xC4,  // ID = 0x73C4 (ChapterUID).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0x56, 0x54,  // ID = 0x73C4 (ChapterStringUID).
      0x81,  // Size = 1.
      0x41,  // Body (value = "A").

      0x91,  // ID = 0x91 (ChapterTimeStart).
      0x40, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0x92,  // ID = 0x91 (ChapterTimeEnd).
      0x40, 0x01,  // Size = 1.
      0x03,  // Body (value = 3).

      0x80,  // ID = 0x80 (ChapterDisplay).
      0x40, 0x04,  // Size = 4.

      0x85,  //   ID = 0x85 (ChapString).
      0x40, 0x01,  //   Size = 1.
      0x42,  //   Body (value = "B").

      0x80,  // ID = 0x80 (ChapterDisplay).
      0x40, 0x04,  // Size = 4.

      0x85,  //   ID = 0x85 (ChapString).
      0x40, 0x01,  //   Size = 1.
      0x43,  //   Body (value = "C").

      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x40, 0x12,  // Size = 18.

      0x73, 0xC4,  //   ID = 0x73C4 (ChapterUID).
      0x81,  //   Size = 1.
      0x04,  //   Body (value = 4).

      0xB6,  //   ID = 0xB6 (ChapterAtom).
      0x40, 0x04,  //   Size = 4.

      0x73, 0xC4,  //     ID = 0x73C4 (ChapterUID).
      0x81,  //     Size = 1.
      0x05,  //     Body (value = 5).

      0xB6,  //   ID = 0xB6 (ChapterAtom).
      0x40, 0x04,  //   Size = 4.

      0x73, 0xC4,  //     ID = 0x73C4 (ChapterUID).
      0x81,  //     Size = 1.
      0x06,  //     Body (value = 6).
  });

  ParseAndVerify();

  const ChapterAtom chapter_atom = parser_.value();

  EXPECT_TRUE(chapter_atom.uid.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), chapter_atom.uid.value());

  EXPECT_TRUE(chapter_atom.string_uid.is_present());
  EXPECT_EQ("A", chapter_atom.string_uid.value());

  EXPECT_TRUE(chapter_atom.time_start.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), chapter_atom.time_start.value());

  EXPECT_TRUE(chapter_atom.time_end.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(3), chapter_atom.time_end.value());

  ChapterDisplay expected_chapter_display;

  ASSERT_EQ(static_cast<std::size_t>(2), chapter_atom.displays.size());
  expected_chapter_display.string.Set("B", true);
  EXPECT_TRUE(chapter_atom.displays[0].is_present());
  EXPECT_EQ(expected_chapter_display, chapter_atom.displays[0].value());
  expected_chapter_display.string.Set("C", true);
  EXPECT_TRUE(chapter_atom.displays[1].is_present());
  EXPECT_EQ(expected_chapter_display, chapter_atom.displays[1].value());

  ChapterAtom expected_chapter_atom;
  expected_chapter_atom.uid.Set(4, true);

  ChapterAtom tmp_atom{};
  tmp_atom.uid.Set(5, true);
  expected_chapter_atom.atoms.emplace_back(tmp_atom, true);
  tmp_atom.uid.Set(6, true);
  expected_chapter_atom.atoms.emplace_back(tmp_atom, true);

  ASSERT_EQ(static_cast<std::size_t>(1), chapter_atom.atoms.size());
  EXPECT_TRUE(chapter_atom.atoms[0].is_present());
  EXPECT_EQ(expected_chapter_atom, chapter_atom.atoms[0].value());
}

TEST_F(ChapterAtomParserTest, ExceedMaxRecursionDepth) {
  ResetParser(1);

  SetReaderData({
      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x80,  // Size = 0.
  });
  ParseAndVerify();

  SetReaderData({
      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x82,  // Size = 2.

      0xB6,  //   ID = 0xB6 (ChapterAtom).
      0x80,  //   Size = 0.
  });
  ParseAndExpectResult(Status::kExceededRecursionDepthLimit);
}

}  // namespace
