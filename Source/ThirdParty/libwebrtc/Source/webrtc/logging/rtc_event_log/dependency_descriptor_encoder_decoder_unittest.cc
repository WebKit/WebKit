/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/dependency_descriptor_encoder_decoder.h"

#include <string>
#include <vector>

#include "logging/rtc_event_log/encoder/delta_encoding.h"
#include "logging/rtc_event_log/encoder/optional_blob_encoding.h"
#include "logging/rtc_event_log/rtc_event_log2_proto_include.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;

namespace webrtc {
namespace {

constexpr uint64_t kNone = 0;
constexpr uint64_t kEnd = 1;
constexpr uint64_t kBegin = 2;
constexpr uint64_t kBeginEnd = 3;

class DdBuilder {
 public:
  DdBuilder() : raw_dd(3) {}
  DdBuilder& B() {
    raw_dd[0] |= 1 << 7;
    return *this;
  }
  DdBuilder& E() {
    raw_dd[0] |= 1 << 6;
    return *this;
  }
  DdBuilder& Tid(uint8_t id) {
    RTC_DCHECK_LE(id, 0x3F);
    raw_dd[0] |= id;
    return *this;
  }
  DdBuilder& Fid(uint16_t id) {
    raw_dd[1] = id >> 8;
    raw_dd[2] = id;
    return *this;
  }
  DdBuilder& Ext(const std::string& ext) {
    raw_dd.insert(raw_dd.end(), ext.begin(), ext.end());
    return *this;
  }
  std::vector<uint8_t> Build() { return raw_dd; }

 private:
  std::vector<uint8_t> raw_dd;
};

TEST(RtcEventLogDependencyDescriptorEncoding, SinglePacket) {
  auto encoded = RtcEventLogDependencyDescriptorEncoderDecoder::Encode(
      {DdBuilder().B().E().Tid(12).Fid(345).Ext("abc").Build()});
  EXPECT_THAT(encoded->start_end_bit(), Eq(kBeginEnd));
  EXPECT_THAT(encoded->template_id(), Eq(12u));
  EXPECT_THAT(encoded->frame_id(), Eq(345u));
  EXPECT_THAT(encoded->extended_infos(), Eq(EncodeOptionalBlobs({{"abc"}})));
  EXPECT_THAT(encoded->has_start_end_bit_deltas(), Eq(false));
  EXPECT_THAT(encoded->has_template_id_deltas(), Eq(false));
  EXPECT_THAT(encoded->has_frame_id_deltas(), Eq(false));
}

TEST(RtcEventLogDependencyDescriptorEncoding, MultiplePackets) {
  auto encoded = RtcEventLogDependencyDescriptorEncoderDecoder::Encode(
      {DdBuilder().B().E().Tid(1).Fid(1).Ext("abc").Build(),
       DdBuilder().B().Tid(2).Fid(3).Ext("def").Build(),
       DdBuilder().E().Tid(2).Fid(3).Build()});

  EXPECT_THAT(encoded->start_end_bit(), Eq(kBeginEnd));
  EXPECT_THAT(encoded->template_id(), Eq(1u));
  EXPECT_THAT(encoded->frame_id(), Eq(1u));
  EXPECT_THAT(encoded->extended_infos(),
              Eq(EncodeOptionalBlobs({{"abc"}, {"def"}, {}})));
  EXPECT_THAT(encoded->start_end_bit_deltas(),
              Eq(EncodeDeltas({kBeginEnd}, {{kBegin}, {kEnd}})));
  EXPECT_THAT(encoded->template_id_deltas(), Eq(EncodeDeltas({1}, {{2}, {2}})));
  EXPECT_THAT(encoded->frame_id_deltas(), Eq(EncodeDeltas({1}, {{3}, {3}})));
}

TEST(RtcEventLogDependencyDescriptorEncoding, MultiplePacketsOneFrame) {
  auto encoded = RtcEventLogDependencyDescriptorEncoderDecoder::Encode(
      {DdBuilder().B().Tid(1).Fid(1).Ext("abc").Build(),
       DdBuilder().Tid(1).Fid(1).Build(),
       DdBuilder().E().Tid(1).Fid(1).Build()});

  EXPECT_THAT(encoded->start_end_bit(), Eq(kBegin));
  EXPECT_THAT(encoded->template_id(), Eq(1u));
  EXPECT_THAT(encoded->frame_id(), Eq(1u));
  EXPECT_THAT(encoded->extended_infos(),
              Eq(EncodeOptionalBlobs({{"abc"}, {}, {}})));
  EXPECT_THAT(encoded->start_end_bit_deltas(),
              Eq(EncodeDeltas({kBegin}, {{kNone}, {kEnd}})));
  EXPECT_THAT(encoded->has_template_id_deltas(), Eq(false));
  EXPECT_THAT(encoded->has_frame_id_deltas(), Eq(false));
}

TEST(RtcEventLogDependencyDescriptorEncoding, FirstPacketMissingDd) {
  auto encoded = RtcEventLogDependencyDescriptorEncoderDecoder::Encode(
      {{},
       DdBuilder().B().E().Tid(1).Fid(1).Build(),
       DdBuilder().E().Tid(2).Fid(3).Ext("three!").Build()});

  EXPECT_THAT(encoded->has_start_end_bit(), Eq(false));
  EXPECT_THAT(encoded->has_template_id(), Eq(false));
  EXPECT_THAT(encoded->has_frame_id(), Eq(false));

  EXPECT_THAT(encoded->extended_infos(),
              Eq(EncodeOptionalBlobs({{}, {}, {"three!"}})));
  EXPECT_THAT(encoded->start_end_bit_deltas(),
              Eq(EncodeDeltas({}, {{kBeginEnd}, {kEnd}})));
  EXPECT_THAT(encoded->template_id_deltas(), Eq(EncodeDeltas({}, {{1}, {2}})));
  EXPECT_THAT(encoded->frame_id_deltas(), Eq(EncodeDeltas({}, {{1}, {3}})));
}

TEST(RtcEventLogDependencyDescriptorEncoding, SomePacketMissingDd) {
  auto encoded = RtcEventLogDependencyDescriptorEncoderDecoder::Encode(
      {DdBuilder().B().E().Tid(1).Fid(1).Build(),
       {},
       DdBuilder().E().Tid(2).Fid(3).Build()});

  EXPECT_THAT(encoded->start_end_bit(), Eq(kBeginEnd));
  EXPECT_THAT(encoded->template_id(), Eq(1u));
  EXPECT_THAT(encoded->frame_id(), Eq(1u));
  EXPECT_THAT(encoded->extended_infos(), Eq(EncodeOptionalBlobs({{}, {}, {}})));
  EXPECT_THAT(encoded->start_end_bit_deltas(),
              Eq(EncodeDeltas({kBeginEnd}, {{}, {kEnd}})));
  EXPECT_THAT(encoded->template_id_deltas(), Eq(EncodeDeltas({1}, {{}, {2}})));
  EXPECT_THAT(encoded->frame_id_deltas(), Eq(EncodeDeltas({1}, {{}, {3}})));
}

TEST(RtcEventLogDependencyDescriptorDecoding, SinglePacket) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_start_end_bit(kBeginEnd);
  encoded.set_template_id(1);
  encoded.set_frame_id(2);
  encoded.set_extended_infos(EncodeOptionalBlobs({"abc"}));

