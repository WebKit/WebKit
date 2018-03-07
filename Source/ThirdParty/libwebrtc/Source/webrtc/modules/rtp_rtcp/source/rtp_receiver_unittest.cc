/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "modules/rtp_rtcp/include/rtp_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_receiver_impl.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::UnorderedElementsAre;

const uint32_t kTestRate = 64000u;
const uint8_t kTestPayload[] = {'t', 'e', 's', 't'};
const uint8_t kPcmuPayloadType = 96;
const int64_t kGetSourcesTimeoutMs = 10000;
const uint32_t kSsrc1 = 123;
const uint32_t kSsrc2 = 124;
const uint32_t kCsrc1 = 111;
const uint32_t kCsrc2 = 222;

static uint32_t rtp_timestamp(int64_t time_ms) {
  return static_cast<uint32_t>(time_ms * kTestRate / 1000);
}

}  // namespace

class RtpReceiverTest : public ::testing::Test {
 protected:
  RtpReceiverTest()
      : fake_clock_(123456),
        rtp_receiver_(
            RtpReceiver::CreateAudioReceiver(&fake_clock_,
                                             &mock_rtp_data_,
                                             nullptr,
                                             &rtp_payload_registry_)) {
    rtp_receiver_->RegisterReceivePayload(kPcmuPayloadType,
                                          SdpAudioFormat("PCMU", 8000, 1));
  }
  ~RtpReceiverTest() {}

  bool FindSourceByIdAndType(const std::vector<RtpSource>& sources,
                             uint32_t source_id,
                             RtpSourceType type,
                             RtpSource* source) {
    for (size_t i = 0; i < sources.size(); ++i) {
      if (sources[i].source_id() == source_id &&
          sources[i].source_type() == type) {
        (*source) = sources[i];
        return true;
      }
    }
    return false;
  }

  SimulatedClock fake_clock_;
  NiceMock<MockRtpData> mock_rtp_data_;
  RTPPayloadRegistry rtp_payload_registry_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
};

TEST_F(RtpReceiverTest, GetSources) {
  int64_t now_ms = fake_clock_.TimeInMilliseconds();

  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(now_ms);
  header.numCSRCs = 2;
  header.arrOfCSRCs[0] = kCsrc1;
  header.arrOfCSRCs[1] = kCsrc2;
  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  auto sources = rtp_receiver_->GetSources();
  // One SSRC source and two CSRC sources.
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC),
                           RtpSource(now_ms, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(now_ms, kCsrc2, RtpSourceType::CSRC)));

  // Advance the fake clock and the method is expected to return the
  // contributing source object with same source id and updated timestamp.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  now_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC),
                           RtpSource(now_ms, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(now_ms, kCsrc2, RtpSourceType::CSRC)));

  // Test the edge case that the sources are still there just before the
  // timeout.
  int64_t prev_time_ms = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources,
              UnorderedElementsAre(
                  RtpSource(prev_time_ms, kSsrc1, RtpSourceType::SSRC),
                  RtpSource(prev_time_ms, kCsrc1, RtpSourceType::CSRC),
                  RtpSource(prev_time_ms, kCsrc2, RtpSourceType::CSRC)));

  // Time out.
  fake_clock_.AdvanceTimeMilliseconds(1);
  sources = rtp_receiver_->GetSources();
  // All the sources should be out of date.
  ASSERT_EQ(0u, sources.size());
}

// Test the case that the SSRC is changed.
TEST_F(RtpReceiverTest, GetSourcesChangeSSRC) {
  int64_t prev_time_ms = -1;
  int64_t now_ms = fake_clock_.TimeInMilliseconds();

  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(now_ms);
  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  auto sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC)));

  // The SSRC is changed and the old SSRC is expected to be returned.
  fake_clock_.AdvanceTimeMilliseconds(100);
  prev_time_ms = now_ms;
  now_ms = fake_clock_.TimeInMilliseconds();
  header.ssrc = kSsrc2;
  header.timestamp = rtp_timestamp(now_ms);
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(prev_time_ms, kSsrc1, RtpSourceType::SSRC),
                           RtpSource(now_ms, kSsrc2, RtpSourceType::SSRC)));

  // The SSRC is changed again and happen to be changed back to 1. No
  // duplication is expected.
  fake_clock_.AdvanceTimeMilliseconds(100);
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(now_ms);
  prev_time_ms = now_ms;
  now_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(prev_time_ms, kSsrc2, RtpSourceType::SSRC),
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC)));

  // Old SSRC source timeout.
  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  now_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC)));
}

