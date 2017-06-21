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

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_impl.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

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
const bool kInOrder = true;

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
    CodecInst voice_codec = {};
    voice_codec.pltype = kPcmuPayloadType;
    voice_codec.plfreq = 8000;
    voice_codec.rate = kTestRate;
    memcpy(voice_codec.plname, "PCMU", 5);
    rtp_receiver_->RegisterReceivePayload(voice_codec);
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
  PayloadUnion payload_specific = {AudioPayload()};

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
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
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
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
  PayloadUnion payload_specific = {AudioPayload()};

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
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
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
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
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(prev_time_ms, kSsrc2, RtpSourceType::SSRC),
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC)));

  // Old SSRC source timeout.
  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  now_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
  sources = rtp_receiver_->GetSources();
  EXPECT_THAT(sources, UnorderedElementsAre(
                           RtpSource(now_ms, kSsrc1, RtpSourceType::SSRC)));
}

TEST_F(RtpReceiverTest, GetSourcesRemoveOutdatedSource) {
  int64_t now_ms = fake_clock_.TimeInMilliseconds();

  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.timestamp = rtp_timestamp(now_ms);
  PayloadUnion payload_specific = {AudioPayload()};
  header.numCSRCs = 1;
  size_t kSourceListSize = 20;

  for (size_t i = 0; i < kSourceListSize; ++i) {
    header.ssrc = i;
    header.arrOfCSRCs[0] = (i + 1);
    EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload,
                                                 sizeof(kTestPayload),
                                                 payload_specific, !kInOrder));
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
      header, kTestPayload, sizeof(kTestPayload), payload_specific, !kInOrder));
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

}  // namespace webrtc
