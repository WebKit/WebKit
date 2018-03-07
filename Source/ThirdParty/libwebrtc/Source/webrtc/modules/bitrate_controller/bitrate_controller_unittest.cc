/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <vector>

#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/pacing/mock/mock_paced_sender.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "test/field_trial.h"
#include "test/gtest.h"

using ::testing::Exactly;
using ::testing::Return;

using webrtc::BitrateController;
using webrtc::BitrateObserver;
using webrtc::PacedSender;
using webrtc::RtcpBandwidthObserver;

uint8_t WeightedLoss(int num_packets1, uint8_t fraction_loss1,
                     int num_packets2, uint8_t fraction_loss2) {
  int weighted_sum = num_packets1 * fraction_loss1 +
      num_packets2 * fraction_loss2;
  int total_num_packets = num_packets1 + num_packets2;
  return (weighted_sum + total_num_packets / 2) / total_num_packets;
}

webrtc::RTCPReportBlock CreateReportBlock(
    uint32_t remote_ssrc, uint32_t source_ssrc,
    uint8_t fraction_lost, uint32_t extended_high_sequence_number) {
  return webrtc::RTCPReportBlock(remote_ssrc, source_ssrc, fraction_lost, 0,
                                 extended_high_sequence_number, 0, 0, 0);
}

class TestBitrateObserver: public BitrateObserver {
 public:
  TestBitrateObserver()
      : last_bitrate_(0),
        last_fraction_loss_(0),
        last_rtt_(0) {
  }

  virtual void OnNetworkChanged(uint32_t bitrate,
                                uint8_t fraction_loss,
                                int64_t rtt) {
    last_bitrate_ = static_cast<int>(bitrate);
    last_fraction_loss_ = fraction_loss;
    last_rtt_ = rtt;
  }
  int last_bitrate_;
  uint8_t last_fraction_loss_;
  int64_t last_rtt_;
};

class BitrateControllerTest : public ::testing::Test {
 protected:
  BitrateControllerTest() : clock_(0) {}
  ~BitrateControllerTest() {}

  virtual void SetUp() {
    controller_.reset(BitrateController::CreateBitrateController(
        &clock_, &bitrate_observer_, &event_log_));
    controller_->SetStartBitrate(kStartBitrateBps);
    EXPECT_EQ(kStartBitrateBps, bitrate_observer_.last_bitrate_);
    controller_->SetMinMaxBitrate(kMinBitrateBps, kMaxBitrateBps);
    EXPECT_EQ(kStartBitrateBps, bitrate_observer_.last_bitrate_);
    bandwidth_observer_ = controller_.get();
  }

  virtual void TearDown() {
  }

  const int kMinBitrateBps = 100000;
  const int kStartBitrateBps = 200000;
  const int kMaxBitrateBps = 300000;

  const int kDefaultMinBitrateBps = 10000;
  const int kDefaultMaxBitrateBps = 1000000000;

  webrtc::SimulatedClock clock_;
  TestBitrateObserver bitrate_observer_;
  std::unique_ptr<BitrateController> controller_;
  RtcpBandwidthObserver* bandwidth_observer_;
  testing::NiceMock<webrtc::MockRtcEventLog> event_log_;
};

TEST_F(BitrateControllerTest, DefaultMinMaxBitrate) {
  // Receive successively lower REMBs, verify the reserved bitrate is deducted.
  controller_->SetMinMaxBitrate(0, 0);
  EXPECT_EQ(kStartBitrateBps, bitrate_observer_.last_bitrate_);
  bandwidth_observer_->OnReceivedEstimatedBitrate(kDefaultMinBitrateBps / 2);
  EXPECT_EQ(webrtc::congestion_controller::GetMinBitrateBps(),
            bitrate_observer_.last_bitrate_);
  bandwidth_observer_->OnReceivedEstimatedBitrate(2 * kDefaultMaxBitrateBps);
  clock_.AdvanceTimeMilliseconds(1000);
  controller_->Process();
  EXPECT_EQ(kDefaultMaxBitrateBps, bitrate_observer_.last_bitrate_);
}

