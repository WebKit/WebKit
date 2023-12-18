/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/rtp_headers.h"
#include "api/test/create_time_controller.h"
#include "api/test/time_controller.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_bitrate_allocation.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/mocks/mock_network_link_rtcp_observer.h"
#include "modules/rtp_rtcp/mocks/mock_rtcp_rtt_stats.h"
#include "modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;
using ::webrtc::rtcp::Bye;
using ::webrtc::rtcp::CompoundPacket;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::rtcp::SenderReport;
using ::webrtc::test::RtcpPacketParser;

class MockReceiveStatisticsProvider : public ReceiveStatisticsProvider {
 public:
  MOCK_METHOD(std::vector<ReportBlock>, RtcpReportBlocks, (size_t), (override));
};

class MockMediaReceiverRtcpObserver : public MediaReceiverRtcpObserver {
 public:
  MOCK_METHOD(void, OnSenderReport, (uint32_t, NtpTime, uint32_t), (override));
  MOCK_METHOD(void, OnBye, (uint32_t), (override));
  MOCK_METHOD(void,
              OnBitrateAllocation,
              (uint32_t, const VideoBitrateAllocation&),
              (override));
};

class MockRtpStreamRtcpHandler : public RtpStreamRtcpHandler {
 public:
  MockRtpStreamRtcpHandler() {
    // With each next call increase number of sent packets and bytes to simulate
    // active RTP sender.
    ON_CALL(*this, SentStats).WillByDefault([this] {
      RtpStats stats;
      stats.set_num_sent_packets(++num_calls_);
      stats.set_num_sent_bytes(1'000 * num_calls_);
      return stats;
    });
  }

  MOCK_METHOD(RtpStats, SentStats, (), (override));
  MOCK_METHOD(void,
              OnNack,
              (uint32_t, rtc::ArrayView<const uint16_t>),
              (override));
  MOCK_METHOD(void, OnFir, (uint32_t), (override));
  MOCK_METHOD(void, OnPli, (uint32_t), (override));
  MOCK_METHOD(void, OnReport, (const ReportBlockData&), (override));

 private:
  int num_calls_ = 0;
};

constexpr TimeDelta kReportPeriod = TimeDelta::Seconds(1);
constexpr TimeDelta kAlmostForever = TimeDelta::Seconds(2);
constexpr TimeDelta kTimePrecision = TimeDelta::Millis(1);

MATCHER_P(Near, value, "") {
  return arg > value - kTimePrecision && arg < value + kTimePrecision;
}

// Helper to wait for an rtcp packet produced on a different thread/task queue.
class FakeRtcpTransport {
 public:
  explicit FakeRtcpTransport(TimeController& time) : time_(time) {}

  std::function<void(rtc::ArrayView<const uint8_t>)> AsStdFunction() {
    return [this](rtc::ArrayView<const uint8_t>) { sent_rtcp_ = true; };
  }

  // Returns true when packet was received by the transport.
  bool WaitPacket() {
    bool got_packet = time_.Wait([this] { return sent_rtcp_; }, kAlmostForever);
    // Clear the 'event' to allow waiting for multiple packets.
    sent_rtcp_ = false;
    return got_packet;
  }

 private:
  TimeController& time_;
  bool sent_rtcp_ = false;
};

std::function<void(rtc::ArrayView<const uint8_t>)> RtcpParserTransport(
    RtcpPacketParser& parser) {
  return [&parser](rtc::ArrayView<const uint8_t> packet) {
    return parser.Parse(packet);
  };
}

class RtcpTransceiverImplTest : public ::testing::Test {
 public:
  RtcpTransceiverConfig DefaultTestConfig() {
    // RtcpTransceiverConfig default constructor sets default values for prod.
    // Test doesn't need to support all key features: Default test config
    // returns valid config with all features turned off.
    RtcpTransceiverConfig config;
    config.clock = time_->GetClock();
    config.schedule_periodic_compound_packets = false;
    config.initial_report_delay = kReportPeriod / 2;
    config.report_period = kReportPeriod;
    return config;
  }

  TimeController& time_controller() { return *time_; }
  Timestamp CurrentTime() { return time_->GetClock()->CurrentTime(); }
  void AdvanceTime(TimeDelta time) { time_->AdvanceTime(time); }
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue() {
    return time_->GetTaskQueueFactory()->CreateTaskQueue(
        "rtcp", TaskQueueFactory::Priority::NORMAL);
  }

 private:
  std::unique_ptr<TimeController> time_ = CreateSimulatedTimeController();
};

TEST_F(RtcpTransceiverImplTest, NeedToStopPeriodicTaskToDestroyOnTaskQueue) {
  FakeRtcpTransport transport(time_controller());
  auto queue = CreateTaskQueue();
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.task_queue = queue.get();
  config.schedule_periodic_compound_packets = true;
  config.rtcp_transport = transport.AsStdFunction();
  auto* rtcp_transceiver = new RtcpTransceiverImpl(config);
  // Wait for a periodic packet.
  EXPECT_TRUE(transport.WaitPacket());

  bool done = false;
  queue->PostTask([rtcp_transceiver, &done] {
    rtcp_transceiver->StopPeriodicTask();
    delete rtcp_transceiver;
    done = true;
  });
  ASSERT_TRUE(time_controller().Wait([&] { return done; }, kAlmostForever));
}

TEST_F(RtcpTransceiverImplTest, CanBeDestroyedRightAfterCreation) {
  FakeRtcpTransport transport(time_controller());
  auto queue = CreateTaskQueue();
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.task_queue = queue.get();
  config.schedule_periodic_compound_packets = true;
  config.rtcp_transport = transport.AsStdFunction();

  bool done = false;
  queue->PostTask([&] {
    RtcpTransceiverImpl rtcp_transceiver(config);
    rtcp_transceiver.StopPeriodicTask();
    done = true;
  });
  ASSERT_TRUE(time_controller().Wait([&] { return done; }, kAlmostForever));
}

TEST_F(RtcpTransceiverImplTest, CanDestroyAfterTaskQueue) {
  FakeRtcpTransport transport(time_controller());
  auto queue = CreateTaskQueue();

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.task_queue = queue.get();
  config.schedule_periodic_compound_packets = true;
  config.rtcp_transport = transport.AsStdFunction();
  auto* rtcp_transceiver = new RtcpTransceiverImpl(config);
  // Wait for a periodic packet.
  EXPECT_TRUE(transport.WaitPacket());

  queue = nullptr;
  delete rtcp_transceiver;
}

TEST_F(RtcpTransceiverImplTest, DelaysSendingFirstCompondPacket) {
  auto queue = CreateTaskQueue();
  FakeRtcpTransport transport(time_controller());
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = true;
  config.rtcp_transport = transport.AsStdFunction();
  config.initial_report_delay = TimeDelta::Millis(10);
  config.task_queue = queue.get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;

  Timestamp started = CurrentTime();
  queue->PostTask([&] { rtcp_transceiver.emplace(config); });
  EXPECT_TRUE(transport.WaitPacket());

  EXPECT_GE(CurrentTime() - started, config.initial_report_delay);

  // Cleanup.
  bool done = false;
  queue->PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done = true;
  });
  ASSERT_TRUE(time_controller().Wait([&] { return done; }, kAlmostForever));
}