  auto decoded =
      RtcEventLogDependencyDescriptorEncoderDecoder::Decode(encoded, 1);

  auto expected = DdBuilder().B().E().Tid(1).Fid(2).Ext("abc").Build();
  ASSERT_THAT(decoded.ok(), Eq(true));
  EXPECT_THAT(decoded.value(), ElementsAre(expected));
}

TEST(RtcEventLogDependencyDescriptorDecoding, MultiplePackets) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_start_end_bit(kBeginEnd);
  encoded.set_template_id(1);
  encoded.set_frame_id(1);
  encoded.set_extended_infos(EncodeOptionalBlobs({{"abc"}, {"def"}, {}}));
  encoded.set_start_end_bit_deltas(
      EncodeDeltas({kBeginEnd}, {{kBegin}, {kEnd}}));
  encoded.set_template_id_deltas(EncodeDeltas({1}, {{2}, {2}}));
  encoded.set_frame_id_deltas(EncodeDeltas({1}, {{3}, {3}}));

  auto decoded =
      RtcEventLogDependencyDescriptorEncoderDecoder::Decode(encoded, 3);

  auto first_expected = DdBuilder().B().E().Tid(1).Fid(1).Ext("abc").Build();
  auto second_expected = DdBuilder().B().Tid(2).Fid(3).Ext("def").Build();
  auto third_expected = DdBuilder().E().Tid(2).Fid(3).Build();
  ASSERT_THAT(decoded.ok(), Eq(true));
  EXPECT_THAT(decoded.value(),
              ElementsAre(first_expected, second_expected, third_expected));
}

