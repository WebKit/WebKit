/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_REMB_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_REMB_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {

class BitrateObserver;
class BitrateController;
class ReceiveStatistics;
class StreamStatistician;

namespace testing {
namespace bwe {

class RembBweSender : public BweSender {
 public:
  RembBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  virtual ~RembBweSender();

  int GetFeedbackIntervalMs() const override;
  void GiveFeedback(const FeedbackPacket& feedback) override;
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 protected:
  std::unique_ptr<BitrateController> bitrate_controller_;
  std::unique_ptr<RtcpBandwidthObserver> feedback_observer_;

 private:
  Clock* clock_;
  ::testing::NiceMock<MockRtcEventLog> event_log_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RembBweSender);
};

class RembReceiver : public BweReceiver, public RemoteBitrateObserver {
 public:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  RembReceiver(int flow_id, bool plot);
  virtual ~RembReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;
  // Implements RemoteBitrateObserver.
  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) override;

 private:
  static RTCPReportBlock BuildReportBlock(StreamStatistician* statistician);
  bool LatestEstimate(uint32_t* estimate_bps);

  std::string estimate_log_prefix_;
  bool plot_estimate_;
  SimulatedClock clock_;
  std::unique_ptr<ReceiveStatistics> recv_stats_;
  int64_t latest_estimate_bps_;
  int64_t last_feedback_ms_;
  std::unique_ptr<RemoteBitrateEstimator> estimator_;
  RTCPReportBlock latest_report_block_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RembReceiver);
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_REMB_H_