TEST_F(RtcpTransceiverImplTest, PeriodicallySendsPackets) {
  auto queue = CreateTaskQueue();
  FakeRtcpTransport transport(time_controller());
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = true;
  config.rtcp_transport = transport.AsStdFunction();
  config.initial_report_delay = TimeDelta::Zero();
  config.report_period = kReportPeriod;
  config.task_queue = queue.get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;
  Timestamp time_just_before_1st_packet = Timestamp::MinusInfinity();
  queue->PostTask([&] {
    // Because initial_report_delay_ms is set to 0, time_just_before_the_packet
    // should be very close to the time_of_the_packet.
    time_just_before_1st_packet = CurrentTime();
    rtcp_transceiver.emplace(config);
  });

  EXPECT_TRUE(transport.WaitPacket());
  EXPECT_TRUE(transport.WaitPacket());
  Timestamp time_just_after_2nd_packet = CurrentTime();

  EXPECT_GE(time_just_after_2nd_packet - time_just_before_1st_packet,
            config.report_period);

  // Cleanup.
  bool done = false;
  queue->PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done = true;
  });
  ASSERT_TRUE(time_controller().Wait([&] { return done; }, kAlmostForever));
}

TEST_F(RtcpTransceiverImplTest, SendCompoundPacketDelaysPeriodicSendPackets) {
  auto queue = CreateTaskQueue();
  FakeRtcpTransport transport(time_controller());
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = true;
  config.rtcp_transport = transport.AsStdFunction();
  config.initial_report_delay = TimeDelta::Zero();
  config.report_period = kReportPeriod;
  config.task_queue = queue.get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;
  queue->PostTask([&] { rtcp_transceiver.emplace(config); });

  // Wait for the first packet.
  EXPECT_TRUE(transport.WaitPacket());
  // Send non periodic one after half period.
  bool non_periodic = false;
  Timestamp time_of_non_periodic_packet = Timestamp::MinusInfinity();
  queue->PostDelayedTask(
      [&] {
        time_of_non_periodic_packet = CurrentTime();
        rtcp_transceiver->SendCompoundPacket();
        non_periodic = true;
      },
      config.report_period / 2);
  // Though non-periodic packet is scheduled just in between periodic, due to
  // small period and task queue flakiness it migth end-up 1ms after next
  // periodic packet. To be sure duration after non-periodic packet is tested
  // wait for transport after ensuring non-periodic packet was sent.
  EXPECT_TRUE(
      time_controller().Wait([&] { return non_periodic; }, kAlmostForever));
  EXPECT_TRUE(transport.WaitPacket());
  // Wait for next periodic packet.
  EXPECT_TRUE(transport.WaitPacket());
  Timestamp time_of_last_periodic_packet = CurrentTime();
  EXPECT_GE(time_of_last_periodic_packet - time_of_non_periodic_packet,
            config.report_period);

  // Cleanup.
  bool done = false;
  queue->PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done = true;
  });
  ASSERT_TRUE(time_controller().Wait([&] { return done; }, kAlmostForever));
}

TEST_F(RtcpTransceiverImplTest, SendsNoRtcpWhenNetworkStateIsDown) {
  MockFunction<void(rtc::ArrayView<const uint8_t>)> mock_transport;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.initial_ready_to_send = false;
  config.rtcp_transport = mock_transport.AsStdFunction();
  RtcpTransceiverImpl rtcp_transceiver(config);

  EXPECT_CALL(mock_transport, Call).Times(0);

  const std::vector<uint16_t> sequence_numbers = {45, 57};
  const uint32_t ssrcs[] = {123};
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendNack(ssrcs[0], sequence_numbers);
  rtcp_transceiver.SendPictureLossIndication(ssrcs[0]);
  rtcp_transceiver.SendFullIntraRequest(ssrcs, true);
}

TEST_F(RtcpTransceiverImplTest, SendsRtcpWhenNetworkStateIsUp) {
  MockFunction<void(rtc::ArrayView<const uint8_t>)> mock_transport;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.initial_ready_to_send = false;
  config.rtcp_transport = mock_transport.AsStdFunction();
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetReadyToSend(true);

  EXPECT_CALL(mock_transport, Call).Times(4);

  const std::vector<uint16_t> sequence_numbers = {45, 57};
  const uint32_t ssrcs[] = {123};
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendNack(ssrcs[0], sequence_numbers);
  rtcp_transceiver.SendPictureLossIndication(ssrcs[0]);
  rtcp_transceiver.SendFullIntraRequest(ssrcs, true);
}

TEST_F(RtcpTransceiverImplTest, SendsPeriodicRtcpWhenNetworkStateIsUp) {
  auto queue = CreateTaskQueue();
  FakeRtcpTransport transport(time_controller());
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = true;
  config.initial_ready_to_send = false;
  config.rtcp_transport = transport.AsStdFunction();
  config.task_queue = queue.get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;
  rtcp_transceiver.emplace(config);

  queue->PostTask([&] { rtcp_transceiver->SetReadyToSend(true); });

  EXPECT_TRUE(transport.WaitPacket());

  // Cleanup.
  bool done = false;
  queue->PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done = true;
  });
  ASSERT_TRUE(time_controller().Wait([&] { return done; }, kAlmostForever));
}

TEST_F(RtcpTransceiverImplTest, SendsMinimalCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.cname = "cname";
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  // Minimal compound RTCP packet contains sender or receiver report and sdes
  // with cname.
  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_GT(rtcp_parser.sdes()->num_packets(), 0);
  ASSERT_EQ(rtcp_parser.sdes()->chunks().size(), 1u);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].ssrc, kSenderSsrc);
  EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].cname, config.cname);
}

TEST_F(RtcpTransceiverImplTest, AvoidsEmptyPacketsInReducedMode) {
  MockFunction<void(rtc::ArrayView<const uint8_t>)> transport;
  EXPECT_CALL(transport, Call).Times(0);
  NiceMock<MockReceiveStatisticsProvider> receive_statistics;

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.rtcp_transport = transport.AsStdFunction();
  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();
}

TEST_F(RtcpTransceiverImplTest, AvoidsEmptyReceiverReportsInReducedMode) {
  RtcpPacketParser rtcp_parser;
  NiceMock<MockReceiveStatisticsProvider> receive_statistics;

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;
  config.receive_statistics = &receive_statistics;
  // Set it to produce something (RRTR) in the "periodic" rtcp packets.
  config.non_sender_rtt_measurement = true;
  RtcpTransceiverImpl rtcp_transceiver(config);

  // Rather than waiting for the right time to produce the periodic packet,
  // trigger it manually.
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_GT(rtcp_parser.xr()->num_packets(), 0);
}

TEST_F(RtcpTransceiverImplTest, SendsNoRembInitially) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 0);
}

TEST_F(RtcpTransceiverImplTest, SetRembIncludesRembInNextCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(54321, 64321));
}

TEST_F(RtcpTransceiverImplTest, SetRembUpdatesValuesToSend) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(54321, 64321));

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/70000, /*ssrcs=*/{67321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 70000);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(67321));
}

TEST_F(RtcpTransceiverImplTest, SetRembSendsImmediatelyIfSendRembOnChange) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.send_remb_on_change = true;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000);

  // If there is no change, the packet is not sent immediately.
  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/20000, /*ssrcs=*/{});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 20000);
}

