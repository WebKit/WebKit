// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/cluster_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/element.h"
#include "webm/id.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

using webm::Action;
using webm::Ancestory;
using webm::BlockGroup;
using webm::Cluster;
using webm::ClusterParser;
using webm::ElementMetadata;
using webm::ElementParserTest;
using webm::Id;
using webm::SimpleBlock;
using webm::Status;

namespace {

class ClusterParserTest
    : public ElementParserTest<ClusterParser, Id::kCluster> {};

TEST_F(ClusterParserTest, DefaultParse) {
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, Cluster{}, NotNull()))
        .Times(1);

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, Cluster{})).Times(1);
  }

  ParseAndVerify();
}

TEST_F(ClusterParserTest, DefaultActionIsRead) {
  {
    InSequence dummy;

    // This intentionally does not set the action and relies on the parser using
    // a default action value of kRead.
    EXPECT_CALL(callback_, OnClusterBegin(metadata_, Cluster{}, NotNull()))
        .WillOnce(Return(Status(Status::kOkCompleted)));
    EXPECT_CALL(callback_, OnClusterEnd(metadata_, Cluster{})).Times(1);
  }

  ParseAndVerify();
}

TEST_F(ClusterParserTest, DefaultValues) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0x40, 0x00,  // Size = 0.

      0xAB,  // ID = 0xAB (PrevSize).
      0x40, 0x00,  // Size = 0.

      0xA3,  // ID = 0xA3 (SimpleBlock).
      0x85,  // Size = 5.
      0x81,  // Track number = 1.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.
      0x00,  // Frame 0.

      0xA0,  // ID = 0xA0 (BlockGroup).
      0x40, 0x00,  // Size = 0.
  });

  {
    InSequence dummy;

    Cluster cluster{};
    cluster.timecode.Set(0, true);
    cluster.previous_size.Set(0, true);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    SimpleBlock simple_block{};
    simple_block.track_number = 1;
    simple_block.num_frames = 1;
    simple_block.is_visible = true;
    cluster.simple_blocks.emplace_back(simple_block, true);

    BlockGroup block_group{};
    cluster.block_groups.emplace_back(block_group, true);

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(ClusterParserTest, CustomValues) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0x40, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0xAB,  // ID = 0xAB (PrevSize).
      0x40, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0xA3,  // ID = 0xA3 (SimpleBlock).
      0x85,  // Size = 5.
      0x81,  // Track number = 1.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.
      0x00,  // Frame 0.

      0xA3,  // ID = 0xA3 (SimpleBlock).
      0x85,  // Size = 5.
      0x82,  // Track number = 2.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.
      0x00,  // Frame 0.

      0xA0,  // ID = 0xA0 (BlockGroup).
      0x40, 0x04,  // Size = 4.

      0x9B,  //   ID = 0x9B (BlockDuration).
      0x40, 0x01,  //   Size = 1.
      0x01,  //   Body (value = 1).

      0xA0,  // ID = 0xA0 (BlockGroup).
      0x40, 0x04,  // Size = 4.

      0x9B,  //   ID = 0x9B (BlockDuration).
      0x40, 0x01,  //   Size = 1.
      0x02,  //   Body (value = 2).
  });

  {
    InSequence dummy;

    Cluster cluster{};
    cluster.timecode.Set(1, true);
    cluster.previous_size.Set(2, true);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    SimpleBlock simple_block{};
    simple_block.num_frames = 1;
    simple_block.is_visible = true;

    simple_block.track_number = 1;
    cluster.simple_blocks.emplace_back(simple_block, true);
    simple_block.track_number = 2;
    cluster.simple_blocks.emplace_back(simple_block, true);

    BlockGroup block_group{};
    block_group.duration.Set(1, true);
    cluster.block_groups.emplace_back(block_group, true);
    block_group.duration.Set(2, true);
    cluster.block_groups.emplace_back(block_group, true);

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(ClusterParserTest, SkipOnClusterBegin) {
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnClusterBegin(_, _, _)).Times(0);

    EXPECT_CALL(callback_, OnBlockGroupBegin(_, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(1);
  }

  ElementMetadata child_metadata = {Id::kBlockGroup, 0, 0, 0};

  Ancestory ancestory;
  ASSERT_TRUE(Ancestory::ById(child_metadata.id, &ancestory));
  // Skip the Segment and Cluster ancestors.
  ancestory = ancestory.next().next();

  parser_.InitAfterSeek(ancestory, child_metadata);

  std::uint64_t num_bytes_read = 0;
  const Status status = parser_.Feed(&callback_, &reader_, &num_bytes_read);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader_.size(), num_bytes_read);
}

TEST_F(ClusterParserTest, SkipSimpleBlock) {
  SetReaderData({
      0xA3,  // ID = 0xA3 (SimpleBlock).
      0x85,  // Size = 5.
      0x81,  // Track number = 1.
      0x00, 0x00,  // Timecode = 0.
      0x00,  // Flags = 0.
      0x00,  // Frame 0.
  });

  {
    InSequence dummy;

    Cluster cluster{};
    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    SimpleBlock simple_block{};
    simple_block.num_frames = 1;
    simple_block.is_visible = true;
    simple_block.track_number = 1;

    EXPECT_CALL(callback_, OnSimpleBlockBegin(_, simple_block, NotNull()))
        .WillOnce(DoAll(SetArgPointee<2>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(ClusterParserTest, SkipBlockGroup) {
  SetReaderData({
      0xA0,  // ID = 0xA0 (BlockGroup).
      0x40, 0x04,  // Size = 4.

      0x9B,  //   ID = 0x9B (BlockDuration).
      0x40, 0x01,  //   Size = 1.
      0x01,  //   Body (value = 1).
  });

  {
    InSequence dummy;

    Cluster cluster{};
    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    EXPECT_CALL(callback_, OnBlockGroupBegin(_, NotNull()))
        .WillOnce(DoAll(SetArgPointee<1>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

}  // namespace
