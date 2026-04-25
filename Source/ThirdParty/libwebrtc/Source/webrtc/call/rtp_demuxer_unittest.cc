/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_demuxer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "call/test/mock_rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::NiceMock;

class RtpDemuxerTest : public ::testing::Test {
 protected:
  ~RtpDemuxerTest() override {
    for (auto* sink : sinks_to_tear_down_) {
      demuxer_.RemoveSink(sink);
    }
  }

  // These are convenience methods for calling demuxer.AddSink with different
  // parameters and will ensure that the sink is automatically removed when the
  // test case finishes.

  bool AddSink(const RtpDemuxerCriteria& criteria,
               RtpPacketSinkInterface* sink) {
    bool added = demuxer_.AddSink(criteria, sink);
    if (added) {
      sinks_to_tear_down_.insert(sink);
    }
    return added;
  }

  bool AddSinkOnlySsrc(uint32_t ssrc, RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria;
    criteria.ssrcs().insert(ssrc);
    return AddSink(criteria, sink);
  }

  bool AddSinkOnlyRsid(absl::string_view rsid, RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria(absl::string_view(), rsid);
    return AddSink(criteria, sink);
  }

  bool AddSinkOnlyMid(absl::string_view mid, RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria(mid);
    return AddSink(criteria, sink);
  }

  bool AddSinkBothMidRsid(absl::string_view mid,
                          absl::string_view rsid,
                          RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria(mid, rsid);
    return AddSink(criteria, sink);
  }

  bool RemoveSink(RtpPacketSinkInterface* sink) {
    sinks_to_tear_down_.erase(sink);
    return demuxer_.RemoveSink(sink);
  }

  // The CreatePacket* methods are helpers for creating new RTP packets with
  // various attributes set. Tests should use the helper that provides the
  // minimum information needed to exercise the behavior under test. Tests also
  // should not rely on any behavior which is not clearly described in the
  // helper name/arguments. Any additional settings that are not covered by the
  // helper should be set manually on the packet once it has been returned.
  // For example, most tests in this file do not care about the RTP sequence
  // number, but to ensure that the returned packets are valid the helpers will
  // auto-increment the sequence number starting with 1. Tests that rely on
  // specific sequence number behavior should call SetSequenceNumber manually on
  // the returned packet.

