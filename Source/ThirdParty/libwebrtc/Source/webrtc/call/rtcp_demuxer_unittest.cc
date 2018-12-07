/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtcp_demuxer.h"

#include <memory>
#include <set>

#include "absl/memory/memory.h"
#include "api/rtp_headers.h"
#include "call/rtcp_packet_sink_interface.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::NiceMock;

class MockRtcpPacketSink : public RtcpPacketSinkInterface {
 public:
  MOCK_METHOD1(OnRtcpPacket, void(rtc::ArrayView<const uint8_t>));
};

class RtcpDemuxerTest : public testing::Test {
 protected:
  ~RtcpDemuxerTest() {
    for (auto* sink : sinks_to_tear_down_) {
      demuxer_.RemoveSink(sink);
    }
    for (auto* sink : broadcast_sinks_to_tear_down_) {
      demuxer_.RemoveBroadcastSink(sink);
    }
  }

  void AddSsrcSink(uint32_t ssrc, RtcpPacketSinkInterface* sink) {
    demuxer_.AddSink(ssrc, sink);
    sinks_to_tear_down_.insert(sink);
  }

  void AddRsidSink(const std::string& rsid, RtcpPacketSinkInterface* sink) {
    demuxer_.AddSink(rsid, sink);
    sinks_to_tear_down_.insert(sink);
  }

  void RemoveSink(RtcpPacketSinkInterface* sink) {
    sinks_to_tear_down_.erase(sink);
    demuxer_.RemoveSink(sink);
  }

  void AddBroadcastSink(RtcpPacketSinkInterface* sink) {
    demuxer_.AddBroadcastSink(sink);
    broadcast_sinks_to_tear_down_.insert(sink);
  }

  void RemoveBroadcastSink(RtcpPacketSinkInterface* sink) {
    broadcast_sinks_to_tear_down_.erase(sink);
    demuxer_.RemoveBroadcastSink(sink);
  }

  RtcpDemuxer demuxer_;
  std::set<RtcpPacketSinkInterface*> sinks_to_tear_down_;
  std::set<RtcpPacketSinkInterface*> broadcast_sinks_to_tear_down_;
};

// Produces a packet buffer representing an RTCP packet with a given SSRC,
// as it would look when sent over the wire.
// |distinguishing_string| allows different RTCP packets with the same SSRC
// to be distinguished. How this is set into the actual packet is
// unimportant, and depends on which RTCP message we choose to use.
rtc::Buffer CreateRtcpPacket(uint32_t ssrc,
                             const std::string& distinguishing_string = "") {
  rtcp::Bye packet;
  packet.SetSenderSsrc(ssrc);
  if (distinguishing_string != "") {
    // Actual way we use |distinguishing_string| is unimportant, so long
    // as it ends up in the packet.
    packet.SetReason(distinguishing_string);
  }
  return packet.Build();
}

static Matcher<rtc::ArrayView<const uint8_t>> SamePacketAs(
    const rtc::Buffer& other) {
  return ElementsAreArray(other.cbegin(), other.cend());
}

}  // namespace