TEST(RtcEventLogDependencyDescriptorDecoding, MultiplePacketsOneFrame) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_start_end_bit(kBegin);
  encoded.set_template_id(1u);
  encoded.set_frame_id(1u);
  encoded.set_extended_infos(EncodeOptionalBlobs({{"abc"}, {}, {}}));
  encoded.set_start_end_bit_deltas(EncodeDeltas({kBegin}, {{kNone}, {kEnd}}));

  auto decoded =
      RtcEventLogDependencyDescriptorEncoderDecoder::Decode(encoded, 3);

  auto first_expected = DdBuilder().B().Tid(1).Fid(1).Ext("abc").Build();
  auto second_expected = DdBuilder().Tid(1).Fid(1).Build();
  auto third_expected = DdBuilder().E().Tid(1).Fid(1).Build();
  ASSERT_THAT(decoded.ok(), Eq(true));
  EXPECT_THAT(decoded.value(),
              ElementsAre(first_expected, second_expected, third_expected));
}

TEST(RtcEventLogDependencyDescriptorDecoding, FirstPacketMissingDd) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_extended_infos(EncodeOptionalBlobs({{}, {}, {"three!"}}));
  encoded.set_start_end_bit_deltas(EncodeDeltas({}, {{kBeginEnd}, {kEnd}}));
  encoded.set_template_id_deltas(EncodeDeltas({}, {{1}, {2}}));
  encoded.set_frame_id_deltas(EncodeDeltas({}, {{1}, {3}}));

  auto decoded =
      RtcEventLogDependencyDescriptorEncoderDecoder::Decode(encoded, 3);

  auto second_expected = DdBuilder().B().E().Tid(1).Fid(1).Build();
  auto third_expected = DdBuilder().E().Tid(2).Fid(3).Ext("three!").Build();
  ASSERT_THAT(decoded.ok(), Eq(true));
  EXPECT_THAT(decoded.value(),
              ElementsAre(IsEmpty(), second_expected, third_expected));
}

TEST(RtcEventLogDependencyDescriptorDecoding, SomePacketMissingDd) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_start_end_bit(kBeginEnd);
  encoded.set_template_id(1u);
  encoded.set_frame_id(1u);
  encoded.set_extended_infos(EncodeOptionalBlobs({{}, {}, {}}));
  encoded.set_start_end_bit_deltas(EncodeDeltas({kBeginEnd}, {{}, {kEnd}}));
  encoded.set_template_id_deltas(EncodeDeltas({1}, {{}, {2}}));
  encoded.set_frame_id_deltas(EncodeDeltas({1}, {{}, {3}}));

  auto decoded =
      RtcEventLogDependencyDescriptorEncoderDecoder::Decode(encoded, 3);

  auto first_expected = DdBuilder().B().E().Tid(1).Fid(1).Build();
  auto third_expected = DdBuilder().E().Tid(2).Fid(3).Build();
  ASSERT_THAT(decoded.ok(), Eq(true));
  EXPECT_THAT(decoded.value(),
              ElementsAre(first_expected, IsEmpty(), third_expected));
}

TEST(RtcEventLogDependencyDescriptorDecoding, ExtendedWithoutRequiredFields) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_start_end_bit_deltas(EncodeDeltas({}, {{kNone}}));
  encoded.set_template_id_deltas(EncodeDeltas({}, {{1}}));
  encoded.set_frame_id_deltas(EncodeDeltas({}, {{1}}));
  encoded.set_extended_infos(EncodeOptionalBlobs({{"Should not be here"}, {}}));

  auto decoded =
      RtcEventLogDependencyDescriptorEncoderDecoder::Decode(encoded, 2);

  ASSERT_THAT(decoded.ok(), Eq(false));
}

TEST(RtcEventLogDependencyDescriptorDecoding, MissingRequiredField) {
  rtclog2::DependencyDescriptorsWireInfo encoded;
  encoded.set_start_end_bit(kBeginEnd);
  encoded.set_template_id(1u);
  encoded.set_frame_id(1u);

  {
    rtclog2::DependencyDescriptorsWireInfo missing = encoded;
    missing.clear_start_end_bit();
    EXPECT_THAT(
        RtcEventLogDependencyDescriptorEncoderDecoder::Decode(missing, 1).ok(),
        Eq(false));
  }

  {
    rtclog2::DependencyDescriptorsWireInfo missing = encoded;
    missing.clear_template_id();
    EXPECT_THAT(
        RtcEventLogDependencyDescriptorEncoderDecoder::Decode(missing, 1).ok(),
        Eq(false));
  }

  {
    rtclog2::DependencyDescriptorsWireInfo missing = encoded;
    missing.clear_frame_id();
    EXPECT_THAT(
        RtcEventLogDependencyDescriptorEncoderDecoder::Decode(missing, 1).ok(),
        Eq(false));
  }
}

}  // namespace
}  // namespace webrtc