  // Intended for use only by other CreatePacket* helpers.
  std::unique_ptr<RtpPacketReceived> CreatePacket(
      uint32_t ssrc,
      RtpPacketReceived::ExtensionManager* extension_manager) {
    auto packet = std::make_unique<RtpPacketReceived>(extension_manager);
    packet->SetSsrc(ssrc);
    packet->SetSequenceNumber(next_sequence_number_++);
    return packet;
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrc(uint32_t ssrc) {
    return CreatePacket(ssrc, nullptr);
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrcMid(
      uint32_t ssrc,
      absl::string_view mid) {
    RtpPacketReceived::ExtensionManager extension_manager;
    extension_manager.Register<RtpMid>(11);

    auto packet = CreatePacket(ssrc, &extension_manager);
    packet->SetExtension<RtpMid>(mid);
    return packet;
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrcRsid(
      uint32_t ssrc,
      absl::string_view rsid) {
    RtpPacketReceived::ExtensionManager extension_manager;
    extension_manager.Register<RtpStreamId>(6);

    auto packet = CreatePacket(ssrc, &extension_manager);
    packet->SetExtension<RtpStreamId>(rsid);
    return packet;
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrcRrid(
      uint32_t ssrc,
      absl::string_view rrid) {
    RtpPacketReceived::ExtensionManager extension_manager;
    extension_manager.Register<RepairedRtpStreamId>(7);

    auto packet = CreatePacket(ssrc, &extension_manager);
    packet->SetExtension<RepairedRtpStreamId>(rrid);
    return packet;
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrcMidRsid(
      uint32_t ssrc,
      absl::string_view mid,
      absl::string_view rsid) {
    RtpPacketReceived::ExtensionManager extension_manager;
    extension_manager.Register<RtpMid>(11);
    extension_manager.Register<RtpStreamId>(6);

    auto packet = CreatePacket(ssrc, &extension_manager);
    packet->SetExtension<RtpMid>(mid);
    packet->SetExtension<RtpStreamId>(rsid);
    return packet;
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrcRsidRrid(
      uint32_t ssrc,
      absl::string_view rsid,
      absl::string_view rrid) {
    RtpPacketReceived::ExtensionManager extension_manager;
    extension_manager.Register<RtpStreamId>(6);
    extension_manager.Register<RepairedRtpStreamId>(7);

    auto packet = CreatePacket(ssrc, &extension_manager);
    packet->SetExtension<RtpStreamId>(rsid);
    packet->SetExtension<RepairedRtpStreamId>(rrid);
    return packet;
  }

  RtpDemuxer demuxer_;
  std::set<RtpPacketSinkInterface*> sinks_to_tear_down_;
  uint16_t next_sequence_number_ = 1;
};

class RtpDemuxerDeathTest : public RtpDemuxerTest {};

MATCHER_P(SamePacketAs, other, "") {
  return arg.Ssrc() == other.Ssrc() &&
         arg.SequenceNumber() == other.SequenceNumber();
}

TEST_F(RtpDemuxerTest, CanAddSinkBySsrc) {
  MockRtpPacketSink sink;
  constexpr uint32_t ssrc = 1;

  EXPECT_TRUE(AddSinkOnlySsrc(ssrc, &sink));
}

TEST_F(RtpDemuxerTest, AllowAddSinkWithOverlappingPayloadTypesIfDifferentMid) {
  const std::string mid1 = "v";
  const std::string mid2 = "a";
  constexpr uint8_t pt1 = 30;
  constexpr uint8_t pt2 = 31;
  constexpr uint8_t pt3 = 32;

  RtpDemuxerCriteria pt1_pt2(mid1);
  pt1_pt2.payload_types() = {pt1, pt2};
  MockRtpPacketSink sink1;
  AddSink(pt1_pt2, &sink1);

  RtpDemuxerCriteria pt1_pt3(mid2);
  pt1_pt3.payload_types() = {pt1, pt3};
  MockRtpPacketSink sink2;
  EXPECT_TRUE(AddSink(pt1_pt3, &sink2));
}

TEST_F(RtpDemuxerTest, RejectAddSinkForSameMidOnly) {
  const std::string mid = "mid";

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);
  EXPECT_FALSE(AddSinkOnlyMid(mid, &sink));
}

TEST_F(RtpDemuxerTest, RejectAddSinkForSameMidRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";

  MockRtpPacketSink sink1;
  AddSinkBothMidRsid(mid, rsid, &sink1);

  MockRtpPacketSink sink2;
  EXPECT_FALSE(AddSinkBothMidRsid(mid, rsid, &sink2));
}

TEST_F(RtpDemuxerTest, RejectAddSinkForConflictingMidAndMidRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";

  MockRtpPacketSink mid_sink;
  AddSinkOnlyMid(mid, &mid_sink);

  // This sink would never get any packets routed to it because the above sink
  // would receive them all.
  MockRtpPacketSink mid_rsid_sink;
  EXPECT_FALSE(AddSinkBothMidRsid(mid, rsid, &mid_rsid_sink));
}

TEST_F(RtpDemuxerTest, RejectAddSinkForConflictingMidRsidAndMid) {
  const std::string mid = "v";
  const std::string rsid = "";

  MockRtpPacketSink mid_rsid_sink;
  AddSinkBothMidRsid(mid, rsid, &mid_rsid_sink);

  // This sink would shadow the above sink.
  MockRtpPacketSink mid_sink;
  EXPECT_FALSE(AddSinkOnlyMid(mid, &mid_sink));
}

TEST_F(RtpDemuxerTest, AddSinkFailsIfCalledForTwoSinksWithSameSsrc) {
  MockRtpPacketSink sink_a;
  MockRtpPacketSink sink_b;
  constexpr uint32_t ssrc = 1;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink_a));

  EXPECT_FALSE(AddSinkOnlySsrc(ssrc, &sink_b));
}

TEST_F(RtpDemuxerTest, AddSinkFailsIfCalledTwiceEvenIfSameSinkWithSameSsrc) {
  MockRtpPacketSink sink;
  constexpr uint32_t ssrc = 1;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink));

  EXPECT_FALSE(AddSinkOnlySsrc(ssrc, &sink));
}

// TODO(steveanton): Currently fails because payload type validation is not
// complete in AddSink (see note in rtp_demuxer.cc).
TEST_F(RtpDemuxerTest, DISABLED_RejectAddSinkForSamePayloadTypes) {
  constexpr uint8_t pt1 = 30;
  constexpr uint8_t pt2 = 31;

  RtpDemuxerCriteria pt1_pt2;
  pt1_pt2.payload_types() = {pt1, pt2};
  MockRtpPacketSink sink1;
  AddSink(pt1_pt2, &sink1);

  RtpDemuxerCriteria pt2_pt1;
  pt2_pt1.payload_types() = {pt2, pt1};
  MockRtpPacketSink sink2;
  EXPECT_FALSE(AddSink(pt2_pt1, &sink2));
}

// Routing Tests

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkBySsrc) {
  constexpr uint32_t ssrcs[] = {101, 202, 303};
  MockRtpPacketSink sinks[std::size(ssrcs)];
  for (size_t i = 0; i < std::size(ssrcs); i++) {
    AddSinkOnlySsrc(ssrcs[i], &sinks[i]);
  }

  for (size_t i = 0; i < std::size(ssrcs); i++) {
    auto packet = CreatePacketWithSsrc(ssrcs[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByRsid) {
  const std::string rsids[] = {"a", "b", "c"};
  MockRtpPacketSink sinks[std::size(rsids)];
  for (size_t i = 0; i < std::size(rsids); i++) {
    AddSinkOnlyRsid(rsids[i], &sinks[i]);
  }

  for (size_t i = 0; i < std::size(rsids); i++) {
    auto packet = CreatePacketWithSsrcRsid(checked_cast<uint32_t>(i), rsids[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByMid) {
  const std::string mids[] = {"a", "v", "s"};
  MockRtpPacketSink sinks[std::size(mids)];
  for (size_t i = 0; i < std::size(mids); i++) {
    AddSinkOnlyMid(mids[i], &sinks[i]);
  }

  for (size_t i = 0; i < std::size(mids); i++) {
    auto packet = CreatePacketWithSsrcMid(checked_cast<uint32_t>(i), mids[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByMidAndRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet = CreatePacketWithSsrcMidRsid(ssrc, mid, rsid);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByRepairedRsid) {
  const std::string rrid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkOnlyRsid(rrid, &sink);

  auto packet_with_rrid = CreatePacketWithSsrcRrid(ssrc, rrid);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_rrid))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_rrid));
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByPayloadType) {
  constexpr uint32_t ssrc = 10;
  constexpr uint8_t payload_type = 30;

  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;
  criteria.payload_types() = {payload_type};
  AddSink(criteria, &sink);

  auto packet = CreatePacketWithSsrc(ssrc);
  packet->SetPayloadType(payload_type);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, PacketsDeliveredInRightOrder) {
  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  std::unique_ptr<RtpPacketReceived> packets[5];
  for (size_t i = 0; i < std::size(packets); i++) {
    packets[i] = CreatePacketWithSsrc(ssrc);
    packets[i]->SetSequenceNumber(checked_cast<uint16_t>(i));
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }

  for (const auto& packet : packets) {
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, SinkMappedToMultipleSsrcs) {
  constexpr uint32_t ssrcs[] = {404, 505, 606};
  MockRtpPacketSink sink;
  for (uint32_t ssrc : ssrcs) {
    AddSinkOnlySsrc(ssrc, &sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : ssrcs) {
    auto packet = CreatePacketWithSsrc(ssrc);
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedBeforeFirstPacket) {
  constexpr uint32_t ssrc = 404;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  ASSERT_TRUE(RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedAfterFirstPacket) {
  constexpr uint32_t ssrc = 404;
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlySsrc(ssrc, &sink);

  InSequence sequence;
  for (size_t i = 0; i < 10; i++) {
    ASSERT_TRUE(demuxer_.OnRtpPacket(*CreatePacketWithSsrc(ssrc)));
  }

  ASSERT_TRUE(RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

// An SSRC may only be mapped to a single sink. However, since configuration
// of this associations might come from the network, we need to fail gracefully.
TEST_F(RtpDemuxerTest, OnlyOneSinkPerSsrcGetsOnRtpPacketTriggered) {
  MockRtpPacketSink sinks[3];
  constexpr uint32_t ssrc = 404;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sinks[0]));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sinks[1]));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sinks[2]));

  // The first sink associated with the SSRC remains active; other sinks
  // were not really added, and so do not get OnRtpPacket() called.
  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sinks[0], OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_CALL(sinks[1], OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sinks[2], OnRtpPacket(_)).Times(0);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoRepeatedCallbackOnRepeatedAddSinkForSameSink) {
  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink sink;

  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sink));

  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, RemoveSinkReturnsFalseForNeverAddedSink) {
  MockRtpPacketSink sink;
  EXPECT_FALSE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest, RemoveSinkReturnsTrueForPreviouslyAddedSsrcSink) {
  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest,
       RemoveSinkReturnsTrueForUnresolvedPreviouslyAddedRsidSink) {
  const std::string rsid = "a";
  MockRtpPacketSink sink;
  AddSinkOnlyRsid(rsid, &sink);

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest,
       RemoveSinkReturnsTrueForResolvedPreviouslyAddedRsidSink) {
  const std::string rsid = "a";
  constexpr uint32_t ssrc = 101;
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*CreatePacketWithSsrcRsid(ssrc, rsid)));

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest, RsidLearnedAndLaterPacketsDeliveredWithOnlySsrc) {
  MockRtpPacketSink sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  // Create a sequence of RTP packets, where only the first one actually
  // mentions the RSID.
  std::unique_ptr<RtpPacketReceived> packets[5];
  constexpr uint32_t rsid_ssrc = 111;
  packets[0] = CreatePacketWithSsrcRsid(rsid_ssrc, rsid);
  for (size_t i = 1; i < std::size(packets); i++) {
    packets[i] = CreatePacketWithSsrc(rsid_ssrc);
  }

  // The first packet associates the RSID with the SSRC, thereby allowing the
  // demuxer to correctly demux all of the packets.
  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }
  for (const auto& packet : packets) {
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedBeforeFirstPacket) {
  MockRtpPacketSink sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(RemoveSink(&sink));

  constexpr uint32_t ssrc = 111;
  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedAfterFirstPacket) {
  NiceMock<MockRtpPacketSink> sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  InSequence sequence;
  constexpr uint32_t ssrc = 111;
  for (size_t i = 0; i < 10; i++) {
    auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
    ASSERT_TRUE(demuxer_.OnRtpPacket(*packet));
  }

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(RemoveSink(&sink));

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoCallbackOnMidSinkRemovedBeforeFirstPacket) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);
  RemoveSink(&sink);

  auto packet = CreatePacketWithSsrcMid(ssrc, mid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoCallbackOnMidSinkRemovedAfterFirstPacket) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyMid(mid, &sink);

  auto p1 = CreatePacketWithSsrcMid(ssrc, mid);
  demuxer_.OnRtpPacket(*p1);

  RemoveSink(&sink);

  auto p2 = CreatePacketWithSsrcMid(ssrc, mid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*p2));
}

