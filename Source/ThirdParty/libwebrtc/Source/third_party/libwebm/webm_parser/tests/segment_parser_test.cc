// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/segment_parser.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"
#include "webm/status.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

using webm::Action;
using webm::Ancestory;
using webm::ElementMetadata;
using webm::ElementParserTest;
using webm::Id;
using webm::SegmentParser;
using webm::Status;

namespace {

class SegmentParserTest
    : public ElementParserTest<SegmentParser, Id::kSegment> {};

TEST_F(SegmentParserTest, EmptyParse) {
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnSegmentBegin(metadata_, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnSegmentEnd(metadata_)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(SegmentParserTest, DefaultActionIsRead) {
  {
    InSequence dummy;

    // This intentionally does not set the action and relies on the parser using
    // a default action value of kRead.
    EXPECT_CALL(callback_, OnSegmentBegin(metadata_, NotNull()))
        .WillOnce(Return(Status(Status::kOkCompleted)));
    EXPECT_CALL(callback_, OnSegmentEnd(metadata_)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(SegmentParserTest, EmptyParseWithDelayedStart) {
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnSegmentBegin(metadata_, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoAll(SetArgPointee<1>(Action::kRead),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnSegmentEnd(metadata_)).Times(1);
  }

  IncrementalParseAndVerify();
}

TEST_F(SegmentParserTest, DefaultValues) {
  SetReaderData({
      0x11, 0x4D, 0x9B, 0x74,  // ID = 0x114D9B74 (SeekHead).
      0x80,  // Size = 0.

      0x15, 0x49, 0xA9, 0x66,  // ID = 0x1549A966 (Info).
      0x80,  // Size = 0.

      0x1F, 0x43, 0xB6, 0x75,  // ID = 0x1F43B675 (Cluster).
      0x80,  // Size = 0.

      0x16, 0x54, 0xAE, 0x6B,  // ID = 0x1654AE6B (Tracks).
      0x80,  // Size = 0.

      0x1C, 0x53, 0xBB, 0x6B,  // ID = 0x1C53BB6B (Cues).
      0x80,  // Size = 0.

      0x10, 0x43, 0xA7, 0x70,  // ID = 0x1043A770 (Chapters).
      0x80,  // Size = 0.

      0x12, 0x54, 0xC3, 0x67,  // ID = 0x1254C367 (Tags).
      0x80,  // Size = 0.
  });

  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnSegmentBegin(metadata_, NotNull())).Times(1);

    ElementMetadata metadata = {Id::kInfo, 5, 0, 5};
    EXPECT_CALL(callback_, OnInfo(metadata, _)).Times(1);

    metadata = {Id::kCluster, 5, 0, 10};
    EXPECT_CALL(callback_, OnClusterBegin(metadata, _, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnClusterEnd(metadata, _)).Times(1);

    EXPECT_CALL(callback_, OnSegmentEnd(metadata_)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(SegmentParserTest, RepeatedValues) {
  SetReaderData({
      // Mutliple SeekHead elements.
      0x11, 0x4D, 0x9B, 0x74,  // ID = 0x114D9B74 (SeekHead).
      0x8D,  // Size = 13.

      0x4D, 0xBB,  //   ID = 0x4DBB (Seek).
      0x10, 0x00, 0x00, 0x07,  //   Size = 7.

      0x53, 0xAC,  //     ID = 0x53AC (SeekPosition).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x01,  //     Body (value = 1).

      0x11, 0x4D, 0x9B, 0x74,  // ID = 0x114D9B74 (SeekHead).
      0x8D,  // Size = 13.

      0x4D, 0xBB,  //   ID = 0x4DBB (Seek).
      0x10, 0x00, 0x00, 0x07,  //   Size = 7.

      0x53, 0xAC,  //     ID = 0x53AC (SeekPosition).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x02,  //     Body (value = 2).

      // Multiple Info elements.
      0x15, 0x49, 0xA9, 0x66,  // ID = 0x1549A966 (Info).
      0x88,  // Size = 8.

      0x2A, 0xD7, 0xB1,  //   ID = 0x2AD7B1 (TimecodeScale).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x01,  //   Body (value = 1).

      0x15, 0x49, 0xA9, 0x66,  // ID = 0x1549A966 (Info).
      0x88,  // Size = 8.

      0x2A, 0xD7, 0xB1,  //   ID = 0x2AD7B1 (TimecodeScale).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x02,  //   Body (value = 2).

      // Multiple Cluster elements.
      0x1F, 0x43, 0xB6, 0x75,  // ID = 0x1F43B675 (Cluster).
      0x86,  // Size = 6.

      0xE7,  //   ID = 0xE7 (Timecode).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x01,  //   Body (value = 1).

      0x1F, 0x43, 0xB6, 0x75,  // ID = 0x1F43B675 (Cluster).
      0x86,  // Size = 6.

      0xE7,  //   ID = 0xE7 (Timecode).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x02,  //   Body (value = 2).

      // Multiple Tracks elements.
      0x16, 0x54, 0xAE, 0x6B,  // ID = 0x1654AE6B (Tracks).
      0x8B,  // Size = 11.

      0xAE,  //   ID = 0xAE (TrackEntry).
      0x10, 0x00, 0x00, 0x06,  //   Size = 6.

      0xD7,  //     ID = 0xD7 (TrackNumber).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x01,  //     Body (value = 1).

      0x16, 0x54, 0xAE, 0x6B,  // ID = 0x1654AE6B (Tracks).
      0x8B,  // Size = 11.

      0xAE,  //   ID = 0xAE (TrackEntry).
      0x10, 0x00, 0x00, 0x06,  //   Size = 6.

      0xD7,  //     ID = 0xD7 (TrackNumber).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x02,  //     Body (value = 2).

      // Single Cues element.
      0x1C, 0x53, 0xBB, 0x6B,  // ID = 0x1C53BB6B (Cues).
      0x8B,  // Size = 11.

      0xBB,  //   ID = 0xBB (CuePoint).
      0x10, 0x00, 0x00, 0x06,  //   Size = 6.

      0xB3,  //     ID = 0xB3 (CueTime).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x01,  //     Body (value = 1).

      // Single Chapters element.
      0x10, 0x43, 0xA7, 0x70,  // ID = 0x1043A770 (Chapters).
      0x8D,  // Size = 13.

      0x45, 0xB9,  //   ID = 0x45B9 (EditionEntry).
      0x10, 0x00, 0x00, 0x07,  //   Size = 7.

      0x45, 0xBC,  //     ID = 0x45BC (EditionUID).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x01,  //     Body (value = 1).

      // Multiple Tags elements.
      0x12, 0x54, 0xC3, 0x67,  // ID = 0x1254C367 (Tags).
      0x93,  // Size = 19.

      0x73, 0x73,  //   ID = 0x7373 (Tag).
      0x10, 0x00, 0x00, 0x0D,  //   Size = 13.

      0x63, 0xC0,  //     ID = 0x63C0 (Targets).
      0x10, 0x00, 0x00, 0x07,  //     Size = 7.

      0x68, 0xCA,  //       ID = 0x68CA (TargetTypeValue).
      0x10, 0x00, 0x00, 0x01,  //       Size = 1.
      0x01,  //       Body (value = 1).

      0x12, 0x54, 0xC3, 0x67,  // ID = 0x1254C367 (Tags).
      0x93,  // Size = 19.

      0x73, 0x73,  //   ID = 0x7373 (Tag).
      0x10, 0x00, 0x00, 0x0D,  //   Size = 13.

      0x63, 0xC0,  //     ID = 0x63C0 (Targets).
      0x10, 0x00, 0x00, 0x07,  //     Size = 7.

      0x68, 0xCA,  //       ID = 0x68CA (TargetTypeValue).
      0x10, 0x00, 0x00, 0x01,  //       Size = 1.
      0x02,  //       Body (value = 2).
  });

  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnSegmentBegin(metadata_, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnSeek(_, _)).Times(2);

    EXPECT_CALL(callback_, OnInfo(_, _)).Times(2);

    EXPECT_CALL(callback_, OnClusterBegin(_, _, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(_, _, NotNull())).Times(1);
    EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(1);

    EXPECT_CALL(callback_, OnTrackEntry(_, _)).Times(2);

    EXPECT_CALL(callback_, OnCuePoint(_, _)).Times(1);

    EXPECT_CALL(callback_, OnEditionEntry(_, _)).Times(1);

    EXPECT_CALL(callback_, OnTag(_, _)).Times(2);

    EXPECT_CALL(callback_, OnSegmentEnd(metadata_)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(SegmentParserTest, Skip) {
  SetReaderData({
      0x1F, 0x43, 0xB6, 0x75,  // ID = 0x1F43B675 (Cluster).
      0x80,  // Size = 0.
  });

  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnSegmentBegin(metadata_, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoAll(SetArgPointee<1>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnClusterBegin(_, _, _)).Times(0);
    EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(0);

    EXPECT_CALL(callback_, OnSegmentEnd(_)).Times(0);
  }

  IncrementalParseAndVerify();
}

TEST_F(SegmentParserTest, Seek) {
  SetReaderData({
      // We start the reader at the beginning of a FlagInterlaced element
      // (skipping its ID and size, as they're passed into InitAfterSeek).
      // FlagInterlaced, StereoMode, and AlphaMode are all part of the Video
      // element, with the ancestory: Segment -> Tracks -> TrackEntry -> Video.
      0x01,  // Body (value = 1).

      0x53, 0xB8,  // ID = 0x53B8 (StereoMode).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0x53, 0xC0,  // ID = 0x53C0 (AlphaMode).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x03,  // Body (value = 3).

      // Single Cues element.
      0x1C, 0x53, 0xBB, 0x6B,  // ID = 0x1C53BB6B (Cues).
      0x8B,  // Size = 11.

      0xBB,  //   ID = 0xBB (CuePoint).
      0x10, 0x00, 0x00, 0x06,  //   Size = 6.

      0xB3,  //     ID = 0xB3 (CueTime).
      0x10, 0x00, 0x00, 0x01,  //     Size = 1.
      0x01,  //     Body (value = 1).
  });

  const ElementMetadata flag_interlaced_metadata = {Id::kFlagInterlaced, 0, 1,
                                                    0};

  EXPECT_CALL(callback_, OnSegmentBegin(_, _)).Times(0);
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnElementBegin(flag_interlaced_metadata, NotNull()))
        .Times(1);

    ElementMetadata metadata = {Id::kStereoMode, 6, 1, 1};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kAlphaMode, 6, 1, 8};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnTrackEntry(_, _)).Times(1);

    metadata = {Id::kCues, 5, 11, 15};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kCuePoint, 5, 6, 20};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kCueTime, 5, 1, 25};
    EXPECT_CALL(callback_, OnElementBegin(metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnCuePoint(_, _)).Times(1);

    EXPECT_CALL(callback_, OnSegmentEnd(_)).Times(1);
  }

  Ancestory ancestory;
  ASSERT_TRUE(Ancestory::ById(flag_interlaced_metadata.id, &ancestory));
  ancestory = ancestory.next();  // Skip the Segment ancestor.

  parser_.InitAfterSeek(ancestory, flag_interlaced_metadata);

  std::uint64_t num_bytes_read = 0;
  const Status status = parser_.Feed(&callback_, &reader_, &num_bytes_read);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader_.size(), num_bytes_read);
}

}  // namespace
