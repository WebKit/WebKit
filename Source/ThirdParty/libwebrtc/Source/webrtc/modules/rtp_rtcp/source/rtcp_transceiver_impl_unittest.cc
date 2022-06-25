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
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_bitrate_allocation.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/mocks/mock_rtcp_rtt_stats.h"
#include "modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrictMock;
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

class MockNetworkLinkRtcpObserver : public NetworkLinkRtcpObserver {
 public:
  MOCK_METHOD(void,
              OnRttUpdate,
              (Timestamp receive_time, TimeDelta rtt),
              (override));
  MOCK_METHOD(void,
              OnTransportFeedback,
              (Timestamp receive_time, const rtcp::TransportFeedback& feedback),
              (override));
  MOCK_METHOD(void,
              OnReceiverEstimatedMaxBitrate,
              (Timestamp receive_time, DataRate bitrate),
              (override));
  MOCK_METHOD(void,
              OnReportBlocks,
              (Timestamp receive_time,
               rtc::ArrayView<const rtcp::ReportBlock> report_blocks),
              (override));
};

// Since some tests will need to wait for this period, make it small to avoid
// slowing tests too much. As long as there are test bots with high scheduler
// granularity, small period should be ok.
constexpr int kReportPeriodMs = 10;
// On some systems task queue might be slow, instead of guessing right
// grace period, use very large timeout, 100x larger expected wait time.
// Use finite timeout to fail tests rather than hang them.
constexpr int kAlmostForeverMs = 1000;

// Helper to wait for an rtcp packet produced on a different thread/task queue.
class FakeRtcpTransport : public webrtc::Transport {
 public:
  bool SendRtcp(const uint8_t* data, size_t size) override {
    sent_rtcp_.Set();
    return true;
  }
  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return true;
  }

  // Returns true when packet was received by the transport.
  bool WaitPacket() {
    // Normally packet should be sent fast, long before the timeout.
    bool packet_sent = sent_rtcp_.Wait(kAlmostForeverMs);
    // Disallow tests to wait almost forever for no packets.
    EXPECT_TRUE(packet_sent);
    // Return wait result even though it is expected to be true, so that
    // individual tests can EXPECT on it for better error message.
    return packet_sent;
  }

 private:
  rtc::Event sent_rtcp_;
};

class RtcpParserTransport : public webrtc::Transport {
 public:
  explicit RtcpParserTransport(RtcpPacketParser* parser) : parser_(parser) {}
  // Returns total number of rtcp packet received.
  int num_packets() const { return num_packets_; }

 private:
  bool SendRtcp(const uint8_t* data, size_t size) override {
    ++num_packets_;
    parser_->Parse(data, size);
    return true;
  }

  bool SendRtp(const uint8_t*, size_t, const webrtc::PacketOptions&) override {
    ADD_FAILURE() << "RtcpTransciver shouldn't send rtp packets.";
    return true;
  }

  RtcpPacketParser* const parser_;
  int num_packets_ = 0;
};

RtcpTransceiverConfig DefaultTestConfig() {
  // RtcpTransceiverConfig default constructor sets default values for prod.
  // Test doesn't need to support all key features: Default test config returns
  // valid config with all features turned off.
  static MockTransport null_transport;
  static SimulatedClock null_clock(0);
  RtcpTransceiverConfig config;
  config.clock = &null_clock;
  config.outgoing_transport = &null_transport;
  config.schedule_periodic_compound_packets = false;
  config.initial_report_delay_ms = 10;
  config.report_period_ms = kReportPeriodMs;
  return config;
}

TEST(RtcpTransceiverImplTest, NeedToStopPeriodicTaskToDestroyOnTaskQueue) {
  SimulatedClock clock(0);
  FakeRtcpTransport transport;
  TaskQueueForTest queue("rtcp");
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.task_queue = queue.Get();
  config.schedule_periodic_compound_packets = true;
  config.outgoing_transport = &transport;
  auto* rtcp_transceiver = new RtcpTransceiverImpl(config);
  // Wait for a periodic packet.
  EXPECT_TRUE(transport.WaitPacket());

  rtc::Event done;
  queue.PostTask([rtcp_transceiver, &done] {
    rtcp_transceiver->StopPeriodicTask();
    delete rtcp_transceiver;
    done.Set();
  });
  ASSERT_TRUE(done.Wait(/*milliseconds=*/1000));
}