TEST_F(RtcpDemuxerTest, OnRtcpPacketCalledOnCorrectSinkBySsrc) {
  constexpr uint32_t ssrcs[] = {101, 202, 303};
  MockRtcpPacketSink sinks[arraysize(ssrcs)];
  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    AddSsrcSink(ssrcs[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    auto packet = CreateRtcpPacket(ssrcs[i]);
    EXPECT_CALL(sinks[i], OnRtcpPacket(SamePacketAs(packet))).Times(1);
    demuxer_.OnRtcpPacket(packet);
  }
}

TEST_F(RtcpDemuxerTest, OnRtcpPacketCalledOnResolvedRsidSink) {
  // Set up some RSID sinks.
  const std::string rsids[] = {"a", "b", "c"};
  MockRtcpPacketSink sinks[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    AddRsidSink(rsids[i], &sinks[i]);
  }

  // Only resolve one of the sinks.
  constexpr size_t resolved_sink_index = 0;
  constexpr uint32_t ssrc = 345;
  demuxer_.OnSsrcBoundToRsid(rsids[resolved_sink_index], ssrc);

  // The resolved sink gets notifications of RTCP messages with its SSRC.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sinks[resolved_sink_index], OnRtcpPacket(SamePacketAs(packet)))
      .Times(1);

  // RTCP received; expected calls triggered.
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest,
       SingleCallbackAfterResolutionOfAnRsidToAlreadyRegisteredSsrc) {
  // Associate a sink with an SSRC.
  MockRtcpPacketSink sink;
  constexpr uint32_t ssrc = 999;
  AddSsrcSink(ssrc, &sink);

  // Associate the same sink with an RSID.
  const std::string rsid = "r";
  AddRsidSink(rsid, &sink);

  // Resolve the RSID to the aforementioned SSRC.
  demuxer_.OnSsrcBoundToRsid(rsid, ssrc);

  // OnRtcpPacket still called only a single time for messages with this SSRC.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet))).Times(1);
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest,
       OnRtcpPacketCalledOnAllBroadcastSinksForAllRtcpPackets) {
  MockRtcpPacketSink sinks[3];
  for (MockRtcpPacketSink& sink : sinks) {
    AddBroadcastSink(&sink);
  }

  constexpr uint32_t ssrc = 747;
  auto packet = CreateRtcpPacket(ssrc);

  for (MockRtcpPacketSink& sink : sinks) {
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet))).Times(1);
  }

  // RTCP received; expected calls triggered.
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, PacketsDeliveredInRightOrderToNonBroadcastSink) {
  constexpr uint32_t ssrc = 101;
  MockRtcpPacketSink sink;
  AddSsrcSink(ssrc, &sink);

  std::vector<rtc::Buffer> packets;
  for (size_t i = 0; i < 5; i++) {
    packets.push_back(CreateRtcpPacket(ssrc, std::to_string(i)));
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet))).Times(1);
  }

  for (const auto& packet : packets) {
    demuxer_.OnRtcpPacket(packet);
  }
}

TEST_F(RtcpDemuxerTest, PacketsDeliveredInRightOrderToBroadcastSink) {
  MockRtcpPacketSink sink;
  AddBroadcastSink(&sink);

  std::vector<rtc::Buffer> packets;
  for (size_t i = 0; i < 5; i++) {
    constexpr uint32_t ssrc = 101;
    packets.push_back(CreateRtcpPacket(ssrc, std::to_string(i)));
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet))).Times(1);
  }

  for (const auto& packet : packets) {
    demuxer_.OnRtcpPacket(packet);
  }
}

TEST_F(RtcpDemuxerTest, MultipleSinksMappedToSameSsrc) {
  MockRtcpPacketSink sinks[3];
  constexpr uint32_t ssrc = 404;
  for (auto& sink : sinks) {
    AddSsrcSink(ssrc, &sink);
  }

  // Reception of an RTCP packet associated with the shared SSRC triggers the
  // callback on all of the sinks associated with it.
  auto packet = CreateRtcpPacket(ssrc);
  for (auto& sink : sinks) {
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet)));
  }

  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, SinkMappedToMultipleSsrcs) {
  constexpr uint32_t ssrcs[] = {404, 505, 606};
  MockRtcpPacketSink sink;
  for (uint32_t ssrc : ssrcs) {
    AddSsrcSink(ssrc, &sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : ssrcs) {
    auto packet = CreateRtcpPacket(ssrc);
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet)));
    demuxer_.OnRtcpPacket(packet);
  }
}

TEST_F(RtcpDemuxerTest, MultipleRsidsOnSameSink) {
  // Sink associated with multiple sinks.
  MockRtcpPacketSink sink;
  const std::string rsids[] = {"a", "b", "c"};
  for (const auto& rsid : rsids) {
    AddRsidSink(rsid, &sink);
  }

  // RSIDs resolved to SSRCs.
  uint32_t ssrcs[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    ssrcs[i] = 1000 + static_cast<uint32_t>(i);
    demuxer_.OnSsrcBoundToRsid(rsids[i], ssrcs[i]);
  }

  // Set up packets to match those RSIDs/SSRCs.
  std::vector<rtc::Buffer> packets;
  for (size_t i = 0; i < arraysize(rsids); i++) {
    packets.push_back(CreateRtcpPacket(ssrcs[i]));
  }

  // The sink expects to receive all of the packets.
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet))).Times(1);
  }

  // Packet demuxed correctly; OnRtcpPacket() triggered on sink.
  for (const auto& packet : packets) {
    demuxer_.OnRtcpPacket(packet);
  }
}