TEST_F(RtpReceiverTest, GetSourcesRemoveOutdatedSource) {
  int64_t now_ms = fake_clock_.TimeInMilliseconds();

  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.timestamp = rtp_timestamp(now_ms);
  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};
  header.numCSRCs = 1;
  size_t kSourceListSize = 20;

  for (size_t i = 0; i < kSourceListSize; ++i) {
    header.ssrc = i;
    header.arrOfCSRCs[0] = (i + 1);
    EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
        header, kTestPayload, sizeof(kTestPayload), payload_specific));
  }

  RtpSource source(0, 0, RtpSourceType::SSRC);
  auto sources = rtp_receiver_->GetSources();
  // Expect |kSourceListSize| SSRC sources and |kSourceListSize| CSRC sources.
  ASSERT_EQ(2 * kSourceListSize, sources.size());
  for (size_t i = 0; i < kSourceListSize; ++i) {
    // The SSRC source IDs are expected to be 19, 18, 17 ... 0
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, i, RtpSourceType::SSRC, &source));
    EXPECT_EQ(now_ms, source.timestamp_ms());

    // The CSRC source IDs are expected to be 20, 19, 18 ... 1
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, (i + 1), RtpSourceType::CSRC, &source));
    EXPECT_EQ(now_ms, source.timestamp_ms());
  }

  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  for (size_t i = 0; i < kSourceListSize; ++i) {
    // The SSRC source IDs are expected to be 19, 18, 17 ... 0
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, i, RtpSourceType::SSRC, &source));
    EXPECT_EQ(now_ms, source.timestamp_ms());

    // The CSRC source IDs are expected to be 20, 19, 18 ... 1
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, (i + 1), RtpSourceType::CSRC, &source));
    EXPECT_EQ(now_ms, source.timestamp_ms());
  }

  // Timeout. All the existing objects are out of date and are expected to be
  // removed.
  fake_clock_.AdvanceTimeMilliseconds(1);
  header.ssrc = kSsrc1;
  header.arrOfCSRCs[0] = kCsrc1;
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  auto rtp_receiver_impl = static_cast<RtpReceiverImpl*>(rtp_receiver_.get());
  auto ssrc_sources = rtp_receiver_impl->ssrc_sources_for_testing();
  ASSERT_EQ(1u, ssrc_sources.size());
  EXPECT_EQ(kSsrc1, ssrc_sources.begin()->source_id());
  EXPECT_EQ(RtpSourceType::SSRC, ssrc_sources.begin()->source_type());
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(),
            ssrc_sources.begin()->timestamp_ms());

  auto csrc_sources = rtp_receiver_impl->csrc_sources_for_testing();
  ASSERT_EQ(1u, csrc_sources.size());
  EXPECT_EQ(kCsrc1, csrc_sources.begin()->source_id());
  EXPECT_EQ(RtpSourceType::CSRC, csrc_sources.begin()->source_type());
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(),
            csrc_sources.begin()->timestamp_ms());
}

// The audio level from the RTPHeader extension should be stored in the
// RtpSource with the matching SSRC.
TEST_F(RtpReceiverTest, GetSourcesContainsAudioLevelExtension) {
  RTPHeader header;
  int64_t time1_ms = fake_clock_.TimeInMilliseconds();
  header.payloadType = kPcmuPayloadType;
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(time1_ms);
  header.extension.hasAudioLevel = true;
  header.extension.audioLevel = 10;
  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  auto sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(RtpSource(
                           time1_ms, kSsrc1, RtpSourceType::SSRC, 10)));

  // Receive a packet from a different SSRC with a different level and check
  // that they are both remembered.
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t time2_ms = fake_clock_.TimeInMilliseconds();
  header.ssrc = kSsrc2;
  header.timestamp = rtp_timestamp(time2_ms);
  header.extension.hasAudioLevel = true;
  header.extension.audioLevel = 20;

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources,
              UnorderedElementsAre(
                  RtpSource(time1_ms, kSsrc1, RtpSourceType::SSRC, 10),
                  RtpSource(time2_ms, kSsrc2, RtpSourceType::SSRC, 20)));

  // Receive a packet from the first SSRC again and check that the level is
  // updated.
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t time3_ms = fake_clock_.TimeInMilliseconds();
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(time3_ms);
  header.extension.hasAudioLevel = true;
  header.extension.audioLevel = 30;

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources,
              UnorderedElementsAre(
                  RtpSource(time3_ms, kSsrc1, RtpSourceType::SSRC, 30),
                  RtpSource(time2_ms, kSsrc2, RtpSourceType::SSRC, 20)));
}