TEST(RtcpTransceiverImplTest, CanBeDestroyedRightAfterCreation) {
  SimulatedClock clock(0);
  FakeRtcpTransport transport;
  TaskQueueForTest queue("rtcp");
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.task_queue = queue.Get();
  config.schedule_periodic_compound_packets = true;
  config.outgoing_transport = &transport;

  rtc::Event done;
  queue.PostTask([&] {
    RtcpTransceiverImpl rtcp_transceiver(config);
    rtcp_transceiver.StopPeriodicTask();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(/*milliseconds=*/1000));
}

TEST(RtcpTransceiverImplTest, CanDestroyAfterTaskQueue) {
  SimulatedClock clock(0);
  FakeRtcpTransport transport;
  auto* queue = new TaskQueueForTest("rtcp");
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.task_queue = queue->Get();
  config.schedule_periodic_compound_packets = true;
  config.outgoing_transport = &transport;
  auto* rtcp_transceiver = new RtcpTransceiverImpl(config);
  // Wait for a periodic packet.
  EXPECT_TRUE(transport.WaitPacket());

  delete queue;
  delete rtcp_transceiver;
}

TEST(RtcpTransceiverImplTest, DelaysSendingFirstCompondPacket) {
  SimulatedClock clock(0);
  TaskQueueForTest queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 10;
  config.task_queue = queue.Get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;

  int64_t started_ms = rtc::TimeMillis();
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });
  EXPECT_TRUE(transport.WaitPacket());

  EXPECT_GE(rtc::TimeMillis() - started_ms, config.initial_report_delay_ms);

  // Cleanup.
  rtc::Event done;
  queue.PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, PeriodicallySendsPackets) {
  SimulatedClock clock(0);
  TaskQueueForTest queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = kReportPeriodMs;
  config.task_queue = queue.Get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;
  int64_t time_just_before_1st_packet_ms = 0;
  queue.PostTask([&] {
    // Because initial_report_delay_ms is set to 0, time_just_before_the_packet
    // should be very close to the time_of_the_packet.
    time_just_before_1st_packet_ms = rtc::TimeMillis();
    rtcp_transceiver.emplace(config);
  });

  EXPECT_TRUE(transport.WaitPacket());
  EXPECT_TRUE(transport.WaitPacket());
  int64_t time_just_after_2nd_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_just_after_2nd_packet_ms - time_just_before_1st_packet_ms,
            config.report_period_ms - 1);

  // Cleanup.
  rtc::Event done;
  queue.PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendCompoundPacketDelaysPeriodicSendPackets) {
  SimulatedClock clock(0);
  TaskQueueForTest queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.outgoing_transport = &transport;
  config.initial_report_delay_ms = 0;
  config.report_period_ms = kReportPeriodMs;
  config.task_queue = queue.Get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;
  queue.PostTask([&] { rtcp_transceiver.emplace(config); });

  // Wait for first packet.
  EXPECT_TRUE(transport.WaitPacket());
  // Send non periodic one after half period.
  rtc::Event non_periodic;
  int64_t time_of_non_periodic_packet_ms = 0;
  queue.PostDelayedTask(
      [&] {
        time_of_non_periodic_packet_ms = rtc::TimeMillis();
        rtcp_transceiver->SendCompoundPacket();
        non_periodic.Set();
      },
      config.report_period_ms / 2);
  // Though non-periodic packet is scheduled just in between periodic, due to
  // small period and task queue flakiness it migth end-up 1ms after next
  // periodic packet. To be sure duration after non-periodic packet is tested
  // wait for transport after ensuring non-periodic packet was sent.
  EXPECT_TRUE(non_periodic.Wait(kAlmostForeverMs));
  EXPECT_TRUE(transport.WaitPacket());
  // Wait for next periodic packet.
  EXPECT_TRUE(transport.WaitPacket());
  int64_t time_of_last_periodic_packet_ms = rtc::TimeMillis();

  EXPECT_GE(time_of_last_periodic_packet_ms - time_of_non_periodic_packet_ms,
            config.report_period_ms - 1);

  // Cleanup.
  rtc::Event done;
  queue.PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendsNoRtcpWhenNetworkStateIsDown) {
  SimulatedClock clock(0);
  MockTransport mock_transport;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.initial_ready_to_send = false;
  config.outgoing_transport = &mock_transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  EXPECT_CALL(mock_transport, SendRtcp(_, _)).Times(0);

  const uint8_t raw[] = {1, 2, 3, 4};
  const std::vector<uint16_t> sequence_numbers = {45, 57};
  const uint32_t ssrcs[] = {123};
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendRawPacket(raw);
  rtcp_transceiver.SendNack(ssrcs[0], sequence_numbers);
  rtcp_transceiver.SendPictureLossIndication(ssrcs[0]);
  rtcp_transceiver.SendFullIntraRequest(ssrcs, true);
}