TEST_F(RtpDemuxerTest, NoCallbackOnMidRsidSinkRemovedAfterFirstPacket) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto p1 = CreatePacketWithSsrcMidRsid(ssrc, mid, rsid);
  demuxer_.OnRtpPacket(*p1);

  RemoveSink(&sink);

  auto p2 = CreatePacketWithSsrcMidRsid(ssrc, mid, rsid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*p2));
}

// The RSID to SSRC mapping should be one-to-one. If we end up receiving
// two (or more) packets with the same SSRC, but different RSIDs, we guarantee
// delivery to one of them but not both.
TEST_F(RtpDemuxerTest, FirstSsrcAssociatedWithAnRsidIsNotForgotten) {
  // Each sink has a distinct RSID.
  MockRtpPacketSink sink_a;
  const std::string rsid_a = "a";
  AddSinkOnlyRsid(rsid_a, &sink_a);

  MockRtpPacketSink sink_b;
  const std::string rsid_b = "b";
  AddSinkOnlyRsid(rsid_b, &sink_b);

  InSequence sequence;  // Verify that the order of delivery is unchanged.

  constexpr uint32_t shared_ssrc = 100;

  // First a packet with `rsid_a` is received, and `sink_a` is associated with
  // its SSRC.
  auto packet_a = CreatePacketWithSsrcRsid(shared_ssrc, rsid_a);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_a))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_a));

  // Second, a packet with `rsid_b` is received. We guarantee that `sink_b`
  // receives it.
  auto packet_b = CreatePacketWithSsrcRsid(shared_ssrc, rsid_b);
  EXPECT_CALL(sink_a, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink_b, OnRtpPacket(SamePacketAs(*packet_b))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_b));

  // Known edge-case; adding a new RSID association makes us re-examine all
  // SSRCs. `sink_b` may or may not be associated with the SSRC now; we make
  // no promises on that. However, since the RSID is specified and it cannot be
  // found the packet should be dropped.
  MockRtpPacketSink sink_c;
  const std::string rsid_c = "c";
  constexpr uint32_t some_other_ssrc = shared_ssrc + 1;
  AddSinkOnlySsrc(some_other_ssrc, &sink_c);

  auto packet_c = CreatePacketWithSsrcMid(shared_ssrc, rsid_c);
  EXPECT_CALL(sink_a, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink_b, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink_c, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet_c));
}

