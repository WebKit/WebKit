// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/block_group_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"
#include "webm/status.h"

using testing::_;
using testing::InSequence;
using testing::NotNull;
using testing::Return;

using webm::Block;
using webm::BlockAdditions;
using webm::BlockGroup;
using webm::BlockGroupParser;
using webm::BlockMore;
using webm::ElementParserTest;
using webm::Id;
using webm::Slices;
using webm::Status;
using webm::TimeSlice;
using webm::VirtualBlock;

namespace {

class BlockGroupParserTest
    : public ElementParserTest<BlockGroupParser, Id::kBlockGroup> {};

TEST_F(BlockGroupParserTest, InvalidBlock) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block).
      0x80,  // Size = 0.
  });

  EXPECT_CALL(callback_, OnBlockGroupBegin(_, _)).Times(1);
  EXPECT_CALL(callback_, OnBlockGroupEnd(_, _)).Times(0);

  ParseAndExpectResult(Status::kInvalidElementSize);
}

TEST_F(BlockGroupParserTest, InvalidVirtualBlock) {
  SetReaderData({
      0xA2,  // ID = 0xA2 (BlockVirtual).
      0x80,  // Size = 0.
  });

  EXPECT_CALL(callback_, OnBlockGroupBegin(_, _)).Times(1);
  EXPECT_CALL(callback_, OnBlockGroupEnd(_, _)).Times(0);

  ParseAndExpectResult(Status::kInvalidElementSize);
}

TEST_F(BlockGroupParserTest, DefaultParse) {
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnBlockGroupBegin(metadata_, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnBlockGroupEnd(metadata_, BlockGroup{})).Times(1);
  }

  ParseAndVerify();
}

TEST_F(BlockGroupParserTest, DefaultActionIsRead) {
  {
    InSequence dummy;

    // This intentionally does not set the action and relies on the parser using
    // a default action value of kRead.
    EXPECT_CALL(callback_, OnBlockGroupBegin(metadata_, NotNull()))
        .WillOnce(Return(Status(Status::kOkCompleted)));
    EXPECT_CALL(callback_, OnBlockGroupEnd(metadata_, BlockGroup{})).Times(1);
  }

  ParseAndVerify();
}

TEST_F(BlockGroupParserTest, DefaultValues) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block).
      0x85,  // Size = 5.
      0x81,  // Track number = 1.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.
      0x00,  // Frame 0.

      0xA2,  // ID = 0xA2 (BlockVirtual).
      0x84,  // Size = 4.
      0x81,  // Track number = 1.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.

      0x75, 0xA1,  // ID = 0x75A1 (BlockAdditions).
      0x80,  // Size = 0.

      0x9B,  // ID = 0x9B (BlockDuration).
      0x40, 0x00,  // Size = 0.

      0xFB,  // ID = 0xFB (ReferenceBlock).
      0x40, 0x00,  // Size = 0.

      0x75, 0xA2,  // ID = 0x75A2 (DiscardPadding).
      0x80,  // Size = 0.

      0x8E,  // ID = 0x8E (Slices).
      0x40, 0x00,  // Size = 0.
  });

  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnBlockGroupBegin(metadata_, NotNull())).Times(1);

    BlockGroup block_group;
    // Blocks and VirtualBlocks don't have any kind of default defined, so they
    // can't have an element state of kPresentAsDefault.
    Block block{};
    block.track_number = 1;
    block.num_frames = 1;
    block.is_visible = true;
    block_group.block.Set(block, true);

    EXPECT_CALL(callback_, OnBlockBegin(_, block, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnFrame(_, NotNull(), NotNull())).Times(1);
    EXPECT_CALL(callback_, OnBlockEnd(_, block_group.block.value())).Times(1);

    VirtualBlock virtual_block{};
    virtual_block.track_number = 1;
    block_group.virtual_block.Set(virtual_block, true);

    block_group.additions.Set({}, true);
    block_group.duration.Set(0, true);
    block_group.references.emplace_back(0, true);
    block_group.discard_padding.Set(0, true);
    block_group.slices.Set({}, true);

    EXPECT_CALL(callback_, OnBlockGroupEnd(metadata_, block_group)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(BlockGroupParserTest, CustomValues) {
  SetReaderData({
      0xA1,  // ID = 0xA1 (Block).
      0x85,  // Size = 5.
      0x82,  // Track number = 2.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.
      0x00,  // Frame 0.

      0xA2,  // ID = 0xA2 (BlockVirtual).
      0x84,  // Size = 4.
      0x83,  // Track number = 3.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.

      0x75, 0xA1,  // ID = 0x75A1 (BlockAdditions).
      0x83,  // Size = 3.

      0xA6,  //   ID = 0xA6 (BlockMore).
      0x40, 0x00,  //   Size = 0.

      0x9B,  // ID = 0x9B (BlockDuration).
      0x40, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0xFB,  // ID = 0xFB (ReferenceBlock).
      0x40, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0xFB,  // ID = 0xFB (ReferenceBlock).
      0x40, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0x75, 0xA2,  // ID = 0x75A2 (DiscardPadding).
      0x81,  // Size = 1.
      0xFF,  // Body (value = -1).

      0x8E,  // ID = 0x8E (Slices).
      0x40, 0x03,  // Size = 3.

      0xE8,  //   ID = 0xE8 (TimeSlice).
      0x40, 0x00,  //   Size = 0.
  });

  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnBlockGroupBegin(metadata_, NotNull())).Times(1);

    BlockGroup block_group;
    // Blocks and VirtualBlocks don't have any kind of default defined, so they
    // can't have an element state of kPresentAsDefault.
    Block block{};
    block.track_number = 2;
    block.num_frames = 1;
    block.is_visible = true;
    block_group.block.Set(block, true);

    VirtualBlock virtual_block{};
    virtual_block.track_number = 3;
    block_group.virtual_block.Set(virtual_block, true);

    BlockAdditions block_additions;
    block_additions.block_mores.emplace_back(BlockMore{}, true);
    block_group.additions.Set(block_additions, true);

    block_group.duration.Set(1, true);
    block_group.references.emplace_back(1, true);
    block_group.references.emplace_back(2, true);
    block_group.discard_padding.Set(-1, true);

    Slices slices;
    slices.slices.emplace_back(TimeSlice{}, true);
    block_group.slices.Set(slices, true);

    EXPECT_CALL(callback_, OnBlockGroupEnd(metadata_, block_group)).Times(1);
  }

  ParseAndVerify();
}

}  // namespace
