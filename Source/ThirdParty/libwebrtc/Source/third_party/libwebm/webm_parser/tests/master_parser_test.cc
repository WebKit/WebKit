// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/master_parser.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/byte_parser.h"
#include "test_utils/element_parser_test.h"
#include "webm/element.h"
#include "webm/id.h"
#include "webm/status.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

using webm::Action;
using webm::BinaryParser;
using webm::ElementMetadata;
using webm::ElementParser;
using webm::ElementParserTest;
using webm::Id;
using webm::kUnknownElementSize;
using webm::LimitedReader;
using webm::MasterParser;
using webm::Status;

namespace {

// Simple helper method that just takes an Id and ElementParser* and returns
// them in a std::pair<Id, std::unique_ptr<ElementParser>>. Provided just for
// simplifying some statements.
std::pair<Id, std::unique_ptr<ElementParser>> ParserForId(
    Id id, ElementParser* parser) {
  return {id, std::unique_ptr<ElementParser>(parser)};
}

class MasterParserTest : public ElementParserTest<MasterParser> {};

// Errors parsing an ID should be returned to the caller.
TEST_F(MasterParserTest, BadId) {
  SetReaderData({
      0x00,  // Invalid ID.
      0x80,  // ID = 0x80 (unknown).
      0x80,  // Size = 0.
  });

  EXPECT_CALL(callback_, OnElementBegin(_, _)).Times(0);

  ParseAndExpectResult(Status::kInvalidElementId);
}

// Errors from a child parser's Init should be returned to the caller.
TEST_F(MasterParserTest, ChildInitFails) {
  SetReaderData({
      0xEC,  // ID = 0xEC (Void).
      0xFF,  // Size = unknown.
  });

  const ElementMetadata metadata = {Id::kVoid, 2, kUnknownElementSize, 0};
  EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

  ParseAndExpectResult(Status::kInvalidElementSize);
}

// Indefinite unknown children should result in an error.
TEST_F(MasterParserTest, IndefiniteUnknownChild) {
  SetReaderData({
      0x80,  // ID = 0x80 (unknown).
      0xFF,  // Size = unknown.
      0x00, 0x00,  // Body.
  });

  EXPECT_CALL(callback_, OnElementBegin(_, _)).Times(0);

  ParseAndExpectResult(Status::kIndefiniteUnknownElement);
}

// Child elements that overflow the master element's size should be detected.
TEST_F(MasterParserTest, ChildOverflow) {
  SetReaderData({
      0xEC,  // ID = 0xEC (Void).
      0x82,  // Size = 2.
      0x00, 0x00,  // Body.
  });

  EXPECT_CALL(callback_, OnElementBegin(_, _)).Times(0);
  EXPECT_CALL(callback_, OnVoid(_, _, _)).Times(0);

  ParseAndExpectResult(Status::kElementOverflow, reader_.size() - 1);
}

// Child elements with an unknown size can't be naively checked to see if they
// overflow the master element's size. Make sure the overflow is still detected.
TEST_F(MasterParserTest, ChildOverflowWithUnknownSize) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block) (master).
      0xFF,  // Size = unknown.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x12,  //   Body.
  });

  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kBlock, 2, kUnknownElementSize, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
  }

  ResetParser(ParserForId(Id::kBlock, new MasterParser));

  ParseAndExpectResult(Status::kElementOverflow, 4);
}

// An element with an unknown size should be terminated by its parents bounds.
TEST_F(MasterParserTest, ChildWithUnknownSizeBoundedByParentSize) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block) (master).
      0xFF,  // Size = unknown.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x12,  //   Body.

      0x00,  // Invalid ID. This should not be read.
  });

  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kBlock, 2, kUnknownElementSize, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kVoid, 2, 1, 2};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(1);
  }

  ResetParser(ParserForId(Id::kBlock, new MasterParser));

  ParseAndVerify(reader_.size() - 1);
}

// An empty master element is okay.
TEST_F(MasterParserTest, Empty) {
  EXPECT_CALL(callback_, OnElementBegin(_, _)).Times(0);

  ParseAndVerify();
}

TEST_F(MasterParserTest, DefaultActionIsRead) {
  SetReaderData({
      0xEC,  // ID = 0xEC (Void).
      0x80,  // Size = 0.
  });

  {
    InSequence dummy;

    const ElementMetadata metadata = {Id::kVoid, 2, 0, 0};

    // This intentionally does not set the action and relies on the parser using
    // a default action value of kRead.
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull()))
        .WillOnce(Return(Status(Status::kOkCompleted)));
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(1);
  }

  ParseAndVerify();
}