TEST_F(RtpDemuxerTest, MultipleRsidsOnSameSink) {
  MockRtpPacketSink sink;
  const std::string rsids[] = {"a", "b", "c"};

  for (const std::string& rsid : rsids) {
    AddSinkOnlyRsid(rsid, &sink);
  }

  InSequence sequence;
  for (size_t i = 0; i < std::size(rsids); i++) {
    // Assign different SSRCs and sequence numbers to all packets.
    const uint32_t ssrc = 1000 + static_cast<uint32_t>(i);
    const uint16_t sequence_number = 50 + static_cast<uint16_t>(i);
    auto packet = CreatePacketWithSsrcRsid(ssrc, rsids[i]);
    packet->SetSequenceNumber(sequence_number);
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

// RSIDs are given higher priority than SSRC because we believe senders are less
// likely to mislabel packets with RSID than mislabel them with SSRCs.
TEST_F(RtpDemuxerTest, SinkWithBothRsidAndSsrcAssociations) {
  MockRtpPacketSink sink;
  constexpr uint32_t standalone_ssrc = 10101;
  constexpr uint32_t rsid_ssrc = 20202;
  const std::string rsid = "1";

  AddSinkOnlySsrc(standalone_ssrc, &sink);
  AddSinkOnlyRsid(rsid, &sink);

  InSequence sequence;

  auto ssrc_packet = CreatePacketWithSsrc(standalone_ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*ssrc_packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*ssrc_packet));

  auto rsid_packet = CreatePacketWithSsrcRsid(rsid_ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*rsid_packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*rsid_packet));
}

