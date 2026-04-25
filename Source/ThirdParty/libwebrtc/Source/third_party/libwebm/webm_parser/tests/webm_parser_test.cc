// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm/webm_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/mock_callback.h"
#include "webm/buffer_reader.h"
#include "webm/status.h"

using testing::_;
using testing::DoDefault;
using testing::InSequence;
using testing::NotNull;
using testing::Return;

using webm::BufferReader;
using webm::Ebml;
using webm::ElementMetadata;
using webm::Id;
using webm::Info;
using webm::kUnknownElementPosition;
using webm::kUnknownElementSize;
using webm::kUnknownHeaderSize;
using webm::MockCallback;
using webm::Status;
using webm::WebmParser;

namespace {

class WebmParserTest : public testing::Test {};

TEST_F(WebmParserTest, InvalidId) {
  BufferReader reader = {
      0x00,  // IDs cannot start with 0x00.
  };

  MockCallback callback;
  {
    InSequence dummy;

    EXPECT_CALL(callback, OnEbml(_, _)).Times(0);

    EXPECT_CALL(callback, OnSegmentBegin(_, NotNull())).Times(0);
    EXPECT_CALL(callback, OnSegmentEnd(_)).Times(0);
  }

  WebmParser parser;
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kInvalidElementId, status.code);
}

TEST_F(WebmParserTest, InvalidSize) {
  BufferReader reader = {
      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x00,  // Size must have 1+ bits set in the first byte.
  };

  MockCallback callback;
  {
    InSequence dummy;

    EXPECT_CALL(callback, OnEbml(_, _)).Times(0);

    EXPECT_CALL(callback, OnSegmentBegin(_, NotNull())).Times(0);
    EXPECT_CALL(callback, OnSegmentEnd(_)).Times(0);
  }

  WebmParser parser;
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kInvalidElementSize, status.code);
}

TEST_F(WebmParserTest, DefaultParse) {
  BufferReader reader = {
      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x80,  // Size = 0.

      0x18, 0x53, 0x80, 0x67,  // ID = 0x18538067 (Segment).
      0x80,  // Size = 0.
  };

  MockCallback callback;
  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kEbml, 5, 0, 0};
    const Ebml ebml{};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnEbml(metadata, ebml)).Times(1);

    metadata = {Id::kSegment, 5, 0, 5};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnSegmentBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnSegmentEnd(metadata)).Times(1);
  }

  WebmParser parser;
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
}

TEST_F(WebmParserTest, DefaultActionIsRead) {
  BufferReader reader = {
      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x80,  // Size = 0.
  };

  MockCallback callback;
  {
    InSequence dummy;

    const ElementMetadata metadata = {Id::kEbml, 5, 0, 0};
    const Ebml ebml{};

    // This intentionally does not set the action and relies on the parser using
    // a default action value of kRead.
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull()))
        .WillOnce(Return(Status(Status::kOkCompleted)));
    EXPECT_CALL(callback, OnEbml(metadata, ebml)).Times(1);
  }

  WebmParser parser;
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
}

TEST_F(WebmParserTest, UnknownElement) {
  BufferReader reader = {
      0x80,  // ID = 0x80.
      0x80,  // Size = 0.
  };

  MockCallback callback;
  {
    InSequence dummy;

    ElementMetadata metadata = {static_cast<Id>(0x80), 2, 0, 0};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnUnknownElement(metadata, NotNull(), NotNull()))
        .Times(1);
  }

  WebmParser parser;
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
}

TEST_F(WebmParserTest, SeekEbml) {
  BufferReader reader = {
      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x87,  // Size = 7.

      0x42, 0x86,  //   ID = 0x4286 (EBMLVersion).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x02,  //   Body (value = 2).
  };
  std::uint64_t num_to_skip = 5;  // Skip the starting EBML element metadata.
  std::uint64_t num_actually_skipped = 0;
  reader.Skip(num_to_skip, &num_actually_skipped);
  EXPECT_EQ(num_to_skip, num_actually_skipped);

  MockCallback callback;
  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kEbmlVersion, 6, 1, num_to_skip};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);

    metadata = {Id::kEbml, kUnknownHeaderSize, kUnknownElementSize,
                kUnknownElementPosition};
    Ebml ebml{};
    ebml.ebml_version.Set(2, true);
    EXPECT_CALL(callback, OnEbml(metadata, ebml)).Times(1);
  }

  WebmParser parser;
  parser.DidSeek();
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader.size(), reader.Position());
}