TEST_F(RtcpTransceiverImplTest,
       SetRembSendsImmediatelyIfSendRembOnChangeReducedSize) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.send_remb_on_change = true;
  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000);
}

TEST_F(RtcpTransceiverImplTest, SetRembIncludesRembInAllCompoundPackets) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{2});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 2);
}

TEST_F(RtcpTransceiverImplTest, SendsNoRembAfterUnset) {
  const uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  ASSERT_EQ(rtcp_parser.remb()->num_packets(), 1);

  rtcp_transceiver.UnsetRemb();
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{2});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
}

TEST_F(RtcpTransceiverImplTest, ReceiverReportUsesReceiveStatistics) {
  const uint32_t kSenderSsrc = 12345;
  const uint32_t kMediaSsrc = 54321;
  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.receive_statistics = &receive_statistics;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  ASSERT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
  ASSERT_THAT(rtcp_parser.receiver_report()->report_blocks(),
              SizeIs(report_blocks.size()));
  EXPECT_EQ(rtcp_parser.receiver_report()->report_blocks()[0].source_ssrc(),
            kMediaSsrc);
}

TEST_F(RtcpTransceiverImplTest, MultipleObserversOnSameSsrc) {
  const uint32_t kRemoteSsrc = 12345;
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer1);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer2);

  const NtpTime kRemoteNtp(0x9876543211);
  const uint32_t kRemoteRtp = 0x444555;
  SenderReport sr;
  sr.SetSenderSsrc(kRemoteSsrc);
  sr.SetNtp(kRemoteNtp);
  sr.SetRtpTimestamp(kRemoteRtp);
  auto raw_packet = sr.Build();

  EXPECT_CALL(observer1, OnSenderReport(kRemoteSsrc, kRemoteNtp, kRemoteRtp));
  EXPECT_CALL(observer2, OnSenderReport(kRemoteSsrc, kRemoteNtp, kRemoteRtp));
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, DoesntCallsObserverAfterRemoved) {
  const uint32_t kRemoteSsrc = 12345;
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer1);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer2);

  SenderReport sr;
  sr.SetSenderSsrc(kRemoteSsrc);
  auto raw_packet = sr.Build();

  rtcp_transceiver.RemoveMediaReceiverRtcpObserver(kRemoteSsrc, &observer1);

  EXPECT_CALL(observer1, OnSenderReport(_, _, _)).Times(0);
  EXPECT_CALL(observer2, OnSenderReport(_, _, _));
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, CallsObserverOnSenderReportBySenderSsrc) {
  const uint32_t kRemoteSsrc1 = 12345;
  const uint32_t kRemoteSsrc2 = 22345;
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc1, &observer1);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc2, &observer2);

  const NtpTime kRemoteNtp(0x9876543211);
  const uint32_t kRemoteRtp = 0x444555;
  SenderReport sr;
  sr.SetSenderSsrc(kRemoteSsrc1);
  sr.SetNtp(kRemoteNtp);
  sr.SetRtpTimestamp(kRemoteRtp);
  auto raw_packet = sr.Build();

  EXPECT_CALL(observer1, OnSenderReport(kRemoteSsrc1, kRemoteNtp, kRemoteRtp));
  EXPECT_CALL(observer2, OnSenderReport).Times(0);
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, CallsObserverOnByeBySenderSsrc) {
  const uint32_t kRemoteSsrc1 = 12345;
  const uint32_t kRemoteSsrc2 = 22345;
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc1, &observer1);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc2, &observer2);

  Bye bye;
  bye.SetSenderSsrc(kRemoteSsrc1);
  auto raw_packet = bye.Build();

  EXPECT_CALL(observer1, OnBye(kRemoteSsrc1));
  EXPECT_CALL(observer2, OnBye(_)).Times(0);
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, CallsObserverOnTargetBitrateBySenderSsrc) {
  const uint32_t kRemoteSsrc1 = 12345;
  const uint32_t kRemoteSsrc2 = 22345;
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc1, &observer1);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc2, &observer2);

  webrtc::rtcp::TargetBitrate target_bitrate;
  target_bitrate.AddTargetBitrate(0, 0, /*target_bitrate_kbps=*/10);
  target_bitrate.AddTargetBitrate(0, 1, /*target_bitrate_kbps=*/20);
  target_bitrate.AddTargetBitrate(1, 0, /*target_bitrate_kbps=*/40);
  target_bitrate.AddTargetBitrate(1, 1, /*target_bitrate_kbps=*/80);
  webrtc::rtcp::ExtendedReports xr;
  xr.SetSenderSsrc(kRemoteSsrc1);
  xr.SetTargetBitrate(target_bitrate);
  auto raw_packet = xr.Build();

  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, /*bitrate_bps=*/10000);
  bitrate_allocation.SetBitrate(0, 1, /*bitrate_bps=*/20000);
  bitrate_allocation.SetBitrate(1, 0, /*bitrate_bps=*/40000);
  bitrate_allocation.SetBitrate(1, 1, /*bitrate_bps=*/80000);
  EXPECT_CALL(observer1, OnBitrateAllocation(kRemoteSsrc1, bitrate_allocation));
  EXPECT_CALL(observer2, OnBitrateAllocation(_, _)).Times(0);
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, SkipsIncorrectTargetBitrateEntries) {
  const uint32_t kRemoteSsrc = 12345;
  MockMediaReceiverRtcpObserver observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer);

  webrtc::rtcp::TargetBitrate target_bitrate;
  target_bitrate.AddTargetBitrate(0, 0, /*target_bitrate_kbps=*/10);
  target_bitrate.AddTargetBitrate(0, webrtc::kMaxTemporalStreams, 20);
  target_bitrate.AddTargetBitrate(webrtc::kMaxSpatialLayers, 0, 40);

  webrtc::rtcp::ExtendedReports xr;
  xr.SetTargetBitrate(target_bitrate);
  xr.SetSenderSsrc(kRemoteSsrc);
  auto raw_packet = xr.Build();

  VideoBitrateAllocation expected_allocation;
  expected_allocation.SetBitrate(0, 0, /*bitrate_bps=*/10000);
  EXPECT_CALL(observer, OnBitrateAllocation(kRemoteSsrc, expected_allocation));
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, CallsObserverOnByeBehindSenderReport) {
  const uint32_t kRemoteSsrc = 12345;
  MockMediaReceiverRtcpObserver observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer);

  CompoundPacket compound;
  auto sr = std::make_unique<SenderReport>();
  sr->SetSenderSsrc(kRemoteSsrc);
  compound.Append(std::move(sr));
  auto bye = std::make_unique<Bye>();
  bye->SetSenderSsrc(kRemoteSsrc);
  compound.Append(std::move(bye));
  auto raw_packet = compound.Build();

  EXPECT_CALL(observer, OnBye(kRemoteSsrc));
  EXPECT_CALL(observer, OnSenderReport(kRemoteSsrc, _, _));
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest, CallsObserverOnByeBehindUnknownRtcpPacket) {
  const uint32_t kRemoteSsrc = 12345;
  MockMediaReceiverRtcpObserver observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, &observer);

  CompoundPacket compound;
  // Use Application-Defined rtcp packet as unknown.
  auto app = std::make_unique<webrtc::rtcp::App>();
  compound.Append(std::move(app));
  auto bye = std::make_unique<Bye>();
  bye->SetSenderSsrc(kRemoteSsrc);
  compound.Append(std::move(bye));
  auto raw_packet = compound.Build();

  EXPECT_CALL(observer, OnBye(kRemoteSsrc));
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST_F(RtcpTransceiverImplTest,
       WhenSendsReceiverReportSetsLastSenderReportTimestampPerRemoteSsrc) {
  const uint32_t kRemoteSsrc1 = 4321;
  const uint32_t kRemoteSsrc2 = 5321;
  std::vector<ReportBlock> statistics_report_blocks(2);
  statistics_report_blocks[0].SetMediaSsrc(kRemoteSsrc1);
  statistics_report_blocks[1].SetMediaSsrc(kRemoteSsrc2);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(statistics_report_blocks));

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  const NtpTime kRemoteNtp(0x9876543211);
  // Receive SenderReport for RemoteSsrc1, but no report for RemoteSsrc2.
  SenderReport sr;
  sr.SetSenderSsrc(kRemoteSsrc1);
  sr.SetNtp(kRemoteNtp);
  auto raw_packet = sr.Build();
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));

  // Trigger sending ReceiverReport.
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  const auto& report_blocks = rtcp_parser.receiver_report()->report_blocks();
  ASSERT_EQ(report_blocks.size(), 2u);
  // RtcpTransceiverImpl doesn't guarantee order of the report blocks
  // match result of ReceiveStatisticsProvider::RtcpReportBlocks callback,
  // but for simplicity of the test asume it is the same.
  ASSERT_EQ(report_blocks[0].source_ssrc(), kRemoteSsrc1);
  EXPECT_EQ(report_blocks[0].last_sr(), CompactNtp(kRemoteNtp));

  ASSERT_EQ(report_blocks[1].source_ssrc(), kRemoteSsrc2);
  // No matching Sender Report for kRemoteSsrc2, LastSR fields has to be 0.
  EXPECT_EQ(report_blocks[1].last_sr(), 0u);
}