TEST_F(RtpReceiverTest,
       MissingAudioLevelHeaderExtensionClearsRtpSourceAudioLevel) {
  RTPHeader header;
  int64_t time1_ms = fake_clock_.TimeInMilliseconds();
  header.payloadType = kPcmuPayloadType;
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(time1_ms);
  header.extension.hasAudioLevel = true;
  header.extension.audioLevel = 10;
  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  auto sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(RtpSource(
                           time1_ms, kSsrc1, RtpSourceType::SSRC, 10)));

  // Receive a second packet without the audio level header extension and check
  // that the audio level is cleared.
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t time2_ms = fake_clock_.TimeInMilliseconds();
  header.timestamp = rtp_timestamp(time2_ms);
  header.extension.hasAudioLevel = false;

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(time2_ms, kSsrc1, RtpSourceType::SSRC)));
}

TEST_F(RtpReceiverTest, UpdatesTimestampsIfAndOnlyIfPacketArrivesInOrder) {
  RTPHeader header;
  int64_t time1_ms = fake_clock_.TimeInMilliseconds();
  header.payloadType = kPcmuPayloadType;
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(time1_ms);
  header.extension.hasAudioLevel = true;
  header.extension.audioLevel = 10;
  header.sequenceNumber = 0xfff0;

  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};
  uint32_t latest_timestamp;
  int64_t latest_receive_time_ms;

  // No packet received yet.
  EXPECT_FALSE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                  &latest_receive_time_ms));
  // Initial packet
  const uint32_t timestamp_1 = header.timestamp;
  const int64_t receive_time_1 = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_1);
  EXPECT_EQ(latest_receive_time_ms, receive_time_1);

  // Late packet, timestamp not recorded.
  fake_clock_.AdvanceTimeMilliseconds(10);
  header.timestamp -= 900;
  header.sequenceNumber -= 2;

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_1);
  EXPECT_EQ(latest_receive_time_ms, receive_time_1);

  // New packet, still late, no wraparound.
  fake_clock_.AdvanceTimeMilliseconds(10);
  header.timestamp += 1800;
  header.sequenceNumber += 1;

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_1);
  EXPECT_EQ(latest_receive_time_ms, receive_time_1);

  // New packet, new timestamp recorded
  fake_clock_.AdvanceTimeMilliseconds(10);
  header.timestamp += 900;
  header.sequenceNumber += 2;
  const uint32_t timestamp_2 = header.timestamp;
  const int64_t receive_time_2 = fake_clock_.TimeInMilliseconds();
  const uint16_t seqno_2 = header.sequenceNumber;

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_2);
  EXPECT_EQ(latest_receive_time_ms, receive_time_2);

  // New packet, timestamp wraps around
  fake_clock_.AdvanceTimeMilliseconds(10);
  header.timestamp += 900;
  header.sequenceNumber += 20;
  const uint32_t timestamp_3 = header.timestamp;
  const int64_t receive_time_3 = fake_clock_.TimeInMilliseconds();
  EXPECT_LT(header.sequenceNumber, seqno_2);  // Wrap-around

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_3);
  EXPECT_EQ(latest_receive_time_ms, receive_time_3);
}

TEST_F(RtpReceiverTest, UpdatesTimestampsWhenStreamResets) {
  RTPHeader header;
  int64_t time1_ms = fake_clock_.TimeInMilliseconds();
  header.payloadType = kPcmuPayloadType;
  header.ssrc = kSsrc1;
  header.timestamp = rtp_timestamp(time1_ms);
  header.extension.hasAudioLevel = true;
  header.extension.audioLevel = 10;
  header.sequenceNumber = 0xfff0;

  const PayloadUnion payload_specific{
      AudioPayload{SdpAudioFormat("foo", 8000, 1), 0}};
  uint32_t latest_timestamp;
  int64_t latest_receive_time_ms;

  // No packet received yet.
  EXPECT_FALSE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                  &latest_receive_time_ms));
  // Initial packet
  const uint32_t timestamp_1 = header.timestamp;
  const int64_t receive_time_1 = fake_clock_.TimeInMilliseconds();
  const uint16_t seqno_1 = header.sequenceNumber;
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_1);
  EXPECT_EQ(latest_receive_time_ms, receive_time_1);

  // Packet with far in the past seqno, but unlikely to be a wrap-around.
  // Treated as a seqno discontinuity, and timestamp is recorded.
  fake_clock_.AdvanceTimeMilliseconds(10);
  header.timestamp += 900;
  header.sequenceNumber = 0x9000;

  const uint32_t timestamp_2 = header.timestamp;
  const int64_t receive_time_2 = fake_clock_.TimeInMilliseconds();
  const uint16_t seqno_2 = header.sequenceNumber;
  EXPECT_LT(seqno_1 - seqno_2, 0x8000);  // In the past.

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific));
  EXPECT_TRUE(rtp_receiver_->GetLatestTimestamps(&latest_timestamp,
                                                 &latest_receive_time_ms));
  EXPECT_EQ(latest_timestamp, timestamp_2);
  EXPECT_EQ(latest_receive_time_ms, receive_time_2);
}

}  // namespace webrtc