TEST_F(WebmParserTest, SeekSegment) {
  BufferReader reader = {
      0x18, 0x53, 0x80, 0x67,  // ID = 0x18538067 (Segment).
      0x85,  // Size = 5.

      0x15, 0x49, 0xA9, 0x66,  //   ID = 0x1549A966 (Info).
      0x80,  //   Size = 0.
  };
  std::uint64_t num_to_skip = 5;  // Skip the starting Segment element metadata.
  std::uint64_t num_actually_skipped = 0;
  reader.Skip(num_to_skip, &num_actually_skipped);
  EXPECT_EQ(num_to_skip, num_actually_skipped);

  MockCallback callback;
  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kInfo, 5, 0, num_to_skip};
    const Info info{};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnInfo(metadata, info)).Times(1);

    metadata = {Id::kSegment, kUnknownHeaderSize, kUnknownElementSize,
                kUnknownElementPosition};
    EXPECT_CALL(callback, OnSegmentEnd(metadata)).Times(1);
  }

  WebmParser parser;
  parser.DidSeek();
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader.size(), reader.Position());
}

TEST_F(WebmParserTest, SeekVoid) {
  BufferReader reader = {
      0xEC,  // ID = 0xEC (Void).
      0x81,  // Size = 0.

      0xEC,  // ID = 0xEC (Void).
      0x81,  // Size = 1.
      0x00,  // Body.

      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x80,  // Size = 0.
  };
  std::uint64_t num_to_skip = 2;  // Skip the first Void element.
  std::uint64_t num_actually_skipped = 0;
  reader.Skip(num_to_skip, &num_actually_skipped);
  EXPECT_EQ(num_to_skip, num_actually_skipped);

  MockCallback callback;
  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kVoid, 2, 1, num_to_skip};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnVoid(metadata, &reader, NotNull())).Times(1);

    metadata = {Id::kEbml, 5, 0, 5};
    const Ebml ebml{};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnEbml(metadata, ebml)).Times(1);
  }

  WebmParser parser;
  parser.DidSeek();
  Status status = parser.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader.size(), reader.Position());
}

TEST_F(WebmParserTest, SwapAfterFailedParse) {
  BufferReader reader = {
      0x00,  // Invalid ID.

      0xEC,  // ID = 0xEC (Void).
      0x81,  // Size = 1.
      0x00,  // Body.
  };

  MockCallback expect_nothing;
  EXPECT_CALL(expect_nothing, OnElementBegin(_, _)).Times(0);

  MockCallback expect_void;
  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kVoid, 2, 1, 1};
    EXPECT_CALL(expect_void, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(expect_void, OnVoid(metadata, &reader, NotNull())).Times(1);
  }

  WebmParser parser1;
  Status status = parser1.Feed(&expect_nothing, &reader);
  EXPECT_EQ(Status::kInvalidElementId, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), reader.Position());

  // After swapping, the parser should retain its failed state and not consume
  // more data.
  WebmParser parser2;
  parser2.Swap(&parser1);
  status = parser2.Feed(&expect_nothing, &reader);
  EXPECT_EQ(Status::kInvalidElementId, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(1), reader.Position());

  // parser1 should be a fresh/new parser after the swap, so parsing should
  // succeed.
  status = parser1.Feed(&expect_void, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader.size(), reader.Position());
}

TEST_F(WebmParserTest, Swap) {
  BufferReader reader = {
      0xEC,  // ID = 0xEC (Void).
      0x81,  // Size = 1.
      0x00,  // Body.

      0x1A, 0x45, 0xDF, 0xA3,  // ID = 0x1A45DFA3 (EBML).
      0x80,  // Size = 0.
  };

  MockCallback callback;
  {
    InSequence dummy;

    ElementMetadata metadata = {Id::kVoid, 2, 1, 0};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnVoid(metadata, &reader, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoDefault());

    metadata = {Id::kEbml, 5, 0, 3};
    EXPECT_CALL(callback, OnElementBegin(metadata, NotNull())).Times(1);
    EXPECT_CALL(callback, OnEbml(metadata, Ebml{})).Times(1);
  }

  WebmParser parser1;
  Status status = parser1.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkPartial, status.code);

  WebmParser parser2;
  swap(parser1, parser2);
  status = parser2.Feed(&callback, &reader);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(reader.size(), reader.Position());
}

}  // namespace
