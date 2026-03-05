/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtc_event_log_driver.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/test/parsed_rtc_event_log_builder.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::_;

constexpr absl::string_view kEmptyFieldTrialsString = "";
constexpr uint32_t kSsrc1 = 123456;
constexpr uint32_t kSsrc2 = 456789;

class MockRtcEventLogDriverStream : public RtcEventLogDriver::StreamInterface {
 public:
  MOCK_METHOD(void,
              InsertPacket,
              (const RtpPacketReceived& rtp_packet),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

class MockRtcEventLogDriverStreamFactory {
 public:
  MockRtcEventLogDriverStreamFactory()
      : stream1_(std::make_unique<MockRtcEventLogDriverStream>()),
        stream2_(std::make_unique<MockRtcEventLogDriverStream>()),
        stream1_ptr_(stream1_.get()),
        stream2_ptr_(stream2_.get()) {}
  ~MockRtcEventLogDriverStreamFactory() = default;

  std::unique_ptr<MockRtcEventLogDriverStream> Create(Environment,
                                                      uint32_t ssrc) {
    if (ssrc == kSsrc1) {
      RTC_CHECK(stream1_) << "Stream 1 was already moved";
      return std::move(stream1_);
    }
    if (ssrc == kSsrc2) {
      RTC_CHECK(stream2_) << "Stream 2 was already moved";
      return std::move(stream2_);
    }
    RTC_CHECK_NOTREACHED();
    return std::make_unique<MockRtcEventLogDriverStream>();
  }

  int NumStreamsCreated() const {
    return static_cast<int>(stream1_ == nullptr) +
           static_cast<int>(stream2_ == nullptr);
  }

  // Unique pointers for ownership.
  std::unique_ptr<MockRtcEventLogDriverStream> stream1_;
  std::unique_ptr<MockRtcEventLogDriverStream> stream2_;

  // Raw pointers for expectations.
  MockRtcEventLogDriverStream* stream1_ptr_;
  MockRtcEventLogDriverStream* stream2_ptr_;
};

class RtcEventLogDriverTest : public ::testing::Test {
 protected:
  auto BuildStreamFactory() {
    return [this](Environment env, uint32_t ssrc) {
      return stream_factory_.Create(env, ssrc);
    };
  }

  MockRtcEventLogDriverStreamFactory stream_factory_;
  ParsedRtcEventLogBuilder parsed_log_builder_;
};

TEST_F(RtcEventLogDriverTest, EmptyLogDoesNotCreateStreams) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 0);
}

TEST_F(RtcEventLogDriverTest, LoggedVideoRecvConfigCreatesStream) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  EXPECT_CALL(*stream_factory_.stream1_ptr_, Close());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 1);
}

TEST_F(RtcEventLogDriverTest, LoggedVideoRecvConfigsCreateStreams) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1);
  parsed_log_builder_.LogVideoRecvConfig(kSsrc2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  EXPECT_CALL(*stream_factory_.stream1_ptr_, Close());
  EXPECT_CALL(*stream_factory_.stream2_ptr_, Close());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 2);
}

TEST_F(RtcEventLogDriverTest, FirstLoggedEventSetsSimulationClock) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(driver.GetCurrentTimeForTesting(),
            parsed_log_builder_.CurrentTime() +
                RtcEventLogDriver::kShutdownAdvanceTimeSlack);
}

TEST_F(RtcEventLogDriverTest, LoggedEventAdvancesSimulationClock) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1);
  parsed_log_builder_.AdvanceTime(TimeDelta::Millis(50));
  parsed_log_builder_.LogVideoRecvConfig(kSsrc2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(driver.GetCurrentTimeForTesting(),
            parsed_log_builder_.CurrentTime() +
                RtcEventLogDriver::kShutdownAdvanceTimeSlack);
}

TEST_F(RtcEventLogDriverTest, LoggedRtpPacketIncomingInsertsPacketIntoStream) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1);
  parsed_log_builder_.LogRtpPacketIncoming(kSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  EXPECT_CALL(*stream_factory_.stream1_ptr_, InsertPacket(_));
  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  driver.Simulate();
}

TEST_F(RtcEventLogDriverTest,
       LoggedRtpPacketIncomingsInsertsPacketsIntoStreams) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1);
  parsed_log_builder_.LogVideoRecvConfig(kSsrc2);
  parsed_log_builder_.LogRtpPacketIncoming(kSsrc1);
  parsed_log_builder_.LogRtpPacketIncoming(kSsrc2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  EXPECT_CALL(*stream_factory_.stream1_ptr_, InsertPacket(_));
  EXPECT_CALL(*stream_factory_.stream2_ptr_, InsertPacket(_));
  RtcEventLogDriver driver(parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  driver.Simulate();
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
