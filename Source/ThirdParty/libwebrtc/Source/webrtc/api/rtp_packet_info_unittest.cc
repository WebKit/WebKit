/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_packet_info.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/rtp_headers.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Optional;

template <typename ExtensionType,
          typename ExtensionValue = ExtensionType::value_type>
RtpPacketReceived CreateRtpPacketReceivedWithExtension(ExtensionValue value) {
  RtpHeaderExtensionMap extensions;
  extensions.Register<ExtensionType>(5);
  RtpPacketReceived packet(&extensions);
  RTC_CHECK(packet.SetExtension<ExtensionType>(value))
      << "Unable to set extension.";
  return packet;
}

TEST(RtpPacketInfoTest, Ssrc) {
  constexpr uint32_t kValue = 4038189233;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_ssrc(kValue);
  EXPECT_EQ(rhs.ssrc(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.ssrc(), kValue);

  rhs.set_ssrc(kValue);
  EXPECT_EQ(rhs.ssrc(), kValue);
}

TEST(RtpPacketInfoTest, SequenceNumber) {
  constexpr uint16_t kValue = 34653;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_sequence_number(kValue);
  EXPECT_EQ(rhs.sequence_number(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.sequence_number(), kValue);

  rhs.set_sequence_number(kValue);
  EXPECT_EQ(rhs.sequence_number(), kValue);
}

TEST(RtpPacketInfoTest, Csrcs) {
  const std::vector<uint32_t> kValue = {4038189233, 3016333617, 1207992985};

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_csrcs(kValue);
  EXPECT_EQ(rhs.csrcs(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.csrcs(), kValue);

  rhs.set_csrcs(kValue);
  EXPECT_EQ(rhs.csrcs(), kValue);
}

TEST(RtpPacketInfoTest, RtpTimestamp) {
  constexpr uint32_t kValue = 4038189233;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_rtp_timestamp(kValue);
  EXPECT_EQ(rhs.rtp_timestamp(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.rtp_timestamp(), kValue);

  rhs.set_rtp_timestamp(kValue);
  EXPECT_EQ(rhs.rtp_timestamp(), kValue);
}

TEST(RtpPacketInfoTest, ReceiveTimeMs) {
  constexpr Timestamp kValue = Timestamp::Micros(8868963877546349045LL);

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_receive_time(kValue);
  EXPECT_EQ(rhs.receive_time(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.receive_time(), kValue);

  rhs.set_receive_time(kValue);
  EXPECT_EQ(rhs.receive_time(), kValue);
}

TEST(RtpPacketInfoTest, AudioLevel) {
  constexpr std::optional<uint8_t> kValue = 31;

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_audio_level(kValue);
  EXPECT_EQ(rhs.audio_level(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.audio_level(), kValue);

  rhs.set_audio_level(kValue);
  EXPECT_EQ(rhs.audio_level(), kValue);
}

TEST(RtpPacketInfoTest, AbsoluteCaptureTime) {
  constexpr std::optional<AbsoluteCaptureTime> kValue = AbsoluteCaptureTime{
      .absolute_capture_timestamp = 12, .estimated_capture_clock_offset = 34};

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_absolute_capture_time(kValue);
  EXPECT_EQ(rhs.absolute_capture_time(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_NE(rhs.absolute_capture_time(), kValue);

  rhs.set_absolute_capture_time(kValue);
  EXPECT_EQ(rhs.absolute_capture_time(), kValue);
}

TEST(RtpPacketInfoTest, LocalCaptureClockOffset) {
  constexpr TimeDelta kValue = TimeDelta::Micros(8868963877546349045LL);

  RtpPacketInfo lhs;
  RtpPacketInfo rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.set_local_capture_clock_offset(kValue);
  EXPECT_EQ(rhs.local_capture_clock_offset(), kValue);

  EXPECT_FALSE(lhs == rhs);
  EXPECT_TRUE(lhs != rhs);

  lhs = rhs;

  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs = RtpPacketInfo();
  EXPECT_EQ(rhs.local_capture_clock_offset(), std::nullopt);

  rhs.set_local_capture_clock_offset(kValue);
  EXPECT_EQ(rhs.local_capture_clock_offset(), kValue);
}

TEST(RtpPacketInfoTest, RtpPacketReceivedNoExtensions) {
  constexpr Timestamp kArrivalTime = Timestamp::Micros(8868963877546349045LL);
  constexpr uint32_t kSsrc = 4038189233;
  constexpr uint16_t kSequenceNumber = 1234;
  constexpr uint32_t kRtpTimestamp = 5684353;
  const std::vector<uint32_t> kCsrcs = {15, 60};
  RtpPacketReceived packet;
  packet.set_arrival_time(kArrivalTime);
  packet.SetSsrc(kSsrc);
  packet.SetCsrcs(kCsrcs);
  packet.SetSequenceNumber(kSequenceNumber);
  packet.SetTimestamp(kRtpTimestamp);

  RtpPacketInfo rtp_packet_info(packet);

  EXPECT_EQ(rtp_packet_info.ssrc(), kSsrc);
  EXPECT_EQ(rtp_packet_info.sequence_number(), kSequenceNumber);
  EXPECT_THAT(rtp_packet_info.csrcs(), ElementsAreArray(kCsrcs));
  EXPECT_EQ(rtp_packet_info.rtp_timestamp(), kRtpTimestamp);
  EXPECT_EQ(rtp_packet_info.receive_time(), kArrivalTime);
}

TEST(RtpPacketInfoTest, RtpPacketReceivedAudioLevel) {
  constexpr uint8_t kValue = 14;
  RtpPacketReceived packet =
      CreateRtpPacketReceivedWithExtension<AudioLevelExtension>(
          AudioLevel(/*voice_activity=*/true, /*level=*/kValue));

  EXPECT_THAT(RtpPacketInfo(packet).audio_level(), Optional(kValue));
}

TEST(RtpPacketInfoTest, RtpPacketReceivedAbsoluteCaptureTime) {
  const AbsoluteCaptureTime kAbsoluteCaptureTime = {
      .absolute_capture_timestamp = 1493489345934859334,
      .estimated_capture_clock_offset = 3453534534};
  RtpPacketReceived packet =
      CreateRtpPacketReceivedWithExtension<AbsoluteCaptureTimeExtension>(
          kAbsoluteCaptureTime);

  EXPECT_THAT(RtpPacketInfo(packet).absolute_capture_time(),
              Optional(kAbsoluteCaptureTime));
}

}  // namespace
}  // namespace webrtc