TEST(RtcpTransceiverImplTest, SendsRtcpWhenNetworkStateIsUp) {
  SimulatedClock clock(0);
  MockTransport mock_transport;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.initial_ready_to_send = false;
  config.outgoing_transport = &mock_transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetReadyToSend(true);

  EXPECT_CALL(mock_transport, SendRtcp(_, _)).Times(5);

  const uint8_t raw[] = {1, 2, 3, 4};
  const std::vector<uint16_t> sequence_numbers = {45, 57};
  const uint32_t ssrcs[] = {123};
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendRawPacket(raw);
  rtcp_transceiver.SendNack(ssrcs[0], sequence_numbers);
  rtcp_transceiver.SendPictureLossIndication(ssrcs[0]);
  rtcp_transceiver.SendFullIntraRequest(ssrcs, true);
}

TEST(RtcpTransceiverImplTest, SendsPeriodicRtcpWhenNetworkStateIsUp) {
  SimulatedClock clock(0);
  TaskQueueForTest queue("rtcp");
  FakeRtcpTransport transport;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.schedule_periodic_compound_packets = true;
  config.initial_ready_to_send = false;
  config.outgoing_transport = &transport;
  config.task_queue = queue.Get();
  absl::optional<RtcpTransceiverImpl> rtcp_transceiver;
  rtcp_transceiver.emplace(config);

  queue.PostTask([&] { rtcp_transceiver->SetReadyToSend(true); });

  EXPECT_TRUE(transport.WaitPacket());

  // Cleanup.
  rtc::Event done;
  queue.PostTask([&] {
    rtcp_transceiver->StopPeriodicTask();
    rtcp_transceiver.reset();
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kAlmostForeverMs));
}

TEST(RtcpTransceiverImplTest, SendsMinimalCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.cname = "cname";
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest, AvoidsEmptyPacketsInReducedMode) {
  MockTransport transport;
  EXPECT_CALL(transport, SendRtcp).Times(0);
  NiceMock<MockReceiveStatisticsProvider> receive_statistics;
  SimulatedClock clock(0);

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.outgoing_transport = &transport;
  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;
  config.schedule_periodic_compound_packets = false;
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();
}

TEST(RtcpTransceiverImplTest, AvoidsEmptyReceiverReportsInReducedMode) {
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  NiceMock<MockReceiveStatisticsProvider> receive_statistics;
  SimulatedClock clock(0);

  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
  config.outgoing_transport = &transport;
  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;
  config.schedule_periodic_compound_packets = false;
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

TEST(RtcpTransceiverImplTest, SendsNoRembInitially) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 0);
}

TEST(RtcpTransceiverImplTest, SetRembIncludesRembInNextCompoundPacket) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000);
  EXPECT_THAT(rtcp_parser.remb()->ssrcs(), ElementsAre(54321, 64321));
}

TEST(RtcpTransceiverImplTest, SetRembUpdatesValuesToSend) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest, SetRembSendsImmediatelyIfSendRembOnChange) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.send_remb_on_change = true;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest,
     SetRembSendsImmediatelyIfSendRembOnChangeReducedSize) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.send_remb_on_change = true;
  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{});
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.remb()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.remb()->bitrate_bps(), 10000);
}

TEST(RtcpTransceiverImplTest, SetRembIncludesRembInAllCompoundPackets) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 2);
}

TEST(RtcpTransceiverImplTest, SendsNoRembAfterUnset) {
  const uint32_t kSenderSsrc = 12345;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SetRemb(/*bitrate_bps=*/10000, /*ssrcs=*/{54321, 64321});
  rtcp_transceiver.SendCompoundPacket();
  EXPECT_EQ(transport.num_packets(), 1);
  ASSERT_EQ(rtcp_parser.remb()->num_packets(), 1);

  rtcp_transceiver.UnsetRemb();
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 2);
  EXPECT_EQ(rtcp_parser.remb()->num_packets(), 1);
}