TEST_F(BitrateControllerTest, OneBitrateObserverOneRtcpObserver) {
  // First REMB applies immediately.
  int64_t time_ms = 1001;
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(200000, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(0, bitrate_observer_.last_rtt_);
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  report_blocks.clear();
  time_ms += 2000;

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test bitrate increase 8% per second.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(217000, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(235360, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(255189, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 81));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(276604, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 101));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(299732, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  // Reach max cap.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 121));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(300000, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(300000, bitrate_observer_.last_bitrate_);

  // Test that a low delay-based estimate limits the combined estimate.
  webrtc::DelayBasedBwe::Result result(false, 280000);
  controller_->OnDelayBasedBweResult(result);
  EXPECT_EQ(280000, bitrate_observer_.last_bitrate_);

  // Test that a low REMB limits the combined estimate.
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(250000, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000, bitrate_observer_.last_bitrate_);
}

TEST_F(BitrateControllerTest, OneBitrateObserverTwoRtcpObservers) {
  const uint32_t kSenderSsrc1 = 1;
  const uint32_t kSenderSsrc2 = 2;
  const uint32_t kMediaSsrc1 = 3;
  const uint32_t kMediaSsrc2 = 4;

  int64_t time_ms = 1;
  webrtc::ReportBlockList report_blocks;
  report_blocks = {CreateReportBlock(kSenderSsrc1, kMediaSsrc1, 0, 1)};
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  time_ms += 500;

  RtcpBandwidthObserver* second_bandwidth_observer = controller_.get();
  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 21)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);

  // Test start bitrate.
  EXPECT_EQ(200000, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(100, bitrate_observer_.last_rtt_);
  time_ms += 500;

  // Test bitrate increase 8% per second.
  report_blocks = {CreateReportBlock(kSenderSsrc1, kMediaSsrc1, 0, 21)};
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  time_ms += 500;

  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 21)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);
  EXPECT_EQ(217000, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(100, bitrate_observer_.last_rtt_);
  time_ms += 500;

  // Extra report should not change estimate.
  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 31)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);
  EXPECT_EQ(217000, bitrate_observer_.last_bitrate_);
  time_ms += 500;

  report_blocks = {CreateReportBlock(kSenderSsrc1, kMediaSsrc1, 0, 41)};
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(235360, bitrate_observer_.last_bitrate_);

  // Second report should not change estimate.
  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 41)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);
  EXPECT_EQ(235360, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  // Reports from only one bandwidth observer is ok.
  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 61)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(255189, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 81)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(276604, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 121)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(299732, bitrate_observer_.last_bitrate_);
  time_ms += 1000;

  // Reach max cap.
  report_blocks = {CreateReportBlock(kSenderSsrc2, kMediaSsrc2, 0, 141)};
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(300000, bitrate_observer_.last_bitrate_);

  // Test that a low REMB trigger immediately.
  // We don't care which bandwidth observer that delivers the REMB.
  second_bandwidth_observer->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(250000, bitrate_observer_.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);

  // Min cap.
  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000, bitrate_observer_.last_bitrate_);
}

TEST_F(BitrateControllerTest, OneBitrateObserverMultipleReportBlocks) {
  uint32_t sequence_number[2] = {0, 0xFF00};
  const int kStartBitrate = 200000;
  const int kMinBitrate = 100000;
  const int kMaxBitrate = 300000;
  controller_->SetStartBitrate(kStartBitrate);
  controller_->SetMinMaxBitrate(kMinBitrate, kMaxBitrate);

  // REMBs during the first 2 seconds apply immediately.
  int64_t time_ms = 1001;
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, sequence_number[0]));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kStartBitrate);
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  report_blocks.clear();
  time_ms += 2000;

  // Receive a high REMB, test bitrate increase.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  int last_bitrate = 0;
  // Ramp up to max bitrate.
  for (int i = 0; i < 7; ++i) {
    report_blocks.push_back(CreateReportBlock(1, 2, 0, sequence_number[0]));
    report_blocks.push_back(CreateReportBlock(1, 3, 0, sequence_number[1]));
    bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                      time_ms);
    EXPECT_GT(bitrate_observer_.last_bitrate_, last_bitrate);
    EXPECT_EQ(0, bitrate_observer_.last_fraction_loss_);
    EXPECT_EQ(50, bitrate_observer_.last_rtt_);
    last_bitrate = bitrate_observer_.last_bitrate_;
    time_ms += 1000;
    sequence_number[0] += 20;
    sequence_number[1] += 1;
    report_blocks.clear();
  }

  EXPECT_EQ(kMaxBitrate, bitrate_observer_.last_bitrate_);

  // Packet loss on the first stream. Verify that bitrate decreases.
  report_blocks.push_back(CreateReportBlock(1, 2, 50, sequence_number[0]));
  report_blocks.push_back(CreateReportBlock(1, 3, 0, sequence_number[1]));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_LT(bitrate_observer_.last_bitrate_, last_bitrate);
  EXPECT_EQ(WeightedLoss(20, 50, 1, 0), bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);
  last_bitrate = bitrate_observer_.last_bitrate_;
  sequence_number[0] += 20;
  sequence_number[1] += 20;
  time_ms += 1000;
  report_blocks.clear();

  // Packet loss on the second stream. Verify that bitrate decreases.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, sequence_number[0]));
  report_blocks.push_back(CreateReportBlock(1, 3, 75, sequence_number[1]));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_LT(bitrate_observer_.last_bitrate_, last_bitrate);
  EXPECT_EQ(WeightedLoss(20, 0, 20, 75), bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);
  last_bitrate = bitrate_observer_.last_bitrate_;
  sequence_number[0] += 20;
  sequence_number[1] += 1;
  time_ms += 1000;
  report_blocks.clear();

  // All packets lost on stream with few packets, no back-off.
  report_blocks.push_back(CreateReportBlock(1, 2, 1, sequence_number[0]));
  report_blocks.push_back(CreateReportBlock(1, 3, 255, sequence_number[1]));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(bitrate_observer_.last_bitrate_, last_bitrate);
  EXPECT_EQ(WeightedLoss(20, 1, 1, 255), bitrate_observer_.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_.last_rtt_);
  last_bitrate = bitrate_observer_.last_bitrate_;
  sequence_number[0] += 20;
  sequence_number[1] += 1;
  report_blocks.clear();
}

