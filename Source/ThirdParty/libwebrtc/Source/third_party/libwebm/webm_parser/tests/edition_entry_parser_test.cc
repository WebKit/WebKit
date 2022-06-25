// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/edition_entry_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ChapterAtom;
using webm::EditionEntry;
using webm::EditionEntryParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class EditionEntryParserTest
    : public ElementParserTest<EditionEntryParser, Id::kEditionEntry> {};

TEST_F(EditionEntryParserTest, DefaultParse) {
  EXPECT_CALL(callback_, OnEditionEntry(metadata_, EditionEntry{})).Times(1);

  ParseAndVerify();
}

TEST_F(EditionEntryParserTest, DefaultValues) {
  SetReaderData({
      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x80,  // Size = 0.
  });

  EditionEntry edition_entry;
  edition_entry.atoms.emplace_back();
  edition_entry.atoms[0].Set({}, true);

  EXPECT_CALL(callback_, OnEditionEntry(metadata_, edition_entry)).Times(1);

  ParseAndVerify();
}

TEST_F(EditionEntryParserTest, CustomValues) {
  SetReaderData({
      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x40, 0x04,  // Size = 4.

      0x73, 0xC4,  //   ID = 0x73C4 (ChapterUID).
      0x81,  //   Size = 1.
      0x01,  //   Body (value = 1).

      0xB6,  // ID = 0xB6 (ChapterAtom).
      0x40, 0x04,  // Size = 4.

      0x73, 0xC4,  //   ID = 0x73C4 (ChapterUID).
      0x81,  //   Size = 1.
      0x02,  //   Body (value = 2).
  });

  EditionEntry edition_entry;
  ChapterAtom chapter_atom;
  chapter_atom.uid.Set(1, true);
  edition_entry.atoms.emplace_back(chapter_atom, true);
  chapter_atom.uid.Set(2, true);
  edition_entry.atoms.emplace_back(chapter_atom, true);

  EXPECT_CALL(callback_, OnEditionEntry(metadata_, edition_entry)).Times(1);

  ParseAndVerify();
}

}  // namespace