TEST(RtcpTransceiverImplTest, ReceiverReportUsesReceiveStatistics) {
  const uint32_t kSenderSsrc = 12345;
  const uint32_t kMediaSsrc = 54321;
  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest, MultipleObserversOnSameSsrc) {
  const uint32_t kRemoteSsrc = 12345;
  SimulatedClock clock(0);
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest, DoesntCallsObserverAfterRemoved) {
  const uint32_t kRemoteSsrc = 12345;
  SimulatedClock clock(0);
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest, CallsObserverOnSenderReportBySenderSsrc) {
  const uint32_t kRemoteSsrc1 = 12345;
  const uint32_t kRemoteSsrc2 = 22345;
  SimulatedClock clock(0);
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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
  EXPECT_CALL(observer2, OnSenderReport(_, _, _)).Times(0);
  rtcp_transceiver.ReceivePacket(raw_packet, Timestamp::Micros(0));
}

TEST(RtcpTransceiverImplTest, CallsObserverOnByeBySenderSsrc) {
  const uint32_t kRemoteSsrc1 = 12345;
  const uint32_t kRemoteSsrc2 = 22345;
  SimulatedClock clock(0);
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest, CallsObserverOnTargetBitrateBySenderSsrc) {
  const uint32_t kRemoteSsrc1 = 12345;
  const uint32_t kRemoteSsrc2 = 22345;
  SimulatedClock clock(0);
  StrictMock<MockMediaReceiverRtcpObserver> observer1;
  StrictMock<MockMediaReceiverRtcpObserver> observer2;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest, SkipsIncorrectTargetBitrateEntries) {
  const uint32_t kRemoteSsrc = 12345;
  SimulatedClock clock(0);
  MockMediaReceiverRtcpObserver observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest, CallsObserverOnByeBehindSenderReport) {
  const uint32_t kRemoteSsrc = 12345;
  SimulatedClock clock(0);
  MockMediaReceiverRtcpObserver observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest, CallsObserverOnByeBehindUnknownRtcpPacket) {
  const uint32_t kRemoteSsrc = 12345;
  SimulatedClock clock(0);
  MockMediaReceiverRtcpObserver observer;
  RtcpTransceiverConfig config = DefaultTestConfig();
  config.clock = &clock;
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

TEST(RtcpTransceiverImplTest,
     WhenSendsReceiverReportSetsLastSenderReportTimestampPerRemoteSsrc) {
  const uint32_t kRemoteSsrc1 = 4321;
  const uint32_t kRemoteSsrc2 = 5321;
  std::vector<ReportBlock> statistics_report_blocks(2);
  statistics_report_blocks[0].SetMediaSsrc(kRemoteSsrc1);
  statistics_report_blocks[1].SetMediaSsrc(kRemoteSsrc2);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(statistics_report_blocks));

  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest,
     WhenSendsReceiverReportCalculatesDelaySinceLastSenderReport) {
  const uint32_t kRemoteSsrc1 = 4321;
  const uint32_t kRemoteSsrc2 = 5321;

  std::vector<ReportBlock> statistics_report_blocks(2);
  statistics_report_blocks[0].SetMediaSsrc(kRemoteSsrc1);
  statistics_report_blocks[1].SetMediaSsrc(kRemoteSsrc2);
  MockReceiveStatisticsProvider receive_statistics;
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillOnce(Return(statistics_report_blocks));

  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.receive_statistics = &receive_statistics;
  RtcpTransceiverImpl rtcp_transceiver(config);

  auto receive_sender_report = [&rtcp_transceiver,
                                &clock](uint32_t remote_ssrc) {
    SenderReport sr;
    sr.SetSenderSsrc(remote_ssrc);
    auto raw_packet = sr.Build();
    rtcp_transceiver.ReceivePacket(raw_packet, clock.CurrentTime());
  };

  receive_sender_report(kRemoteSsrc1);
  clock.AdvanceTime(TimeDelta::Millis(100));

  receive_sender_report(kRemoteSsrc2);
  clock.AdvanceTime(TimeDelta::Millis(100));

  // Trigger ReceiverReport back.
  rtcp_transceiver.SendCompoundPacket();

  EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
  const auto& report_blocks = rtcp_parser.receiver_report()->report_blocks();
  ASSERT_EQ(report_blocks.size(), 2u);
  // RtcpTransceiverImpl doesn't guarantee order of the report blocks
  // match result of ReceiveStatisticsProvider::RtcpReportBlocks callback,
  // but for simplicity of the test asume it is the same.
  ASSERT_EQ(report_blocks[0].source_ssrc(), kRemoteSsrc1);
  EXPECT_EQ(CompactNtpRttToMs(report_blocks[0].delay_since_last_sr()), 200);

  ASSERT_EQ(report_blocks[1].source_ssrc(), kRemoteSsrc2);
  EXPECT_EQ(CompactNtpRttToMs(report_blocks[1].delay_since_last_sr()), 100);
}