// Unrecognized children should be skipped over.
TEST_F(MasterParserTest, UnknownChildren) {
  SetReaderData({
      0x40, 0x00,  // ID = 0x4000 (unknown).
      0x80,  // Size = 0.

      0x80,  // ID = 0x80 (unknown).
      0x40, 0x00,  // Size = 0.
  });

  EXPECT_CALL(callback_, OnVoid(_, _, _)).Times(0);
  {
    InSequence dummy;

    ElementMetadata metadata = {static_cast<Id>(0x4000), 3, 0, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {static_cast<Id>(0x80), 3, 0, 3};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
  }

  ParseAndVerify();
}

// A master element with unknown size is terminated by the first element that is
// not a valid child.
TEST_F(MasterParserTest, UnknownSize) {
  SetReaderData({
      // Void elements may appear anywhere in a master element and should not
      // terminate the parse for a master element with an unknown size. In other
      // words, they're always valid children.
      0xEC,  // ID = 0xEC (Void).
      0x81,  // Size = 1.
      0x00,  // Body.

      // This element marks the end for the parser since this is the first
      // unrecognized element. The ID and size should be read (which the parser
      // uses to determine the end has been reached), but nothing more.
      0x80,  // ID = 0x80 (unknown).
      0x81,  // Size = 1.
      0x12,  // Body.
  });

  {
    InSequence dummy;

    const ElementMetadata metadata = {Id::kVoid, 2, 1, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(1);
  }

  ParseAndVerify(kUnknownElementSize);

  EXPECT_EQ(static_cast<std::uint64_t>(5), reader_.Position());
}

// Consecutive elements with unknown size should parse without issues, despite
// the internal parsers having to read ahead into the next (non-child) element.
TEST_F(MasterParserTest, MultipleUnknownChildSize) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block) (master).
      0xFF,  // Size = unknown.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x12,  //   Body.

      0xA1,  // ID = 0xA1 (Block) (master).
      0xFF,  // Size = unknown.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x13,  //   Body.
  });

  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kBlock, 2, kUnknownElementSize, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kVoid, 2, 1, 2};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(1);

    metadata = {Id::kBlock, 2, kUnknownElementSize, 5};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kVoid, 2, 1, 7};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(1);
  }

  ResetParser(ParserForId(Id::kBlock, new MasterParser));

  ParseAndVerify();
}

// Reaching the end of the file while reading an element with unknown size
// should return Status::kOkCompleted instead of Status::kEndOfFile.
TEST_F(MasterParserTest, UnknownSizeToFileEnd) {
  SetReaderData({
      0xEC,  // ID = 0xEC (Void).
      0x81,  // Size = 1.
      0x00,  // Body.
  });

  {
    InSequence dummy;

    const ElementMetadata metadata = {Id::kVoid, 2, 1, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(1);
  }

  ParseAndVerify();
}

// Parsing one byte at a time is okay.
TEST_F(MasterParserTest, IncrementalParse) {
  SetReaderData({
      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x08, 0x00, 0x00, 0x00, 0x06,  // Size = 6.
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // Body.
  });

  const ElementMetadata metadata = {Id::kEbml, 9, 6, 0};
  EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

  BinaryParser* binary_parser = new BinaryParser;
  ResetParser(ParserForId(Id::kEbml, binary_parser));

  IncrementalParseAndVerify();

  std::vector<std::uint8_t> expected = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  EXPECT_EQ(expected, binary_parser->value());
}

// Alternating actions between skip and read is okay. The parser should remember
// the requested action between repeated calls to Feed.
TEST_F(MasterParserTest, IncrementalSkipThenReadThenSkip) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block) (master).
      0x83,  // Size = 3.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x12,  //   Body.

      0xA1,  // ID = 0xA1 (Block) (master).
      0x83,  // Size = 3.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x13,  //   Body.

      0xA1,  // ID = 0xA1 (Block) (master).
      0xFF,  // Size = unknown.

      0xEC,  //   ID = 0xEC (Void) (child).
      0x81,  //   Size = 1.
      0x14,  //   Body.
  });

  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kBlock, 2, 3, 0};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoAll(SetArgPointee<1>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    metadata = {Id::kBlock, 2, 3, 5};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kVoid, 2, 1, 7};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    // Expect to get called twice because we'll cap the LimitedReader to 1-byte
    // reads. The first attempt to read will fail because we'll have already
    // reached the 1-byte max.
    EXPECT_CALL(callback_, OnVoid(metadata, NotNull(), NotNull())).Times(2);

    metadata = {Id::kBlock, 2, kUnknownElementSize, 10};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull()))
        .WillOnce(DoAll(SetArgPointee<1>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));
  }

  ResetParser(ParserForId(Id::kBlock, new MasterParser));

  IncrementalParseAndVerify();
}

}  // namespace
