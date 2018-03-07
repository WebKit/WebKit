/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/packet_receiver.h"

#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/remote_bitrate_estimator/test/bwe.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {

PacketReceiver::PacketReceiver(PacketProcessorListener* listener,
                               int flow_id,
                               BandwidthEstimatorType bwe_type,
                               bool plot_delay,
                               bool plot_bwe,
                               MetricRecorder* metric_recorder)
    : PacketProcessor(listener, flow_id, kReceiver),
      bwe_receiver_(CreateBweReceiver(bwe_type, flow_id, plot_bwe)),
      metric_recorder_(metric_recorder),
      plot_delay_(plot_delay),
      last_delay_plot_ms_(0),
      // #2 aligns the plot with the right axis.
      delay_prefix_("Delay_ms#2"),
      bwe_type_(bwe_type) {
  if (metric_recorder_ != nullptr) {
    // Setup the prefix std::strings used when logging.
    std::vector<std::string> prefixes;

    // Metric recorder plots them in separated figures,
    // alignment will take place with the #1 left axis.
    prefixes.push_back("MetricRecorderThroughput_kbps#1");
    prefixes.push_back("Sending_Estimate_kbps#1");
    prefixes.push_back("Delay_ms_#1");
    prefixes.push_back("Packet_Loss_#1");
    prefixes.push_back("Objective_function_#1");

    // Plot Total/PerFlow Available capacity together with throughputs.
    prefixes.push_back("Capacity_kbps#1");         // Total Available.
    prefixes.push_back("PerFlowCapacity_kbps#1");  // Available per flow.

    bool plot_loss = plot_delay;  // Plot loss if delay is plotted.
    metric_recorder_->SetPlotInformation(prefixes, plot_delay, plot_loss);
  }
}

PacketReceiver::PacketReceiver(PacketProcessorListener* listener,
                               int flow_id,
                               BandwidthEstimatorType bwe_type,
                               bool plot_delay,
                               bool plot_bwe)
    : PacketReceiver(listener,
                     flow_id,
                     bwe_type,
                     plot_delay,
                     plot_bwe,
                     nullptr) {
}

PacketReceiver::~PacketReceiver() {
}

void PacketReceiver::RunFor(int64_t time_ms, Packets* in_out) {
  Packets feedback;
  for (auto it = in_out->begin(); it != in_out->end();) {
    // PacketReceivers are only associated with a single stream, and therefore
    // should only process a single flow id.
    // TODO(holmer): Break this out into a Demuxer which implements both
    // PacketProcessorListener and PacketProcessor.
    BWE_TEST_LOGGING_CONTEXT("Receiver");
    if ((*it)->GetPacketType() == Packet::kMedia &&
        (*it)->flow_id() == *flow_ids().begin()) {
      BWE_TEST_LOGGING_CONTEXT(*flow_ids().begin());
      const MediaPacket* media_packet = static_cast<const MediaPacket*>(*it);
      // We're treating the send time (from previous filter) as the arrival
      // time once packet reaches the estimator.
      int64_t arrival_time_ms = media_packet->send_time_ms();
      int64_t send_time_ms = media_packet->creation_time_ms();
      delay_stats_.Push(arrival_time_ms - send_time_ms);

      if (metric_recorder_ != nullptr) {
        metric_recorder_->UpdateTimeMs(arrival_time_ms);
        UpdateMetrics(arrival_time_ms, send_time_ms,
                      media_packet->payload_size());
        metric_recorder_->PlotAllDynamics();
      } else if (plot_delay_) {
        PlotDelay(arrival_time_ms, send_time_ms);
      }

      bwe_receiver_->ReceivePacket(arrival_time_ms, *media_packet);
      FeedbackPacket* fb = bwe_receiver_->GetFeedback(arrival_time_ms);
      if (fb)
        feedback.push_back(fb);
      delete media_packet;
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  // Insert feedback packets to be sent back to the sender.
  in_out->merge(feedback, DereferencingComparator<Packet>);
}

void PacketReceiver::UpdateMetrics(int64_t arrival_time_ms,
                                   int64_t send_time_ms,
                                   size_t payload_size) {
  metric_recorder_->UpdateThroughput(bwe_receiver_->RecentKbps(), payload_size);
  metric_recorder_->UpdateDelayMs(arrival_time_ms - send_time_ms);
  metric_recorder_->UpdateLoss(bwe_receiver_->RecentPacketLossRatio());
  metric_recorder_->UpdateObjective();
}

void PacketReceiver::PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms) {
  const int64_t kDelayPlotIntervalMs = 100;
  if (arrival_time_ms >= last_delay_plot_ms_ + kDelayPlotIntervalMs) {
    BWE_TEST_LOGGING_PLOT_WITH_NAME(0, delay_prefix_, arrival_time_ms,
                                    arrival_time_ms - send_time_ms,
                                    bwe_names[bwe_type_]);
    last_delay_plot_ms_ = arrival_time_ms;
  }
}

float PacketReceiver::GlobalPacketLoss() {
  return bwe_receiver_->GlobalReceiverPacketLossRatio();
}

Stats<double> PacketReceiver::GetDelayStats() const {
  return delay_stats_;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