// Packets are always guaranteed to be routed to only one sink.
TEST_F(RtpDemuxerTest, AssociatingByRsidAndBySsrcCannotTriggerDoubleCall) {
  constexpr uint32_t ssrc = 10101;
  const std::string rsid = "a";

  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);
  AddSinkOnlyRsid(rsid, &sink);

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

// If one sink is associated with SSRC x, and another sink with RSID y, then if
// we receive a packet with both SSRC x and RSID y, route that to only the sink
// for RSID y since we believe RSID tags to be more trustworthy than signaled
// SSRCs.
TEST_F(RtpDemuxerTest,
       PacketFittingBothRsidSinkAndSsrcSinkGivenOnlyToRsidSink) {
  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  MockRtpPacketSink rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);

  EXPECT_CALL(ssrc_sink, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(rsid_sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

// We're not expecting RSIDs to be resolved to SSRCs which were previously
// mapped to sinks, and make no guarantees except for graceful handling.
TEST_F(RtpDemuxerTest,
       GracefullyHandleRsidBeingMappedToPrevouslyAssociatedSsrc) {
  constexpr uint32_t ssrc = 111;
  NiceMock<MockRtpPacketSink> ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  // The SSRC was mapped to an SSRC sink, but was even active (packets flowed
  // over it).
  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  demuxer_.OnRtpPacket(*packet);

  // If the SSRC sink is ever removed, the RSID sink *might* receive indications
  // of packets, and observers *might* be informed. Only graceful handling
  // is guaranteed.
  RemoveSink(&ssrc_sink);
  EXPECT_CALL(rsid_sink, OnRtpPacket(SamePacketAs(*packet))).Times(AtLeast(0));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

// Tests that when one MID sink is configured, packets that include the MID
// extension will get routed to that sink and any packets that use the same
// SSRC as one of those packets later will also get routed to the sink, even
// if a new SSRC is introduced for the same MID.
TEST_F(RtpDemuxerTest, RoutedByMidWhenSsrcAdded) {
  const std::string mid = "v";
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyMid(mid, &sink);

  constexpr uint32_t ssrc1 = 10;
  constexpr uint32_t ssrc2 = 11;

  auto packet_ssrc1_mid = CreatePacketWithSsrcMid(ssrc1, mid);
  demuxer_.OnRtpPacket(*packet_ssrc1_mid);
  auto packet_ssrc2_mid = CreatePacketWithSsrcMid(ssrc2, mid);
  demuxer_.OnRtpPacket(*packet_ssrc2_mid);

  auto packet_ssrc1_only = CreatePacketWithSsrc(ssrc1);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_ssrc1_only))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_ssrc1_only));

  auto packet_ssrc2_only = CreatePacketWithSsrc(ssrc2);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_ssrc2_only))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_ssrc2_only));
}

TEST_F(RtpDemuxerTest, DontLearnMidSsrcBindingBeforeSinkAdded) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  auto packet_ssrc_mid = CreatePacketWithSsrcMid(ssrc, mid);
  ASSERT_FALSE(demuxer_.OnRtpPacket(*packet_ssrc_mid));

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);

  auto packet_ssrc_only = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet_ssrc_only));
}

TEST_F(RtpDemuxerTest, DontForgetMidSsrcBindingWhenSinkRemoved) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink1;
  AddSinkOnlyMid(mid, &sink1);

  auto packet_with_mid = CreatePacketWithSsrcMid(ssrc, mid);
  demuxer_.OnRtpPacket(*packet_with_mid);

  RemoveSink(&sink1);

  MockRtpPacketSink sink2;
  AddSinkOnlyMid(mid, &sink2);

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink2, OnRtpPacket(SamePacketAs(*packet_with_ssrc)));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

// If a sink is added with only a MID, then any packet with that MID no matter
// the RSID should be routed to that sink.
TEST_F(RtpDemuxerTest, RoutedByMidWithAnyRsid) {
  const std::string mid = "v";
  const std::string rsid1 = "1";
  const std::string rsid2 = "2";
  constexpr uint32_t ssrc1 = 10;
  constexpr uint32_t ssrc2 = 11;

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);

  InSequence sequence;

  auto packet_ssrc1_rsid1 = CreatePacketWithSsrcMidRsid(ssrc1, mid, rsid1);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_ssrc1_rsid1))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_ssrc1_rsid1));

  auto packet_ssrc2_rsid2 = CreatePacketWithSsrcMidRsid(ssrc2, mid, rsid2);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_ssrc2_rsid2))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_ssrc2_rsid2));
}

// These two tests verify that for a sink added with a MID, RSID pair, if the
// MID and RSID are learned in separate packets (e.g., because the header
// extensions are sent separately), then a later packet with just SSRC will get
// routed to that sink.
// The first test checks that the functionality works when MID is learned first.
// The second test checks that the functionality works when RSID is learned
// first.
TEST_F(RtpDemuxerTest, LearnMidThenRsidSeparatelyAndRouteBySsrc) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_mid = CreatePacketWithSsrcMid(ssrc, mid);
  ASSERT_FALSE(demuxer_.OnRtpPacket(*packet_with_mid));

  auto packet_with_rsid = CreatePacketWithSsrcRsid(ssrc, rsid);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*packet_with_rsid));

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_ssrc))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