TEST(RtcpTransceiverImplTest, SendsNack) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrc = 4321;
  std::vector<uint16_t> kMissingSequenceNumbers = {34, 37, 38};
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendNack(kRemoteSsrc, kMissingSequenceNumbers);

  EXPECT_EQ(rtcp_parser.nack()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.nack()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.nack()->media_ssrc(), kRemoteSsrc);
  EXPECT_EQ(rtcp_parser.nack()->packet_ids(), kMissingSequenceNumbers);
}

TEST(RtcpTransceiverImplTest, RequestKeyFrameWithPictureLossIndication) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrc = 4321;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendPictureLossIndication(kRemoteSsrc);

  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.pli()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.pli()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.pli()->media_ssrc(), kRemoteSsrc);
}

TEST(RtcpTransceiverImplTest, RequestKeyFrameWithFullIntraRequest) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kRemoteSsrcs[] = {4321, 5321};
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs, true);

  EXPECT_EQ(rtcp_parser.fir()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.fir()->sender_ssrc(), kSenderSsrc);
  EXPECT_EQ(rtcp_parser.fir()->requests()[0].ssrc, kRemoteSsrcs[0]);
  EXPECT_EQ(rtcp_parser.fir()->requests()[1].ssrc, kRemoteSsrcs[1]);
}

TEST(RtcpTransceiverImplTest, RequestKeyFrameWithFirIncreaseSeqNoPerSsrc) {
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest, SendFirDoesNotIncreaseSeqNoIfOldRequest) {
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
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

TEST(RtcpTransceiverImplTest, KeyFrameRequestCreatesCompoundPacket) {
  const uint32_t kRemoteSsrcs[] = {4321};
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  // Turn periodic off to ensure sent rtcp packet is explicitly requested.
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;

  config.rtcp_mode = webrtc::RtcpMode::kCompound;

  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs, true);

  // Test sent packet is compound by expecting presense of receiver report.
  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 1);
}

TEST(RtcpTransceiverImplTest, KeyFrameRequestCreatesReducedSizePacket) {
  const uint32_t kRemoteSsrcs[] = {4321};
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  // Turn periodic off to ensure sent rtcp packet is explicitly requested.
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;

  config.rtcp_mode = webrtc::RtcpMode::kReducedSize;

  RtcpTransceiverImpl rtcp_transceiver(config);
  rtcp_transceiver.SendFullIntraRequest(kRemoteSsrcs, true);

  // Test sent packet is reduced size by expecting absense of receiver report.
  EXPECT_EQ(transport.num_packets(), 1);
  EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 0);
}

TEST(RtcpTransceiverImplTest, SendsXrRrtrWhenEnabled) {
  const uint32_t kSenderSsrc = 4321;
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.non_sender_rtt_measurement = true;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();
  NtpTime ntp_time_now = clock.CurrentNtpTime();

  EXPECT_EQ(rtcp_parser.xr()->num_packets(), 1);
  EXPECT_EQ(rtcp_parser.xr()->sender_ssrc(), kSenderSsrc);
  ASSERT_TRUE(rtcp_parser.xr()->rrtr());
  EXPECT_EQ(rtcp_parser.xr()->rrtr()->ntp(), ntp_time_now);
}

TEST(RtcpTransceiverImplTest, SendsNoXrRrtrWhenDisabled) {
  SimulatedClock clock(0);
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.schedule_periodic_compound_packets = false;
  RtcpPacketParser rtcp_parser;
  RtcpParserTransport transport(&rtcp_parser);
  config.outgoing_transport = &transport;
  config.non_sender_rtt_measurement = false;
  RtcpTransceiverImpl rtcp_transceiver(config);

  rtcp_transceiver.SendCompoundPacket();

  EXPECT_EQ(transport.num_packets(), 1);
  // Extended reports rtcp packet might be included for another reason,
  // but it shouldn't contain rrtr block.
  EXPECT_FALSE(rtcp_parser.xr()->rrtr());
}