TEST_F(RtcpTransceiverImplTest,
       WhenSendsReceiverReportCalculatesDelaySinceLastSenderReport) {
  const uint32_t kRemoteSsrc1 = 4321;
  const uint32_t kRemoteSsrc2 = 5321;

  std::vector<ReportBlock> statistics_report_blocks(2);
  statistics_report_blocks[0].SetMediaSsrc(kRemoteSsrc1);
  statistics_report_blocks[1].SetMediaSsrc(kRemoteSsrc2);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(statistics_report_blocks));

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  auto receive_sender_report = [&](uint32_t remote_ssrc) {
    SenderReport sr;
    sr.SetSenderSsrc(remote_ssrc);
    rtcp_transceiver.ReceivePacket(sr.Build(), CurrentTime());
  };

  receive_sender_report(kRemoteSsrc1);
  time_controller().AdvanceTime(TimeDelta::Millis(100));

  receive_sender_report(kRemoteSsrc2);
  time_controller().AdvanceTime(TimeDelta::Millis(100));

  // Trigger ReceiverReport back.
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  const auto& report_blocks = rtcp_parser.receiver_report()->report_blocks();
  ASSERT_EQ(report_blocks.size(), 2u);
  // RtcpTransceiverImpl doesn't guarantee order of the report blocks
  // match result of ReceiveStatisticsProvider::RtcpReportBlocks callback,
  // but for simplicity of the test asume it is the same.
  ASSERT_EQ(report_blocks[0].source_ssrc(), kRemoteSsrc1);
  EXPECT_THAT(CompactNtpRttToTimeDelta(report_blocks[0].delay_since_last_sr()),
              Near(TimeDelta::Millis(200)));

  ASSERT_EQ(report_blocks[1].source_ssrc(), kRemoteSsrc2);
  EXPECT_THAT(CompactNtpRttToTimeDelta(report_blocks[1].delay_since_last_sr()),
              Near(TimeDelta::Millis(100)));
}

TEST_F(RtcpTransceiverImplTest, MaySendMultipleReceiverReportInSinglePacket) {
  std::vector<ReportBlock> statistics_report_blocks(40);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(/*max_blocks=*/Ge(40u)))
      .WillOnce(Return(statistics_report_blocks));

  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  // Trigger ReceiverReports.
  rtcp_transceiver.SendCompoundPacket();

  // Expect a single RTCP packet with multiple receiver reports in it.
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  // Receiver report may contain up to 31 report blocks, thus 2 reports are
  // needed to carry 40 blocks: 31 in the first, 9 in the last.
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 2);
  // RtcpParser remembers just the last receiver report, thus can't check number
  // of blocks in the first receiver report.
  EXPECT_THAT(rtcp_parser.receiver_report()->report_blocks(), SizeIs(9));
}

TEST_F(RtcpTransceiverImplTest, AttachMaxNumberOfReportBlocksToCompoundPacket) {
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks)
      .WillOnce([](size_t max_blocks) {
        return std::vector<ReportBlock>(max_blocks);
      });
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.rtcp_mode = RtcpMode::kCompound;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{0});
  // Send some fast feedback message. Because of compound mode, report blocks
  // should be attached.
  rtcp_transceiver.SendPictureLossIndication(/*ssrc=*/123);

  // Expect single RTCP packet with multiple receiver reports and a PLI.
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.pli()->num_packets(), 1);
}

TEST_F(RtcpTransceiverImplTest, SendsNack) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrc = 4321;
  std::vector<uint16_t> kMissingSequenceNumbers = {34, 37, 38};
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendNack(kRemoteSsrc, kMissingSequenceNumbers);

  EXPECT_EQ(rtcp_parser.nack()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.nack()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.nack()->media_ssrc(), kRemoteSsrc);
  EXPECT_EQ(rtcp_parser.nack()->packet_ids(), kMissingSequenceNumbers);
}

TEST_F(RtcpTransceiverImplTest, ReceivesNack) {
  static constexpr uint32_t kRemoteSsrc = 4321;
  static constexpr uint32_t kMediaSsrc1 = 1234;
  static constexpr uint32_t kMediaSsrc2 = 1235;
  std::vector<uint16_t> kMissingSequenceNumbers = {34, 37, 38};
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_stream1;
  MockRtpStreamRtcpHandler local_stream2;
  EXPECT_CALL(local_stream1,
              OnNack(kRemoteSsrc, ElementsAreArray(kMissingSequenceNumbers)));
  EXPECT_CALL(local_stream2, OnNack).Times(0);

  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc1, &local_stream1));
  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc2, &local_stream2));

  rtcp::Nack nack;
  nack.SetSenderSsrc(kRemoteSsrc);
  nack.SetMediaSsrc(kMediaSsrc1);
  nack.SetPacketIds(kMissingSequenceNumbers);
  rtcp_transceiver.ReceivePacket(nack.Build(), config.clock->CurrentTime());
}

TEST_F(RtcpTransceiverImplTest, RequestKeyFrameWithPictureLossIndication) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrc = 4321;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendPictureLossIndication(kRemoteSsrc);

  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  EXPECT_EQ(rtcp_parser.pli()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.pli()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.pli()->media_ssrc(), kRemoteSsrc);
}