TEST_F(RtpDemuxerTest, LearnRsidThenMidSeparatelyAndRouteBySsrc) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_rsid = CreatePacketWithSsrcRsid(ssrc, rsid);
  ASSERT_FALSE(demuxer_.OnRtpPacket(*packet_with_rsid));

  auto packet_with_mid = CreatePacketWithSsrcMid(ssrc, mid);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*packet_with_mid));

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_ssrc))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

TEST_F(RtpDemuxerTest, DontLearnMidRsidBindingBeforeSinkAdded) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  auto packet_with_both = CreatePacketWithSsrcMidRsid(ssrc, mid, rsid);
  ASSERT_FALSE(demuxer_.OnRtpPacket(*packet_with_both));

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

TEST_F(RtpDemuxerTest, DontForgetMidRsidBindingWhenSinkRemoved) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink1;
  AddSinkBothMidRsid(mid, rsid, &sink1);

  auto packet_with_both = CreatePacketWithSsrcMidRsid(ssrc, mid, rsid);
  demuxer_.OnRtpPacket(*packet_with_both);

  RemoveSink(&sink1);

  MockRtpPacketSink sink2;
  AddSinkBothMidRsid(mid, rsid, &sink2);

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink2, OnRtpPacket(SamePacketAs(*packet_with_ssrc)));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

TEST_F(RtpDemuxerTest, LearnMidRsidBindingAfterSinkAdded) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_both = CreatePacketWithSsrcMidRsid(ssrc, mid, rsid);
  demuxer_.OnRtpPacket(*packet_with_both);

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_ssrc)));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

TEST_F(RtpDemuxerTest, DropByPayloadTypeIfNoSink) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  auto packet = CreatePacketWithSsrc(ssrc);
  packet->SetPayloadType(payload_type);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

// For legacy applications, it's possible for us to demux if the payload type is
// unique. But if multiple sinks are registered with different MIDs and the same
// payload types, then we cannot route a packet with just payload type because
// it is ambiguous which sink it should be sent to.
TEST_F(RtpDemuxerTest, DropByPayloadTypeIfAddedInMultipleSinks) {
  const std::string mid1 = "v";
  const std::string mid2 = "a";
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria mid1_pt(mid1);
  mid1_pt.payload_types() = {payload_type};
  MockRtpPacketSink sink1;
  AddSink(mid1_pt, &sink1);

  RtpDemuxerCriteria mid2_pt(mid2);
  mid2_pt.payload_types() = {payload_type};
  MockRtpPacketSink sink2;
  AddSink(mid2_pt, &sink2);

  auto packet = CreatePacketWithSsrc(ssrc);
  packet->SetPayloadType(payload_type);

  EXPECT_CALL(sink1, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink2, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

// If two sinks are added with different MIDs but the same payload types, then
// we cannot demux on the payload type only unless one of the sinks is removed.
TEST_F(RtpDemuxerTest, RoutedByPayloadTypeIfAmbiguousSinkRemoved) {
  const std::string mid1 = "v";
  const std::string mid2 = "a";
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria mid1_pt(mid1);
  mid1_pt.payload_types().insert(payload_type);
  MockRtpPacketSink sink1;
  AddSink(mid1_pt, &sink1);

  RtpDemuxerCriteria mid2_pt(mid2);
  mid2_pt.payload_types().insert(payload_type);
  MockRtpPacketSink sink2;
  AddSink(mid2_pt, &sink2);

  RemoveSink(&sink1);

  auto packet = CreatePacketWithSsrc(ssrc);
  packet->SetPayloadType(payload_type);

  EXPECT_CALL(sink1, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink2, OnRtpPacket(SamePacketAs(*packet))).Times(1);

  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, MatchAnySinkReceivesAllPackets) {
  MockRtpPacketSink match_any_sink;
  auto match_any_criteria = RtpDemuxerCriteria::MatchAny();
  AddSink(match_any_criteria, &match_any_sink);

  // Packet that should go to the match_any sink.
  auto generic_packet = CreatePacketWithSsrc(456);
  EXPECT_CALL(match_any_sink, OnRtpPacket(SamePacketAs(*generic_packet)))
      .Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*generic_packet));
}

TEST_F(RtpDemuxerTest, AddingSpecificSinkFailsIfMatchAnyExists) {
  MockRtpPacketSink match_any_sink;
  auto match_any_criteria = RtpDemuxerCriteria::MatchAny();
  ASSERT_TRUE(AddSink(match_any_criteria, &match_any_sink));

  MockRtpPacketSink specific_sink;
  EXPECT_FALSE(AddSinkOnlySsrc(123, &specific_sink));
}