TEST(RtcpTransceiverImplTest, CalculatesRoundTripTimeOnDlrr) {
  const uint32_t kSenderSsrc = 4321;
  SimulatedClock clock(0);
  MockRtcpRttStats rtt_observer;
  MockTransport null_transport;
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  config.outgoing_transport = &null_transport;
  config.non_sender_rtt_measurement = true;
  config.rtt_observer = &rtt_observer;
  RtcpTransceiverImpl rtcp_transceiver(config);

  Timestamp time = Timestamp::Micros(12345678);
  webrtc::rtcp::ReceiveTimeInfo rti;
  rti.ssrc = kSenderSsrc;
  rti.last_rr = CompactNtp(clock.ConvertTimestampToNtpTime(time));
  rti.delay_since_last_rr = SaturatedUsToCompactNtp(10 * 1000);
  webrtc::rtcp::ExtendedReports xr;
  xr.AddDlrrItem(rti);
  auto raw_packet = xr.Build();

  EXPECT_CALL(rtt_observer, OnRttUpdate(100 /* rtt_ms */));
  rtcp_transceiver.ReceivePacket(raw_packet, time + TimeDelta::Millis(110));
}

TEST(RtcpTransceiverImplTest, PassRttFromDlrrToLinkObserver) {
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
  rti.delay_since_last_rr = SaturatedUsToCompactNtp(10'000);  // 10ms
  rtcp::ExtendedReports xr;
  xr.AddDlrrItem(rti);

  EXPECT_CALL(link_observer, OnRttUpdate(receive_time, TimeDelta::Millis(100)));
  rtcp_transceiver.ReceivePacket(xr.Build(), receive_time);
}

TEST(RtcpTransceiverImplTest, CalculatesRoundTripTimeFromReportBlocks) {
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
  rb1.SetDelayLastSr(SaturatedUsToCompactNtp(10'000));  // 10ms
  rr.AddReportBlock(rb1);
  rtcp::ReportBlock rb2;
  rb2.SetLastSr(CompactNtp(config.clock->ConvertTimestampToNtpTime(
      receive_time - rtt - TimeDelta::Millis(20))));
  rb2.SetDelayLastSr(SaturatedUsToCompactNtp(20'000));  // 20ms
  rr.AddReportBlock(rb2);

  EXPECT_CALL(link_observer, OnRttUpdate(receive_time, rtt));
  rtcp_transceiver.ReceivePacket(rr.Build(), receive_time);
}

TEST(RtcpTransceiverImplTest, IgnoresUnknownSsrcInDlrr) {
  const uint32_t kSenderSsrc = 4321;
  const uint32_t kUnknownSsrc = 4322;
  SimulatedClock clock(0);
  MockRtcpRttStats rtt_observer;
  MockTransport null_transport;
  RtcpTransceiverConfig config;
  config.clock = &clock;
  config.feedback_ssrc = kSenderSsrc;
  config.schedule_periodic_compound_packets = false;
  config.outgoing_transport = &null_transport;
  config.non_sender_rtt_measurement = true;
  config.rtt_observer = &rtt_observer;
  RtcpTransceiverImpl rtcp_transceiver(config);

  Timestamp time = Timestamp::Micros(12345678);
  webrtc::rtcp::ReceiveTimeInfo rti;
  rti.ssrc = kUnknownSsrc;
  rti.last_rr = CompactNtp(clock.ConvertTimestampToNtpTime(time));
  webrtc::rtcp::ExtendedReports xr;
  xr.AddDlrrItem(rti);
  auto raw_packet = xr.Build();

  EXPECT_CALL(rtt_observer, OnRttUpdate(_)).Times(0);
  rtcp_transceiver.ReceivePacket(raw_packet, time + TimeDelta::Millis(100));
}

TEST(RtcpTransceiverImplTest, ParsesTransportFeedback) {
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
  tb.SetBase(/*base_sequence=*/321, /*ref_timestamp_us=*/15);
  tb.AddReceivedPacket(/*base_sequence=*/321, /*timestamp_us=*/15);
  tb.AddReceivedPacket(/*base_sequence=*/322, /*timestamp_us=*/17);
  rtcp_transceiver.ReceivePacket(tb.Build(), receive_time);
}

TEST(RtcpTransceiverImplTest, ParsesRemb) {
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

TEST(RtcpTransceiverImplTest,
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

  EXPECT_CALL(link_observer, OnReportBlocks(receive_time, SizeIs(64)));

  rtcp_transceiver.ReceivePacket(packet.Build(), receive_time);
}

}  // namespace
}  // namespace webrtc