TEST_F(RtcpDemuxerTest, RsidUsedByMultipleSinks) {
  MockRtcpPacketSink sinks[3];
  const std::string shared_rsid = "a";

  for (MockRtcpPacketSink& sink : sinks) {
    AddRsidSink(shared_rsid, &sink);
  }

  constexpr uint32_t shared_ssrc = 888;
  demuxer_.OnSsrcBoundToRsid(shared_rsid, shared_ssrc);

  auto packet = CreateRtcpPacket(shared_ssrc);

  for (MockRtcpPacketSink& sink : sinks) {
    EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet))).Times(1);
  }

  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, NoCallbackOnSsrcSinkRemovedBeforeFirstPacket) {
  constexpr uint32_t ssrc = 404;
  MockRtcpPacketSink sink;
  AddSsrcSink(ssrc, &sink);

  RemoveSink(&sink);

  // The removed sink does not get callbacks.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, NoCallbackOnSsrcSinkRemovedAfterFirstPacket) {
  constexpr uint32_t ssrc = 404;
  NiceMock<MockRtcpPacketSink> sink;
  AddSsrcSink(ssrc, &sink);

  auto before_packet = CreateRtcpPacket(ssrc);
  demuxer_.OnRtcpPacket(before_packet);

  RemoveSink(&sink);

  // The removed sink does not get callbacks.
  auto after_packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer_.OnRtcpPacket(after_packet);
}

TEST_F(RtcpDemuxerTest, NoCallbackOnRsidSinkRemovedBeforeRsidResolution) {
  const std::string rsid = "a";
  constexpr uint32_t ssrc = 404;
  MockRtcpPacketSink sink;
  AddRsidSink(rsid, &sink);

  // Removal before resolution.
  RemoveSink(&sink);
  demuxer_.OnSsrcBoundToRsid(rsid, ssrc);

  // The removed sink does not get callbacks.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, NoCallbackOnRsidSinkRemovedAfterRsidResolution) {
  const std::string rsid = "a";
  constexpr uint32_t ssrc = 404;
  MockRtcpPacketSink sink;
  AddRsidSink(rsid, &sink);

  // Removal after resolution.
  demuxer_.OnSsrcBoundToRsid(rsid, ssrc);
  RemoveSink(&sink);

  // The removed sink does not get callbacks.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, NoCallbackOnBroadcastSinkRemovedBeforeFirstPacket) {
  MockRtcpPacketSink sink;
  AddBroadcastSink(&sink);

  RemoveBroadcastSink(&sink);

  // The removed sink does not get callbacks.
  constexpr uint32_t ssrc = 404;
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer_.OnRtcpPacket(packet);
}

TEST_F(RtcpDemuxerTest, NoCallbackOnBroadcastSinkRemovedAfterFirstPacket) {
  NiceMock<MockRtcpPacketSink> sink;
  AddBroadcastSink(&sink);

  constexpr uint32_t ssrc = 404;
  auto before_packet = CreateRtcpPacket(ssrc);
  demuxer_.OnRtcpPacket(before_packet);

  RemoveBroadcastSink(&sink);

  // The removed sink does not get callbacks.
  auto after_packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer_.OnRtcpPacket(after_packet);
}