TEST_F(RtpDemuxerTest, AddingMatchAnySinkFailsIfSpecificSinkExists) {
  MockRtpPacketSink specific_sink;
  ASSERT_TRUE(AddSinkOnlySsrc(123, &specific_sink));

  MockRtpPacketSink match_any_sink;
  auto match_any_criteria = RtpDemuxerCriteria::MatchAny();
  EXPECT_FALSE(AddSink(match_any_criteria, &match_any_sink));
}

TEST_F(RtpDemuxerTest, RoutedByPayloadTypeLatchesSsrc) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria pt;
  pt.payload_types().insert(payload_type);
  NiceMock<MockRtpPacketSink> sink;
  AddSink(pt, &sink);

  auto packet_with_pt = CreatePacketWithSsrc(ssrc);
  packet_with_pt->SetPayloadType(payload_type);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*packet_with_pt));

  auto packet_with_ssrc = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_ssrc))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_ssrc));
}

// RSIDs are scoped within MID, so if two sinks are registered with the same
// RSIDs but different MIDs, then packets containing both extensions should be
// routed to the correct one.
TEST_F(RtpDemuxerTest, PacketWithSameRsidDifferentMidRoutedToProperSink) {
  const std::string mid1 = "mid1";
  const std::string mid2 = "mid2";
  const std::string rsid = "rsid";
  constexpr uint32_t ssrc1 = 10;
  constexpr uint32_t ssrc2 = 11;

  NiceMock<MockRtpPacketSink> mid1_sink;
  AddSinkBothMidRsid(mid1, rsid, &mid1_sink);

  MockRtpPacketSink mid2_sink;
  AddSinkBothMidRsid(mid2, rsid, &mid2_sink);

  auto packet_mid1 = CreatePacketWithSsrcMidRsid(ssrc1, mid1, rsid);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*packet_mid1));

  auto packet_mid2 = CreatePacketWithSsrcMidRsid(ssrc2, mid2, rsid);
  EXPECT_CALL(mid2_sink, OnRtpPacket(SamePacketAs(*packet_mid2))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_mid2));
}

// If a sink is first bound to a given SSRC by signaling but later a new sink is
// bound to a given MID by a later signaling, then when a packet arrives with
// both the SSRC and MID, then the signaled MID sink should take precedence.
TEST_F(RtpDemuxerTest, SignaledMidShouldOverwriteSignaledSsrc) {
  constexpr uint32_t ssrc = 11;
  const std::string mid = "mid";

  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  MockRtpPacketSink mid_sink;
  AddSinkOnlyMid(mid, &mid_sink);

  auto p = CreatePacketWithSsrcMid(ssrc, mid);
  EXPECT_CALL(ssrc_sink, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(mid_sink, OnRtpPacket(SamePacketAs(*p))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*p));
}

// Extends the previous test to also ensure that later packets that do not
// specify MID are still routed to the MID sink rather than the overwritten SSRC
// sink.
TEST_F(RtpDemuxerTest, SignaledMidShouldOverwriteSignalledSsrcPersistent) {
  constexpr uint32_t ssrc = 11;
  const std::string mid = "mid";

  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  NiceMock<MockRtpPacketSink> mid_sink;
  AddSinkOnlyMid(mid, &mid_sink);

  EXPECT_CALL(ssrc_sink, OnRtpPacket(_)).Times(0);

  auto packet_with_mid = CreatePacketWithSsrcMid(ssrc, mid);
  demuxer_.OnRtpPacket(*packet_with_mid);

  auto packet_without_mid = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(mid_sink, OnRtpPacket(SamePacketAs(*packet_without_mid)))
      .Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_without_mid));
}

TEST_F(RtpDemuxerTest, RouteByPayloadTypeMultipleMatch) {
  constexpr uint32_t ssrc = 10;
  constexpr uint8_t pt1 = 30;
  constexpr uint8_t pt2 = 31;

  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;
  criteria.payload_types() = {pt1, pt2};
  AddSink(criteria, &sink);

  auto packet_with_pt1 = CreatePacketWithSsrc(ssrc);
  packet_with_pt1->SetPayloadType(pt1);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_pt1)));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_pt1));

  auto packet_with_pt2 = CreatePacketWithSsrc(ssrc);
  packet_with_pt2->SetPayloadType(pt2);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet_with_pt2)));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_with_pt2));
}

TEST_F(RtpDemuxerTest, DontDemuxOnMidAloneIfAddedWithRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);

  auto packet = CreatePacketWithSsrcMid(ssrc, mid);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, DemuxBySsrcEvenWithMidAndRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria criteria(mid, rsid);
  criteria.ssrcs().insert(ssrc);
  MockRtpPacketSink sink;
  AddSink(criteria, &sink);

  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