TEST_F(RtcpTransceiverImplTest, ReceivesPictureLossIndication) {
  static constexpr uint32_t kRemoteSsrc = 4321;
  static constexpr uint32_t kMediaSsrc1 = 1234;
  static constexpr uint32_t kMediaSsrc2 = 1235;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_stream1;
  MockRtpStreamRtcpHandler local_stream2;
  EXPECT_CALL(local_stream1, OnPli(kRemoteSsrc));
  EXPECT_CALL(local_stream2, OnPli).Times(0);

  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc1, &local_stream1));
  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc2, &local_stream2));

  rtcp::Pli pli;
  pli.SetSenderSsrc(kRemoteSsrc);
  pli.SetMediaSsrc(kMediaSsrc1);
  rtcp_transceiver.ReceivePacket(pli.Build(), config.clock->CurrentTime());
}

TEST_F(RtcpTransceiverImplTest, RequestKeyFrameWithFullIntraRequest) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrcs[] = {4321, 5321};
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs, true);

  EXPECT_EQ(rtcp_parser.fir()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.fir()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kRemoteSsrcs[1]);
}

TEST_F(RtcpTransceiverImplTest, RequestKeyFrameWithFirIncreaseSeqNoPerSsrc) {
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  const uint32_t kBothRemoteSsrcs[] = {4321, 5321};
  const uint32_t kOneRemoteSsrc[] = {4321};

  rtcp_transceiver.SendFullIntraRequest(kBothRemoteSsrcs, true);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  uint8_t fir_sequence_number0 = rtcp_parser.fir()->requests()[0].seq_nr;
  ASSERT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kBothRemoteSsrcs[1]);
  uint8_t fir_sequence_number1 = rtcp_parser.fir()->requests()[1].seq_nr;

  rtcp_transceiver.SendFullIntraRequest(kOneRemoteSsrc, true);
  ASSERT_EQ(rtcp_parser.fir()->requests().size(), 1u);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].seq_nr, fir_sequence_number0 + 1);

  rtcp_transceiver.SendFullIntraRequest(kBothRemoteSsrcs, true);
  ASSERT_EQ(rtcp_parser.fir()->requests().size(), 2u);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].seq_nr, fir_sequence_number0 + 2);
  ASSERT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kBothRemoteSsrcs[1]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[1].seq_nr, fir_sequence_number1 + 1);
}

TEST_F(RtcpTransceiverImplTest, SendFirDoesNotIncreaseSeqNoIfOldRequest) {
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  const uint32_t kBothRemoteSsrcs[] = {4321, 5321};

  rtcp_transceiver.SendFullIntraRequest(kBothRemoteSsrcs, true);
  ASSERT_EQ(rtcp_parser.fir()->requests().size(), 2u);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  uint8_t fir_sequence_number0 = rtcp_parser.fir()->requests()[0].seq_nr;
  ASSERT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kBothRemoteSsrcs[1]);
  uint8_t fir_sequence_number1 = rtcp_parser.fir()->requests()[1].seq_nr;

  rtcp_transceiver.SendFullIntraRequest(kBothRemoteSsrcs, false);
  ASSERT_EQ(rtcp_parser.fir()->requests().size(), 2u);
  ASSERT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kBothRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].seq_nr, fir_sequence_number0);
  ASSERT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kBothRemoteSsrcs[1]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[1].seq_nr, fir_sequence_number1);
}

TEST_F(RtcpTransceiverImplTest, ReceivesFir) {
  static constexpr uint32_t kRemoteSsrc = 4321;
  static constexpr uint32_t kMediaSsrc1 = 1234;
  static constexpr uint32_t kMediaSsrc2 = 1235;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_stream1;
  MockRtpStreamRtcpHandler local_stream2;
  EXPECT_CALL(local_stream1, OnFir(kRemoteSsrc));
  EXPECT_CALL(local_stream2, OnFir).Times(0);

  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc1, &local_stream1));
  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc2, &local_stream2));

  rtcp::Fir fir;
  fir.SetSenderSsrc(kRemoteSsrc);
  fir.AddRequestTo(kMediaSsrc1, /*seq_num=*/13);

  rtcp_transceiver.ReceivePacket(fir.Build(), config.clock->CurrentTime());
}

TEST_F(RtcpTransceiverImplTest, IgnoresReceivedFirWithRepeatedSequenceNumber) {
  static constexpr uint32_t kRemoteSsrc = 4321;
  static constexpr uint32_t kMediaSsrc1 = 1234;
  static constexpr uint32_t kMediaSsrc2 = 1235;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_stream1;
  MockRtpStreamRtcpHandler local_stream2;
  EXPECT_CALL(local_stream1, OnFir(kRemoteSsrc)).Times(1);
  EXPECT_CALL(local_stream2, OnFir(kRemoteSsrc)).Times(2);

  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc1, &local_stream1));
  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc2, &local_stream2));

  rtcp::Fir fir1;
  fir1.SetSenderSsrc(kRemoteSsrc);
  fir1.AddRequestTo(kMediaSsrc1, /*seq_num=*/132);
  fir1.AddRequestTo(kMediaSsrc2, /*seq_num=*/10);
  rtcp_transceiver.ReceivePacket(fir1.Build(), config.clock->CurrentTime());

  // Repeat request for MediaSsrc1 - expect it to be ignored,
  // Change FIR sequence number for MediaSsrc2 - expect a 2nd callback.
  rtcp::Fir fir2;
  fir2.SetSenderSsrc(kRemoteSsrc);
  fir2.AddRequestTo(kMediaSsrc1, /*seq_num=*/132);
  fir2.AddRequestTo(kMediaSsrc2, /*seq_num=*/13);
  rtcp_transceiver.ReceivePacket(fir2.Build(), config.clock->CurrentTime());
}

TEST_F(RtcpTransceiverImplTest, ReceivedFirTracksSequenceNumberPerRemoteSsrc) {
  static constexpr uint32_t kRemoteSsrc1 = 4321;
  static constexpr uint32_t kRemoteSsrc2 = 4323;
  static constexpr uint32_t kMediaSsrc = 1234;
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_stream;
  EXPECT_CALL(local_stream, OnFir(kRemoteSsrc1));
  EXPECT_CALL(local_stream, OnFir(kRemoteSsrc2));

  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc, &local_stream));

  rtcp::Fir fir1;
  fir1.SetSenderSsrc(kRemoteSsrc1);
  fir1.AddRequestTo(kMediaSsrc, /*seq_num=*/13);
  rtcp_transceiver.ReceivePacket(fir1.Build(), config.clock->CurrentTime());

  // Use the same FIR sequence number, but different sender SSRC.
  rtcp::Fir fir2;
  fir2.SetSenderSsrc(kRemoteSsrc2);
  fir2.AddRequestTo(kMediaSsrc, /*seq_num=*/13);
  rtcp_transceiver.ReceivePacket(fir2.Build(), config.clock->CurrentTime());
}

TEST_F(RtcpTransceiverImplTest, KeyFrameRequestCreatesCompoundPacket) {
  const uint32_t kRemoteSsrcs[] = {4321};
  RtcpTransceiverConfig config = DefaultTestConfig();
  // Turn periodic off to ensure sent rtcp packet is explicitly requested.
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);

  config.rtcp_mode = webrtc::RtcpMode::kCompound;

  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs, true);

  // Test sent packet is compound by expecting presense of receiver report.
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 1);
}

