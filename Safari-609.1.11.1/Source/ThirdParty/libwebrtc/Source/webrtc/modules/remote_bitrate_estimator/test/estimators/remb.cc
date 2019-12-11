/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "modules/remote_bitrate_estimator/test/estimators/remb.h"

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/unused.h"
#include "test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {

RembBweSender::RembBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock,
                                                     observer,
                                                     &event_log_)),
      feedback_observer_(bitrate_controller_.get()),
      clock_(clock) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetStartBitrate(1000 * kbps);
  bitrate_controller_->SetMinMaxBitrate(1000 * kMinBitrateKbps,
                                        1000 * kMaxBitrateKbps);
}

RembBweSender::~RembBweSender() {}

void RembBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const RembFeedback& remb_feedback =
      static_cast<const RembFeedback&>(feedback);
  feedback_observer_->OnReceivedEstimatedBitrate(remb_feedback.estimated_bps());
  ReportBlockList report_blocks;
  report_blocks.push_back(remb_feedback.report_block());
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

int64_t RembBweSender::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

void RembBweSender::Process() {
  bitrate_controller_->Process();
}

int RembBweSender::GetFeedbackIntervalMs() const {
  return 100;
}

RembReceiver::RembReceiver(int flow_id, bool plot)
    : BweReceiver(flow_id),
      estimate_log_prefix_(),
      plot_estimate_(plot),
      clock_(0),
      recv_stats_(ReceiveStatistics::Create(&clock_)),
      latest_estimate_bps_(-1),
      last_feedback_ms_(-1),
      estimator_(new RemoteBitrateEstimatorAbsSendTime(this, &clock_)) {
  rtc::StringBuilder ss;
  ss << "Estimate_" << flow_id_ << "#1";
  estimate_log_prefix_ = ss.str();
  // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
  estimator_->OnRttUpdate(50, 50);
  estimator_->SetMinBitrate(kRemoteBitrateEstimatorMinBitrateBps);
}

RembReceiver::~RembReceiver() {}

void RembReceiver::ReceivePacket(int64_t arrival_time_ms,
                                 const MediaPacket& media_packet) {
  recv_stats_->OnRtpPacket(media_packet.GetRtpPacket());

  latest_estimate_bps_ = -1;

  int64_t step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
  while ((clock_.TimeInMilliseconds() + step_ms) < arrival_time_ms) {
    clock_.AdvanceTimeMilliseconds(step_ms);
    estimator_->Process();
    step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
  }
  estimator_->IncomingPacket(arrival_time_ms, media_packet.payload_size(),
                             media_packet.header());
  clock_.AdvanceTimeMilliseconds(arrival_time_ms - clock_.TimeInMilliseconds());
  ASSERT_TRUE(arrival_time_ms == clock_.TimeInMilliseconds());

  // Log received packet information.
  BweReceiver::ReceivePacket(arrival_time_ms, media_packet);
}

FeedbackPacket* RembReceiver::GetFeedback(int64_t now_ms) {
  BWE_TEST_LOGGING_CONTEXT("Remb");
  uint32_t estimated_bps = 0;
  RembFeedback* feedback = NULL;
  if (LatestEstimate(&estimated_bps)) {
    auto report_blocks = recv_stats_->RtcpReportBlocks(1);
    if (!report_blocks.empty()) {
      const rtcp::ReportBlock& stat = report_blocks.front();
      latest_report_block_.fraction_lost = stat.fraction_lost();
      latest_report_block_.packets_lost = stat.cumulative_lost();
      latest_report_block_.extended_highest_sequence_number =
          stat.extended_high_seq_num();
      latest_report_block_.jitter = stat.jitter();
    }

    feedback = new RembFeedback(flow_id_, now_ms * 1000, last_feedback_ms_,
                                estimated_bps, latest_report_block_);
    last_feedback_ms_ = now_ms;

    double estimated_kbps = static_cast<double>(estimated_bps) / 1000.0;
    RTC_UNUSED(estimated_kbps);
    if (plot_estimate_) {
      BWE_TEST_LOGGING_PLOT(0, estimate_log_prefix_,
                            clock_.TimeInMilliseconds(), estimated_kbps);
    }
  }
  return feedback;
}

void RembReceiver::OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                           uint32_t bitrate) {}

bool RembReceiver::LatestEstimate(uint32_t* estimate_bps) {
  if (latest_estimate_bps_ < 0) {
    std::vector<uint32_t> ssrcs;
    uint32_t bps = 0;
    if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
      return false;
    }
    latest_estimate_bps_ = bps;
  }
  *estimate_bps = latest_estimate_bps_;
  return true;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
