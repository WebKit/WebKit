// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/master_value_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "src/int_parser.h"
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
using webm::Callback;
using webm::Cluster;
using webm::ElementMetadata;
using webm::ElementParserTest;
using webm::Id;
using webm::MasterValueParser;
using webm::Status;
using webm::UnsignedIntParser;

namespace {

// An instance of MasterValueParser that can actually be used in the tests to
// parse Cluster structures.
class FakeClusterParser : public MasterValueParser<Cluster> {
 public:
  FakeClusterParser()
      : MasterValueParser(
            MakeChild<UnsignedIntParser>(Id::kTimecode, &Cluster::timecode),
            MakeChild<UnsignedIntParser>(Id::kPrevSize, &Cluster::previous_size)
                .UseAsStartEvent()) {}

 protected:
  Status OnParseStarted(Callback* callback, Action* action) override {
    return callback->OnClusterBegin(metadata(Id::kCluster), value(), action);
  }

  Status OnParseCompleted(Callback* callback) override {
    return callback->OnClusterEnd(metadata(Id::kCluster), value());
  }
};

class MasterValueParserTest
    : public ElementParserTest<FakeClusterParser, Id::kCluster> {};

TEST_F(MasterValueParserTest, InvalidId) {
  SetReaderData({
      0x00,  // Invalid ID.
  });

  ParseAndExpectResult(Status::kInvalidElementId);
}

TEST_F(MasterValueParserTest, InvalidSize) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0xFF,  // Size = unknown.
  });

  ParseAndExpectResult(Status::kInvalidElementSize);
}

TEST_F(MasterValueParserTest, DefaultParse) {
  {
    InSequence dummy;

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, Cluster{}, NotNull()))
        .Times(1);

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, Cluster{})).Times(1);
  }

  ParseAndVerify();
}

TEST_F(MasterValueParserTest, DefaultValues) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0x80,  // Size = 0.

      0xAB,  // ID = 0xAB (PrevSize).
      0x80,  // Size = 0.
  });

  {
    InSequence dummy;

    Cluster cluster{};
    cluster.timecode.Set(0, true);

    ElementMetadata child_metadata = {Id::kTimecode, 2, 0, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    child_metadata = {Id::kPrevSize, 2, 0, 2};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    cluster.previous_size.Set(0, true);
    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(MasterValueParserTest, CustomValues) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0x82,  // Size = 2.
      0x04, 0x00,  // Body (value = 1024).

      0xAB,  // ID = 0xAB (PrevSize).
      0x40, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).
  });

  {
    InSequence dummy;

    Cluster cluster{};
    cluster.timecode.Set(1024, true);

    ElementMetadata child_metadata = {Id::kTimecode, 2, 2, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    child_metadata = {Id::kPrevSize, 3, 1, 4};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    cluster.previous_size.Set(1, true);
    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(MasterValueParserTest, IncrementalParse) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0x40, 0x02,  // Size = 2.
      0x04, 0x00,  // Body (value = 1024).

      0x40, 0x00,  // ID = 0x4000 (Unknown).
      0x40, 0x02,  // Size = 2.
      0x00, 0x00,  // Body.

      0xAB,  // ID = 0xAB (PrevSize).
      0x40, 0x02,  // Size = 2.
      0x00, 0x01,  // Body (value = 1).
  });

  {
    InSequence dummy;

    Cluster cluster{};
    cluster.timecode.Set(1024, true);

    ElementMetadata child_metadata = {Id::kTimecode, 3, 2, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    child_metadata = {static_cast<Id>(0x4000), 4, 2, 5};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);
    EXPECT_CALL(callback_,
                OnUnknownElement(child_metadata, NotNull(), NotNull()))
        .Times(3);

    child_metadata = {Id::kPrevSize, 3, 2, 11};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    cluster.previous_size.Set(1, true);
    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  IncrementalParseAndVerify();
}

TEST_F(MasterValueParserTest, StartEventWithoutPreviousSize) {
  SetReaderData({
      0xE7,  // ID = 0xE7 (Timecode).
      0x80,  // Size = 0.
  });

  {
    InSequence dummy;

    Cluster cluster{};

    const ElementMetadata child_metadata = {Id::kTimecode, 2, 0, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    cluster.timecode.Set(0, true);
    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(MasterValueParserTest, StartEventWithPreviousSize) {
  SetReaderData({
      0xAB,  // ID = 0xAB (PrevSize).
      0x80,  // Size = 0.

      0xE7,  // ID = 0xE7 (Timecode).
      0x80,  // Size = 0.
  });

  {
    InSequence dummy;

    Cluster cluster{};

    ElementMetadata child_metadata = {Id::kPrevSize, 2, 0, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .Times(1);

    child_metadata = {Id::kTimecode, 2, 0, 2};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    cluster.previous_size.Set(0, true);
    cluster.timecode.Set(0, true);
    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  ParseAndVerify();
}

TEST_F(MasterValueParserTest, PartialStartThenRead) {
  SetReaderData({
      0xAB,  // ID = 0xAB (PrevSize).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0xE7,  // ID = 0xE7 (Timecode).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).
  });

  {
    InSequence dummy;

    Cluster cluster{};

    ElementMetadata child_metadata = {Id::kPrevSize, 2, 1, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoAll(SetArgPointee<2>(Action::kRead),
                        Return(Status(Status::kOkCompleted))));

    child_metadata = {Id::kTimecode, 2, 1, 3};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    cluster.previous_size.Set(1, true);
    cluster.timecode.Set(1, true);
    EXPECT_CALL(callback_, OnClusterEnd(metadata_, cluster)).Times(1);
  }

  IncrementalParseAndVerify();
}

TEST_F(MasterValueParserTest, EmptySkip) {
  const Cluster cluster{};

  EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
      .WillOnce(DoAll(SetArgPointee<2>(Action::kSkip),
                      Return(Status(Status::kOkCompleted))));

  EXPECT_CALL(callback_, OnElementBegin(_, _)).Times(0);
  EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(0);

  ParseAndVerify();

  EXPECT_EQ(cluster, parser_.value());
}

TEST_F(MasterValueParserTest, Skip) {
  SetReaderData({
      0xAB,  // ID = 0xAB (PrevSize).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0xE7,  // ID = 0xE7 (Timecode).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).
  });

  const Cluster cluster{};

  {
    InSequence dummy;

    const ElementMetadata child_metadata = {Id::kPrevSize, 2, 1, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .WillOnce(DoAll(SetArgPointee<2>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(0);
  }

  ParseAndVerify();

  EXPECT_EQ(cluster, parser_.value());
}

TEST_F(MasterValueParserTest, PartialStartThenSkip) {
  SetReaderData({
      0xAB,  // ID = 0xAB (PrevSize).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0xE7,  // ID = 0xE7 (Timecode).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).
  });

  const Cluster cluster{};

  {
    InSequence dummy;

    const ElementMetadata child_metadata = {Id::kPrevSize, 2, 1, 0};
    EXPECT_CALL(callback_, OnElementBegin(child_metadata, NotNull())).Times(1);

    EXPECT_CALL(callback_, OnClusterBegin(metadata_, cluster, NotNull()))
        .WillOnce(Return(Status(Status::kOkPartial)))
        .WillOnce(DoAll(SetArgPointee<2>(Action::kSkip),
                        Return(Status(Status::kOkCompleted))));

    EXPECT_CALL(callback_, OnClusterEnd(_, _)).Times(0);
  }

  IncrementalParseAndVerify();

  EXPECT_EQ(cluster, parser_.value());
}

}  // namespace