TEST_F(RtcpTransceiverImplTest, KeyFrameRequestCreatesReducedSizePacket) {
  const uint32_t kRemoteSsrcs[] = {4321};
  RtcpTransceiverConfig config = DefaultTestConfig();
  // Turn periodic off to ensure sent rtcp packet is explicitly requested.
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);

  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;

  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs, true);

  // Test sent packet is reduced size by expecting absense of receiver report.
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 0);
}

TEST_F(RtcpTransceiverImplTest, SendsXrRrtrWhenEnabled) {
  const uint32_t kSenderSsrc = 4321;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.non_sender_rtt_measurement = true;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();
  NtpTime ntp_time_now = config.clock->CurrentNtpTime();

  EXPECT_EQ(rtcp_parser.xr()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.xr()->sender_ssrc(), kSenderSsrc);
  ASSERT_TRUE(rtcp_parser.xr()->rrtr());
  EXPECT_EQ(rtcp_parser.xr()->rrtr()->ntp(), ntp_time_now);
}

TEST_F(RtcpTransceiverImplTest, RepliesToRrtrWhenEnabled) {
  static constexpr uint32_t kSenderSsrc[] = {4321, 9876};
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.reply_to_non_sender_rtt_measurement = true;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp::ExtendedReports xr;
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(NtpTime(uint64_t{0x1111'2222'3333'4444}));
  xr.SetRrtr(rrtr);
  xr.SetSenderSsrc(kSenderSsrc[0]);
  rtcp_transceiver.ReceivePacket(xr.Build(), CurrentTime());
  AdvanceTime(TimeDelta::Millis(1'500));

  rrtr.SetNtp(NtpTime(uint64_t{0x4444'5555'6666'7777}));
  xr.SetRrtr(rrtr);
  xr.SetSenderSsrc(kSenderSsrc[1]);
  rtcp_transceiver.ReceivePacket(xr.Build(), CurrentTime());
  AdvanceTime(TimeDelta::Millis(500));

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.xr()->num_packets(), 1);
  static constexpr uint32_t kComactNtpOneSecond = 0x0001'0000;
  EXPECT_THAT(rtcp_parser.xr()->dlrr().sub_blocks(),
              UnorderedElementsAre(
                  rtcp::ReceiveTimeInfo(kSenderSsrc[0], 0x2222'3333,
                                        /*delay=*/2 * kComactNtpOneSecond),
                  rtcp::ReceiveTimeInfo(kSenderSsrc[1], 0x5555'6666,
                                        /*delay=*/kComactNtpOneSecond / 2)));
}

TEST_F(RtcpTransceiverImplTest, CanReplyToRrtrOnceForAllLocalSsrcs) {
  static constexpr uint32_t kRemoteSsrc = 4321;
  static constexpr uint32_t kLocalSsrcs[] = {1234, 5678};
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.reply_to_non_sender_rtt_measurement = true;
  config.reply_to_non_sender_rtt_mesaurments_on_all_ssrcs = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_sender0;
  MockRtpStreamRtcpHandler local_sender1;
  rtcp_transceiver.AddMediaSender(kLocalSsrcs[0], &local_sender0);
  rtcp_transceiver.AddMediaSender(kLocalSsrcs[1], &local_sender1);

  rtcp::ExtendedReports xr;
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(NtpTime(uint64_t{0x1111'2222'3333'4444}));
  xr.SetRrtr(rrtr);
  xr.SetSenderSsrc(kRemoteSsrc);
  rtcp_transceiver.ReceivePacket(xr.Build(), CurrentTime());
  AdvanceTime(TimeDelta::Millis(1'500));

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.xr()->num_packets(), 1);
}

TEST_F(RtcpTransceiverImplTest, CanReplyToRrtrForEachLocalSsrc) {
  static constexpr uint32_t kRemoteSsrc = 4321;
  static constexpr uint32_t kLocalSsrc[] = {1234, 5678};
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.reply_to_non_sender_rtt_measurement = true;
  config.reply_to_non_sender_rtt_mesaurments_on_all_ssrcs = true;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_sender0;
  MockRtpStreamRtcpHandler local_sender1;
  rtcp_transceiver.AddMediaSender(kLocalSsrc[0], &local_sender0);
  rtcp_transceiver.AddMediaSender(kLocalSsrc[1], &local_sender1);

  rtcp::ExtendedReports xr;
  rtcp::Rrtr rrtr;
  rrtr.SetNtp(NtpTime(uint64_t{0x1111'2222'3333'4444}));
  xr.SetRrtr(rrtr);
  xr.SetSenderSsrc(kRemoteSsrc);
  rtcp_transceiver.ReceivePacket(xr.Build(), CurrentTime());
  AdvanceTime(TimeDelta::Millis(1'500));

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.xr()->num_packets(), 2);
}

TEST_F(RtcpTransceiverImplTest, SendsNoXrRrtrWhenDisabled) {
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.non_sender_rtt_measurement = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  // Extended reports rtcp packet might be included for another reason,
  // but it shouldn't contain rrtr block.
  EXPECT_FALSE(rtcp_parser.xr()->rrtr());
}

TEST_F(RtcpTransceiverImplTest, PassRttFromDlrrToLinkObserver) {
  const uint32_t kSenderSsrc = 4321;
  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.network_link_observer = &link_observer;
  config.non_sender_rtt_measurement = true;
  RtcpTransceiverImpl rtcp_transceiver(config);

  Timestamp send_time = Timestamp::Seconds(5678);
  Timestamp receive_time = send_time + TimeDelta::Millis(110);
  rtcp::ReceiveTimeInfo rti;
  rti.ssrc = kSenderSsrc;
  rti.last_rr = CompactNtp(config.clock->ConvertTimestampToNtpTime(send_time));
  rti.delay_since_last_rr = SaturatedToCompactNtp(TimeDelta::Millis(10));
  rtcp::ExtendedReports xr;
  xr.AddDlrrItem(rti);

  EXPECT_CALL(link_observer,
              OnRttUpdate(receive_time, Near(TimeDelta::Millis(100))));
  rtcp_transceiver.ReceivePacket(xr.Build(), receive_time);
}

TEST_F(RtcpTransceiverImplTest, CalculatesRoundTripTimeFromReportBlocks) {
  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.network_link_observer = &link_observer;
  RtcpTransceiverImpl rtcp_transceiver(config);

  TimeDelta rtt = TimeDelta::Millis(100);
  Timestamp send_time = Timestamp::Seconds(5678);
  Timestamp receive_time = send_time + TimeDelta::Millis(110);
  rtcp::ReceiverReport rr;
  rtcp::ReportBlock rb1;
  rb1.SetLastSr(CompactNtp(config.clock->ConvertTimestampToNtpTime(
      receive_time - rtt - TimeDelta::Millis(10))));
  rb1.SetDelayLastSr(SaturatedToCompactNtp(TimeDelta::Millis(10)));
  rr.AddReportBlock(rb1);
  rtcp::ReportBlock rb2;
  rb2.SetLastSr(CompactNtp(config.clock->ConvertTimestampToNtpTime(
      receive_time - rtt - TimeDelta::Millis(20))));
  rb2.SetDelayLastSr(SaturatedToCompactNtp(TimeDelta::Millis(20)));
  rr.AddReportBlock(rb2);

  EXPECT_CALL(link_observer, OnRttUpdate(receive_time, Near(rtt)));
  rtcp_transceiver.ReceivePacket(rr.Build(), receive_time);
}

TEST_F(RtcpTransceiverImplTest, IgnoresUnknownSsrcInDlrr) {
  const uint32_t kSenderSsrc = 4321;
  const uint32_t kUnknownSsrc = 4322;
  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  config.non_sender_rtt_measurement = true;
  config.network_link_observer = &link_observer;
  RtcpTransceiverImpl rtcp_transceiver(config);

  Timestamp time = Timestamp::Micros(12345678);
  webrtc::rtcp::ReceiveTimeInfo rti;
  rti.ssrc = kUnknownSsrc;
  rti.last_rr = CompactNtp(config.clock->ConvertTimestampToNtpTime(time));
  webrtc::rtcp::ExtendedReports xr;
  xr.AddDlrrItem(rti);
  auto raw_packet = xr.Build();

  EXPECT_CALL(link_observer, OnRttUpdate).Times(0);
  rtcp_transceiver.ReceivePacket(raw_packet, time + TimeDelta::Millis(100));
}

TEST_F(RtcpTransceiverImplTest, ParsesTransportFeedback) {
  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.network_link_observer = &link_observer;
  Timestamp receive_time = Timestamp::Seconds(5678);
  RtcpTransceiverImpl rtcp_transceiver(config);

  EXPECT_CALL(link_observer, OnTransportFeedback(receive_time, _))
      .WillOnce(WithArg<1>([](const rtcp::TransportFeedback& message) {
        EXPECT_EQ(message.GetBaseSequence(), 321);
        EXPECT_THAT(message.GetReceivedPackets(), SizeIs(2));
      }));

  rtcp::TransportFeedback tb;
  tb.SetBase(/*base_sequence=*/321, Timestamp::Micros(15));
  tb.AddReceivedPacket(/*base_sequence=*/321, Timestamp::Micros(15));
  tb.AddReceivedPacket(/*base_sequence=*/322, Timestamp::Micros(17));
  rtcp_transceiver.ReceivePacket(tb.Build(), receive_time);
}

TEST_F(RtcpTransceiverImplTest, ParsesRemb) {
  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.network_link_observer = &link_observer;
  Timestamp receive_time = Timestamp::Seconds(5678);
  RtcpTransceiverImpl rtcp_transceiver(config);

  EXPECT_CALL(link_observer,
              OnReceiverEstimatedMaxBitrate(receive_time,
                                            DataRate::BitsPerSec(1'234'000)));

  rtcp::Remb remb;
  remb.SetBitrateBps(1'234'000);
  rtcp_transceiver.ReceivePacket(remb.Build(), receive_time);
}

TEST_F(RtcpTransceiverImplTest,
       CombinesReportBlocksFromSenderAndRecieverReports) {
  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.network_link_observer = &link_observer;
  Timestamp receive_time = Timestamp::Seconds(5678);
  RtcpTransceiverImpl rtcp_transceiver(config);

  // Assemble compound packet with multiple rtcp packets in it.
  rtcp::CompoundPacket packet;
  auto sr = std::make_unique<rtcp::SenderReport>();
  sr->SetSenderSsrc(1234);
  sr->SetReportBlocks(std::vector<ReportBlock>(31));
  packet.Append(std::move(sr));
  auto rr1 = std::make_unique<rtcp::ReceiverReport>();
  rr1->SetReportBlocks(std::vector<ReportBlock>(31));
  packet.Append(std::move(rr1));
  auto rr2 = std::make_unique<rtcp::ReceiverReport>();
  rr2->SetReportBlocks(std::vector<ReportBlock>(2));
  packet.Append(std::move(rr2));

  EXPECT_CALL(link_observer, OnReport(receive_time, SizeIs(64)));

  rtcp_transceiver.ReceivePacket(packet.Build(), receive_time);
}

TEST_F(RtcpTransceiverImplTest,
       CallbackOnReportBlocksFromSenderAndReceiverReports) {
  static constexpr uint32_t kRemoteSsrc = 5678;
  // Has registered sender, report block attached to sender report.
  static constexpr uint32_t kMediaSsrc1 = 1234;
  // No registered sender, report block attached to receiver report.
  // Such report block shouldn't prevent handling following report block.
  static constexpr uint32_t kMediaSsrc2 = 1235;
  // Has registered sender, no report block attached.
  static constexpr uint32_t kMediaSsrc3 = 1236;
  // Has registered sender, report block attached to receiver report.
  static constexpr uint32_t kMediaSsrc4 = 1237;

  MockNetworkLinkRtcpObserver link_observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  Timestamp receive_time = Timestamp::Seconds(5678);
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler local_stream1;
  MockRtpStreamRtcpHandler local_stream3;
  MockRtpStreamRtcpHandler local_stream4;
  EXPECT_CALL(local_stream1,
              OnReport(Property(&ReportBlockData::sender_ssrc, kRemoteSsrc)));
  EXPECT_CALL(local_stream3, OnReport).Times(0);
  EXPECT_CALL(local_stream4,
              OnReport(Property(&ReportBlockData::sender_ssrc, kRemoteSsrc)));

  ASSERT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc1, &local_stream1));
  ASSERT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc3, &local_stream3));
  ASSERT_TRUE(rtcp_transceiver.AddMediaSender(kMediaSsrc4, &local_stream4));

  // Assemble compound packet with multiple RTCP packets in it.
  rtcp::CompoundPacket packet;
  auto sr = std::make_unique<rtcp::SenderReport>();
  sr->SetSenderSsrc(kRemoteSsrc);
  std::vector<ReportBlock> rb(1);
  rb[0].SetMediaSsrc(kMediaSsrc1);
  sr->SetReportBlocks(std::move(rb));
  packet.Append(std::move(sr));
  auto rr = std::make_unique<rtcp::ReceiverReport>();
  rr->SetSenderSsrc(kRemoteSsrc);
  rb = std::vector<ReportBlock>(2);
  rb[0].SetMediaSsrc(kMediaSsrc2);
  rb[1].SetMediaSsrc(kMediaSsrc4);
  rr->SetReportBlocks(std::move(rb));
  packet.Append(std::move(rr));

  rtcp_transceiver.ReceivePacket(packet.Build(), receive_time);
}

TEST_F(RtcpTransceiverImplTest, FailsToRegisterTwoSendersWithTheSameSsrc) {
  RtcpTransceiverImpl rtcp_transceiver(DefaultTestConfig());
  MockRtpStreamRtcpHandler sender1;
  MockRtpStreamRtcpHandler sender2;

  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(/*local_ssrc=*/10001, &sender1));
  EXPECT_FALSE(rtcp_transceiver.AddMediaSender(/*local_ssrc=*/10001, &sender2));
  EXPECT_TRUE(rtcp_transceiver.AddMediaSender(/*local_ssrc=*/10002, &sender2));

  EXPECT_TRUE(rtcp_transceiver.RemoveMediaSender(/*local_ssrc=*/10001));
  EXPECT_FALSE(rtcp_transceiver.RemoveMediaSender(/*local_ssrc=*/10001));
}

TEST_F(RtcpTransceiverImplTest, SendsSenderReport) {
  static constexpr uint32_t kFeedbackSsrc = 123;
  static constexpr uint32_t kSenderSsrc = 12345;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.feedback_ssrc = kFeedbackSsrc;
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  RtpStreamRtcpHandler::RtpStats sender_stats;
  sender_stats.set_num_sent_packets(10);
  sender_stats.set_num_sent_bytes(1000);
  sender_stats.set_last_rtp_timestamp(0x3333);
  sender_stats.set_last_capture_time(CurrentTime() - TimeDelta::Seconds(2));
  sender_stats.set_last_clock_rate(0x1000);
  MockRtpStreamRtcpHandler sender;
  ON_CALL(sender, SentStats).WillByDefault(Return(sender_stats));
  rtcp_transceiver.AddMediaSender(kSenderSsrc, &sender);

  rtcp_transceiver.SendCompoundPacket();

  ASSERT_GT(rtcp_parser.sender_report()->num_packets(), 0);
  EXPECT_EQ(rtcp_parser.sender_report()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.sender_report()->ntp(),
            time_controller().GetClock()->CurrentNtpTime());
  EXPECT_EQ(rtcp_parser.sender_report()->rtp_timestamp(), 0x3333u + 0x2000u);
  EXPECT_EQ(rtcp_parser.sender_report()->sender_packet_count(), 10u);
  EXPECT_EQ(rtcp_parser.sender_report()->sender_octet_count(), 1000u);
}

TEST_F(RtcpTransceiverImplTest,
       MaySendBothSenderReportAndReceiverReportInTheSamePacket) {
  RtcpPacketParser rtcp_parser;
  std::vector<ReportBlock> statistics_report_blocks(40);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(/*max_blocks=*/Ge(40u)))
      .WillOnce(Return(statistics_report_blocks));
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  MockRtpStreamRtcpHandler sender;
  rtcp_transceiver.AddMediaSender(/*ssrc=*/12345, &sender);

  rtcp_transceiver.SendCompoundPacket();

  // Expect a single RTCP packet with a sender and a receiver reports in it.
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  ASSERT_EQ(rtcp_parser.sender_report()->num_packets(), 1);
  ASSERT_EQ(rtcp_parser.receiver_report()->num_packets(), 1);
  // Sender report may contain up to 31 report blocks, thus remaining 9 report
  // block should be attached to the receiver report.
  EXPECT_THAT(rtcp_parser.sender_report()->report_blocks(), SizeIs(31));
  EXPECT_THAT(rtcp_parser.receiver_report()->report_blocks(), SizeIs(9));
}

TEST_F(RtcpTransceiverImplTest, RotatesSendersWhenAllSenderReportDoNotFit) {
  // Send 6 compound packet, each should contain 5 sender reports,
  // each of 6 senders should be mentioned 5 times.
  static constexpr int kNumSenders = 6;
  static constexpr uint32_t kSenderSsrc[kNumSenders] = {10, 20, 30, 40, 50, 60};
  static constexpr int kSendersPerPacket = 5;
  // RtcpPacketParser remembers only latest block for each type, but this test
  // is about sending multiple sender reports in the same packet, thus need
  // a more advance parser: RtcpTranceiver
  RtcpTransceiverConfig receiver_config = DefaultTestConfig();
  RtcpTransceiverImpl rtcp_receiver(receiver_config);
  // Main expectatation: all senders are spread equally across multiple packets.
  NiceMock<MockMediaReceiverRtcpObserver> receiver[kNumSenders];
  for (int i = 0; i < kNumSenders; ++i) {
    SCOPED_TRACE(i);
    EXPECT_CALL(receiver[i], OnSenderReport(kSenderSsrc[i], _, _))
        .Times(kSendersPerPacket);
    rtcp_receiver.AddMediaReceiverRtcpObserver(kSenderSsrc[i], &receiver[i]);
  }

  MockFunction<void(rtc::ArrayView<const uint8_t>)> transport;
  EXPECT_CALL(transport, Call)
      .Times(kNumSenders)
      .WillRepeatedly([&](rtc::ArrayView<const uint8_t> packet) {
        rtcp_receiver.ReceivePacket(packet, CurrentTime());
        return true;
      });
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.rtcp_transport = transport.AsStdFunction();
  // Limit packet to have space just for kSendersPerPacket sender reports.
  // Sender report without report blocks require 28 bytes.
  config.max_packet_size = kSendersPerPacket * 28;
  RtcpTransceiverImpl rtcp_transceiver(config);
  NiceMock<MockRtpStreamRtcpHandler> sender[kNumSenders];
  for (int i = 0; i < kNumSenders; ++i) {
    rtcp_transceiver.AddMediaSender(kSenderSsrc[i], &sender[i]);
  }

  for (int i = 1; i <= kNumSenders; ++i) {
    SCOPED_TRACE(i);
    rtcp_transceiver.SendCompoundPacket();
  }
}

TEST_F(RtcpTransceiverImplTest, SkipsSenderReportForInactiveSender) {
  static constexpr uint32_t kSenderSsrc[] = {12345, 23456};
  RtcpTransceiverConfig config = DefaultTestConfig();
  RtcpPacketParser rtcp_parser;
  config.rtcp_transport = RtcpParserTransport(rtcp_parser);
  RtcpTransceiverImpl rtcp_transceiver(config);

  RtpStreamRtcpHandler::RtpStats sender_stats[2];
  NiceMock<MockRtpStreamRtcpHandler> sender[2];
  ON_CALL(sender[0], SentStats).WillByDefault([&] { return sender_stats[0]; });
  ON_CALL(sender[1], SentStats).WillByDefault([&] { return sender_stats[1]; });
  rtcp_transceiver.AddMediaSender(kSenderSsrc[0], &sender[0]);
  rtcp_transceiver.AddMediaSender(kSenderSsrc[1], &sender[1]);

  // Start with both senders beeing active.
  sender_stats[0].set_num_sent_packets(10);
  sender_stats[0].set_num_sent_bytes(1'000);
  sender_stats[1].set_num_sent_packets(5);
  sender_stats[1].set_num_sent_bytes(2'000);
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{1});
  EXPECT_EQ(rtcp_parser.sender_report()->num_packets(), 2);

  // Keep 1st sender active, but make 2nd second look inactive by returning the
  // same RtpStats.
  sender_stats[0].set_num_sent_packets(15);
  sender_stats[0].set_num_sent_bytes(2'000);
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{2});
  EXPECT_EQ(rtcp_parser.sender_report()->num_packets(), 3);
  EXPECT_EQ(rtcp_parser.sender_report()->sender_ssrc(), kSenderSsrc[0]);

  // Swap active sender.
  sender_stats[1].set_num_sent_packets(20);
  sender_stats[1].set_num_sent_bytes(3'000);
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{3});
  EXPECT_EQ(rtcp_parser.sender_report()->num_packets(), 4);
  EXPECT_EQ(rtcp_parser.sender_report()->sender_ssrc(), kSenderSsrc[1]);

  // Activate both senders again.
  sender_stats[0].set_num_sent_packets(20);
  sender_stats[0].set_num_sent_bytes(3'000);
  sender_stats[1].set_num_sent_packets(25);
  sender_stats[1].set_num_sent_bytes(3'500);
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(rtcp_parser.processed_rtcp_packets(), size_t{4});
  EXPECT_EQ(rtcp_parser.sender_report()->num_packets(), 6);
}

}  // namespace
}  // namespace webrtc
