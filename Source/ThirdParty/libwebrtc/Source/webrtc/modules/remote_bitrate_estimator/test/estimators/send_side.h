/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_SEND_SIDE_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_SEND_SIDE_H_

#include <memory>
#include <vector>

#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/congestion_controller/acknowledge_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {

class SendSideBweSender : public BweSender, public RemoteBitrateObserver {
 public:
  SendSideBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  virtual ~SendSideBweSender();

  int GetFeedbackIntervalMs() const override;
  void GiveFeedback(const FeedbackPacket& feedback) override;
  void OnPacketsSent(const Packets& packets) override;
  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) override;
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 protected:
  std::unique_ptr<BitrateController> bitrate_controller_;
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator_;
  std::unique_ptr<DelayBasedBwe> bwe_;
  std::unique_ptr<RtcpBandwidthObserver> feedback_observer_;

 private:
  Clock* const clock_;
  RTCPReportBlock report_block_;
  SendTimeHistory send_time_history_;
  bool has_received_ack_;
  uint16_t last_acked_seq_num_;
  int64_t last_log_time_ms_;
  SequenceNumberUnwrapper seq_num_unwrapper_;
  ::testing::NiceMock<MockRtcEventLog> event_log_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendSideBweSender);
};

class SendSideBweReceiver : public BweReceiver {
 public:
  explicit SendSideBweReceiver(int flow_id);
  virtual ~SendSideBweReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;

 private:
  int64_t last_feedback_ms_;
  std::vector<PacketFeedback> packet_feedback_vector_;
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_SEND_SIDE_H_