// In slight deviation from the BUNDLE spec, if we match a sink according to
// SSRC, then we do not verify payload type against the criteria and defer to
// the sink to check that it is correct.
TEST_F(RtpDemuxerTest, DoNotCheckPayloadTypeIfMatchedByOtherCriteria) {
  constexpr uint32_t ssrc = 10;
  constexpr uint8_t payload_type = 30;
  constexpr uint8_t different_payload_type = payload_type + 1;

  RtpDemuxerCriteria criteria;
  criteria.ssrcs().insert(ssrc);
  criteria.payload_types().insert(payload_type);
  MockRtpPacketSink sink;
  AddSink(criteria, &sink);

  auto packet = CreatePacketWithSsrc(ssrc);
  packet->SetPayloadType(different_payload_type);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

// If a repair packet includes an RSID it should be ignored and the packet
// should be routed by its RRID.
TEST_F(RtpDemuxerTest, PacketWithRsidAndRridRoutedByRrid) {
  const std::string rsid = "1";
  const std::string rrid = "1r";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink_rsid;
  AddSinkOnlyRsid(rsid, &sink_rsid);

  MockRtpPacketSink sink_rrid;
  AddSinkOnlyRsid(rrid, &sink_rrid);

  auto packet = CreatePacketWithSsrcRsidRrid(ssrc, rsid, rrid);
  EXPECT_CALL(sink_rsid, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink_rrid, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

// Same test as above but checks that the latched SSRC routes to the RRID sink.
TEST_F(RtpDemuxerTest, PacketWithRsidAndRridLatchesSsrcToRrid) {
  const std::string rsid = "1";
  const std::string rrid = "1r";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink_rsid;
  AddSinkOnlyRsid(rsid, &sink_rsid);

  NiceMock<MockRtpPacketSink> sink_rrid;
  AddSinkOnlyRsid(rrid, &sink_rrid);

  auto packet_rsid_rrid = CreatePacketWithSsrcRsidRrid(ssrc, rsid, rrid);
  demuxer_.OnRtpPacket(*packet_rsid_rrid);

  auto packet_ssrc_only = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink_rsid, OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sink_rrid, OnRtpPacket(SamePacketAs(*packet_ssrc_only))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_ssrc_only));
}

// Tests that a packet which includes MID and RSID is dropped and not routed by
// SSRC if the MID and RSID do not match an added sink.
TEST_F(RtpDemuxerTest, PacketWithMidAndUnknownRsidIsNotRoutedBySsrc) {
  constexpr uint32_t ssrc = 10;
  const std::string mid = "v";
  const std::string rsid = "1";
  const std::string wrong_rsid = "2";

  RtpDemuxerCriteria criteria(mid, rsid);
  criteria.ssrcs().insert(ssrc);
  MockRtpPacketSink sink;
  AddSink(criteria, &sink);

  auto packet = CreatePacketWithSsrcMidRsid(ssrc, mid, wrong_rsid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

// Tests that a packet which includes MID and RSID is dropped and not routed by
// payload type if the MID and RSID do not match an added sink.
TEST_F(RtpDemuxerTest, PacketWithMidAndUnknownRsidIsNotRoutedByPayloadType) {
  constexpr uint32_t ssrc = 10;
  const std::string mid = "v";
  const std::string rsid = "1";
  const std::string wrong_rsid = "2";
  constexpr uint8_t payload_type = 30;

  RtpDemuxerCriteria criteria(mid, rsid);
  criteria.payload_types().insert(payload_type);
  MockRtpPacketSink sink;
  AddSink(criteria, &sink);

  auto packet = CreatePacketWithSsrcMidRsid(ssrc, mid, wrong_rsid);
  packet->SetPayloadType(payload_type);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST_F(RtpDemuxerDeathTest, MidMustNotExceedMaximumLength) {
  MockRtpPacketSink sink1;
  std::string mid1(BaseRtpStringExtension::kMaxValueSizeBytes + 1, 'a');
  EXPECT_DEATH(AddSinkOnlyMid(mid1, &sink1), "");
}

TEST_F(RtpDemuxerDeathTest, CriteriaMustBeNonEmpty) {
  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;
  EXPECT_DEATH(AddSink(criteria, &sink), "");
}

TEST_F(RtpDemuxerDeathTest, RsidMustBeAlphaNumeric) {
  MockRtpPacketSink sink;
  EXPECT_DEATH(AddSinkOnlyRsid("a_3", &sink), "");
}

TEST_F(RtpDemuxerDeathTest, MidMustBeToken) {
  MockRtpPacketSink sink;
  EXPECT_DEATH(AddSinkOnlyMid("a(3)", &sink), "");
}

TEST_F(RtpDemuxerDeathTest, RsidMustNotExceedMaximumLength) {
  MockRtpPacketSink sink;
  std::string rsid(BaseRtpStringExtension::kMaxValueSizeBytes + 1, 'a');
  EXPECT_DEATH(AddSinkOnlyRsid(rsid, &sink), "");
}

#endif

}  // namespace
}  // namespace webrtc