// The RSID to SSRC mapping should be one-to-one. If we end up receiving
// two (or more) packets with the same SSRC, but different RSIDs, we guarantee
// remembering the first one; no guarantees are made about further associations.
TEST_F(RtcpDemuxerTest, FirstResolutionOfRsidNotForgotten) {
  MockRtcpPacketSink sink;
  const std::string rsid = "a";
  AddRsidSink(rsid, &sink);

  constexpr uint32_t ssrc_a = 111;  // First resolution - guaranteed effective.
  demuxer_.OnSsrcBoundToRsid(rsid, ssrc_a);

  constexpr uint32_t ssrc_b = 222;  // Second resolution - no guarantees.
  demuxer_.OnSsrcBoundToRsid(rsid, ssrc_b);

  auto packet_a = CreateRtcpPacket(ssrc_a);
  EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet_a))).Times(1);
  demuxer_.OnRtcpPacket(packet_a);

  auto packet_b = CreateRtcpPacket(ssrc_b);
  EXPECT_CALL(sink, OnRtcpPacket(SamePacketAs(packet_b))).Times(AtLeast(0));
  demuxer_.OnRtcpPacket(packet_b);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST_F(RtcpDemuxerTest, RepeatedSsrcToSinkAssociationsDisallowed) {
  MockRtcpPacketSink sink;

  constexpr uint32_t ssrc = 101;
  AddSsrcSink(ssrc, &sink);
  EXPECT_DEATH(AddSsrcSink(ssrc, &sink), "");
}

TEST_F(RtcpDemuxerTest, RepeatedRsidToSinkAssociationsDisallowed) {
  MockRtcpPacketSink sink;

  const std::string rsid = "z";
  AddRsidSink(rsid, &sink);
  EXPECT_DEATH(AddRsidSink(rsid, &sink), "");
}

TEST_F(RtcpDemuxerTest, RepeatedBroadcastSinkRegistrationDisallowed) {
  MockRtcpPacketSink sink;

  AddBroadcastSink(&sink);
  EXPECT_DEATH(AddBroadcastSink(&sink), "");
}

TEST_F(RtcpDemuxerTest, SsrcSinkCannotAlsoBeRegisteredAsBroadcast) {
  MockRtcpPacketSink sink;

  constexpr uint32_t ssrc = 101;
  AddSsrcSink(ssrc, &sink);
  EXPECT_DEATH(AddBroadcastSink(&sink), "");
}

TEST_F(RtcpDemuxerTest, RsidSinkCannotAlsoBeRegisteredAsBroadcast) {
  MockRtcpPacketSink sink;

  const std::string rsid = "z";
  AddRsidSink(rsid, &sink);
  EXPECT_DEATH(AddBroadcastSink(&sink), "");
}

TEST_F(RtcpDemuxerTest, BroadcastSinkCannotAlsoBeRegisteredAsSsrcSink) {
  MockRtcpPacketSink sink;

  AddBroadcastSink(&sink);
  constexpr uint32_t ssrc = 101;
  EXPECT_DEATH(AddSsrcSink(ssrc, &sink), "");
}

TEST_F(RtcpDemuxerTest, BroadcastSinkCannotAlsoBeRegisteredAsRsidSink) {
  MockRtcpPacketSink sink;

  AddBroadcastSink(&sink);
  const std::string rsid = "j";
  EXPECT_DEATH(AddRsidSink(rsid, &sink), "");
}

TEST_F(RtcpDemuxerTest, MayNotCallRemoveSinkOnNeverAddedSink) {
  MockRtcpPacketSink sink;
  EXPECT_DEATH(RemoveSink(&sink), "");
}

TEST_F(RtcpDemuxerTest, MayNotCallRemoveBroadcastSinkOnNeverAddedSink) {
  MockRtcpPacketSink sink;
  EXPECT_DEATH(RemoveBroadcastSink(&sink), "");
}

TEST_F(RtcpDemuxerTest, RsidMustBeNonEmpty) {
  MockRtcpPacketSink sink;
  EXPECT_DEATH(AddRsidSink("", &sink), "");
}

TEST_F(RtcpDemuxerTest, RsidMustBeAlphaNumeric) {
  MockRtcpPacketSink sink;
  EXPECT_DEATH(AddRsidSink("a_3", &sink), "");
}

TEST_F(RtcpDemuxerTest, RsidMustNotExceedMaximumLength) {
  MockRtcpPacketSink sink;
  std::string rsid(StreamId::kMaxSize + 1, 'a');
  EXPECT_DEATH(AddRsidSink(rsid, &sink), "");
}

#endif

}  // namespace webrtc
