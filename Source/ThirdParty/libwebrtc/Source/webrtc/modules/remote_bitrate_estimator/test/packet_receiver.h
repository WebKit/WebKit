/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_RECEIVER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_RECEIVER_H_

#include <memory>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/test/metric_recorder.h"

namespace webrtc {
namespace testing {
namespace bwe {

class PacketReceiver : public PacketProcessor {
 public:
  PacketReceiver(PacketProcessorListener* listener,
                 int flow_id,
                 BandwidthEstimatorType bwe_type,
                 bool plot_delay,
                 bool plot_bwe);
  PacketReceiver(PacketProcessorListener* listener,
                 int flow_id,
                 BandwidthEstimatorType bwe_type,
                 bool plot_delay,
                 bool plot_bwe,
                 MetricRecorder* metric_recorder);
  ~PacketReceiver();

  // Implements PacketProcessor.
  void RunFor(int64_t time_ms, Packets* in_out) override;

  void LogStats();

  Stats<double> GetDelayStats() const;

  float GlobalPacketLoss();

 protected:
  void UpdateMetrics(int64_t arrival_time_ms,
                     int64_t send_time_ms,
                     size_t payload_size);

  Stats<double> delay_stats_;
  std::unique_ptr<BweReceiver> bwe_receiver_;

 private:
  void PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms);
  MetricRecorder* metric_recorder_;
  bool plot_delay_;  // Used in case there isn't a metric recorder.
  int64_t last_delay_plot_ms_;
  std::string delay_prefix_;
  BandwidthEstimatorType bwe_type_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PacketReceiver);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_RECEIVER_H_
