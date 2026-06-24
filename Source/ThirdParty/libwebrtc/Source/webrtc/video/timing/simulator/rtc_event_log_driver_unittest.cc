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
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/rtp_packet_simulator.h"
#include "video/timing/simulator/test/parsed_rtc_event_log_builder.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::Eq;
using ::testing::Field;

constexpr absl::string_view kEmptyFieldTrialsString = "";

constexpr uint32_t kSsrc1 = 123456;
constexpr uint32_t kRtxSsrc1 = 234567;
constexpr uint32_t kSsrc2 = 345678;
constexpr uint32_t kRtxSsrc2 = 456789;

constexpr uint16_t kRtxOsn = 823;

class MockRtcEventLogDriverStream : public RtcEventLogDriver::StreamInterface {
 public:
  MOCK_METHOD(void,
              InsertSimulatedPacket,
              (const RtpPacketSimulator::SimulatedPacket& simulated_packet),
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
    return [this](Environment env, uint32_t ssrc, uint32_t rtx_ssrc) {
      return stream_factory_.Create(env, ssrc);
    };
  }

  MockRtcEventLogDriverStreamFactory stream_factory_;
  ParsedRtcEventLogBuilder parsed_log_builder_;
};

TEST_F(RtcEventLogDriverTest, EmptyLogDoesNotCreateStreams) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 0);
}

TEST_F(RtcEventLogDriverTest, LoggedVideoRecvConfigCreatesStream) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  EXPECT_CALL(*stream_factory_.stream1_ptr_, Close());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 1);
}

TEST_F(RtcEventLogDriverTest,
       LoggedVideoRecvConfigCreatesStreamIfIncludedInSsrcFilter) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config{.ssrc_filter = {kSsrc1}},
                           parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  EXPECT_CALL(*stream_factory_.stream1_ptr_, Close());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 1);
}

TEST_F(RtcEventLogDriverTest,
       LoggedVideoRecvConfigDoesNotCreateStreamIfNotIncludedInSsrcFilter) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config{.ssrc_filter = {kSsrc2}},
                           parsed_log.get(), kEmptyFieldTrialsString,
                           BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 0);
}

TEST_F(RtcEventLogDriverTest, LoggedVideoRecvConfigsCreateStreams) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.LogVideoRecvConfig(kSsrc2, kRtxSsrc2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  EXPECT_CALL(*stream_factory_.stream1_ptr_, Close());
  EXPECT_CALL(*stream_factory_.stream2_ptr_, Close());
  driver.Simulate();

  EXPECT_EQ(stream_factory_.NumStreamsCreated(), 2);
}

class CountingRtcEventLogDriverStreamFactoryFactory {
 public:
  auto BuildStreamFactory() {
    return [this](Environment env, uint32_t ssrc, uint32_t rtx_ssrc) {
      ++num_streams_created_;
      return std::make_unique<MockRtcEventLogDriverStream>();
    };
  }

  int NumStreamsCreated() const { return num_streams_created_; }

 private:
  int num_streams_created_ = 0;
};

TEST_F(RtcEventLogDriverTest, ReusesStreamIfConfigured) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  CountingRtcEventLogDriverStreamFactoryFactory counter;
  RtcEventLogDriver driver(RtcEventLogDriver::Config{.reuse_streams = true},
                           parsed_log.get(), kEmptyFieldTrialsString,
                           counter.BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(counter.NumStreamsCreated(), 1);
}

TEST_F(RtcEventLogDriverTest, DoesNotReuseStreamIfConfigured) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  CountingRtcEventLogDriverStreamFactoryFactory counter;
  RtcEventLogDriver driver(RtcEventLogDriver::Config{.reuse_streams = false},
                           parsed_log.get(), kEmptyFieldTrialsString,
                           counter.BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(counter.NumStreamsCreated(), 2);
}

TEST_F(RtcEventLogDriverTest, FirstLoggedEventSetsSimulationClock) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(driver.GetCurrentTimeForTesting(),
            parsed_log_builder_.CurrentTime() +
                RtcEventLogDriver::kShutdownAdvanceTimeSlack);
}

TEST_F(RtcEventLogDriverTest, LoggedEventAdvancesSimulationClock) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.AdvanceTime(TimeDelta::Millis(50));
  parsed_log_builder_.LogVideoRecvConfig(kSsrc2, kRtxSsrc2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  driver.Simulate();

  EXPECT_EQ(driver.GetCurrentTimeForTesting(),
            parsed_log_builder_.CurrentTime() +
                RtcEventLogDriver::kShutdownAdvanceTimeSlack);
}

TEST_F(RtcEventLogDriverTest, LoggedRtpPacketIncomingInsertsPacketIntoStream) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.LogRtpPacketIncoming(kSsrc1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  EXPECT_CALL(*stream_factory_.stream1_ptr_, InsertSimulatedPacket);
  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  driver.Simulate();
}

TEST_F(RtcEventLogDriverTest,
       LoggedRtpPacketIncomingInsertsRtxPacketIntoStream) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.LogRtpPacketIncoming(kRtxSsrc1, kRtxOsn);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  EXPECT_CALL(
      *stream_factory_.stream1_ptr_,
      InsertSimulatedPacket(
          Field(&RtpPacketSimulator::SimulatedPacket::has_rtx_osn, Eq(true))));
  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  driver.Simulate();
}

TEST_F(RtcEventLogDriverTest,
       LoggedRtpPacketIncomingsInsertsPacketsIntoStreams) {
  parsed_log_builder_.LogVideoRecvConfig(kSsrc1, kRtxSsrc1);
  parsed_log_builder_.LogVideoRecvConfig(kSsrc2, kRtxSsrc2);
  parsed_log_builder_.LogRtpPacketIncoming(kSsrc1);
  parsed_log_builder_.LogRtpPacketIncoming(kSsrc2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = parsed_log_builder_.Build();

  EXPECT_CALL(*stream_factory_.stream1_ptr_, InsertSimulatedPacket);
  EXPECT_CALL(*stream_factory_.stream2_ptr_, InsertSimulatedPacket);
  RtcEventLogDriver driver(RtcEventLogDriver::Config(), parsed_log.get(),
                           kEmptyFieldTrialsString, BuildStreamFactory());
  driver.Simulate();
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