TEST_F(BitrateControllerTest, SetReservedBitrate) {
  // Receive successively lower REMBs, verify the reserved bitrate is deducted.
  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);
  EXPECT_EQ(200000, bitrate_observer_.last_bitrate_);
  controller_->SetReservedBitrate(50000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);
  EXPECT_EQ(150000, bitrate_observer_.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(200000, bitrate_observer_.last_bitrate_);
  controller_->SetReservedBitrate(50000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(150000, bitrate_observer_.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(200000, bitrate_observer_.last_bitrate_);
  controller_->SetReservedBitrate(30000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(170000, bitrate_observer_.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(160000);
  EXPECT_EQ(160000, bitrate_observer_.last_bitrate_);
  controller_->SetReservedBitrate(30000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(160000);
  EXPECT_EQ(130000, bitrate_observer_.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(120000, bitrate_observer_.last_bitrate_);
  controller_->SetReservedBitrate(10000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(110000, bitrate_observer_.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(120000, bitrate_observer_.last_bitrate_);
  controller_->SetReservedBitrate(50000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  // Limited by min bitrate.
  EXPECT_EQ(100000, bitrate_observer_.last_bitrate_);

  controller_->SetReservedBitrate(10000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(1);
  EXPECT_EQ(100000, bitrate_observer_.last_bitrate_);
}

TEST_F(BitrateControllerTest, TimeoutsWithoutFeedback) {
  {
    webrtc::test::ScopedFieldTrials override_field_trials(
        "WebRTC-FeedbackTimeout/Enabled/");
    SetUp();
    int expected_bitrate_bps = 300000;
    controller_->SetBitrates(300000, kDefaultMinBitrateBps,
                             kDefaultMaxBitrateBps);

    webrtc::ReportBlockList report_blocks;
    report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
    bandwidth_observer_->OnReceivedRtcpReceiverReport(
        report_blocks, 50, clock_.TimeInMilliseconds());
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(500);

    report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
    bandwidth_observer_->OnReceivedRtcpReceiverReport(
        report_blocks, 50, clock_.TimeInMilliseconds());
    report_blocks.clear();
    expected_bitrate_bps = expected_bitrate_bps * 1.08 + 1000;
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(1500);

    report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
    bandwidth_observer_->OnReceivedRtcpReceiverReport(
        report_blocks, 50, clock_.TimeInMilliseconds());
    expected_bitrate_bps = expected_bitrate_bps * 1.08 + 1000;
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(4000);

    // 4 seconds since feedback, expect increase.
    controller_->Process();
    expected_bitrate_bps = expected_bitrate_bps * 1.08 + 1000;
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(2000);

    // 6 seconds since feedback, expect no increase.
    controller_->Process();
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(9001);

    // More than 15 seconds since feedback, expect decrease.
    controller_->Process();
    expected_bitrate_bps *= 0.8;
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(500);

    // Only one timeout every second.
    controller_->Process();
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
    clock_.AdvanceTimeMilliseconds(501);

    // New timeout allowed.
    controller_->Process();
    expected_bitrate_bps *= 0.8;
    EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
  }
}

TEST_F(BitrateControllerTest, StopIncreaseWithoutPacketReports) {
  int expected_bitrate_bps = 300000;
  controller_->SetBitrates(300000, kDefaultMinBitrateBps,
                           kDefaultMaxBitrateBps);

  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 50, clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
  clock_.AdvanceTimeMilliseconds(500);

  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 50, clock_.TimeInMilliseconds());
  report_blocks.clear();
  expected_bitrate_bps = expected_bitrate_bps * 1.08 + 1000;
  EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
  clock_.AdvanceTimeMilliseconds(1500);

  // 1.2 seconds without packets reported as received, no increase.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 50, clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
  clock_.AdvanceTimeMilliseconds(1000);

  // 5 packets reported as received since last, too few, no increase.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 26));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 50, clock_.TimeInMilliseconds());
  report_blocks.clear();
  EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
  clock_.AdvanceTimeMilliseconds(100);

  // 15 packets reported as received since last, enough to increase.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 50, clock_.TimeInMilliseconds());
  expected_bitrate_bps = expected_bitrate_bps * 1.08 + 1000;
  EXPECT_EQ(expected_bitrate_bps, bitrate_observer_.last_bitrate_);
  clock_.AdvanceTimeMilliseconds(1000);
}
