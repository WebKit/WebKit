/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_ADAPTER_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_ADAPTER_H_

#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"

namespace webrtc {

class BitrateController;
class RtcEventLog;
class ProcessThread;

class TransportFeedbackAdapter : public TransportFeedbackObserver,
                                 public CallStatsObserver {
 public:
  TransportFeedbackAdapter(RtcEventLog* event_log,
                           Clock* clock,
                           BitrateController* bitrate_controller);
  virtual ~TransportFeedbackAdapter();

  void InitBwe();
  // Implements TransportFeedbackObserver.
  void AddPacket(uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  // TODO(holmer): This method should return DelayBasedBwe::Result so that we
  // can get rid of the dependency on BitrateController. Requires changes
  // to the CongestionController interface.
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
  std::vector<PacketInfo> GetTransportFeedbackVector() const override;

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  void SetStartBitrate(int start_bitrate_bps);
  void SetMinBitrate(int min_bitrate_bps);
  void SetTransportOverhead(int transport_overhead_bytes_per_packet);

  int64_t GetProbingIntervalMs() const;

 private:
  std::vector<PacketInfo> GetPacketFeedbackVector(
      const rtcp::TransportFeedback& feedback);

  const bool send_side_bwe_with_overhead_;
  rtc::CriticalSection lock_;
  rtc::CriticalSection bwe_lock_;
  int transport_overhead_bytes_per_packet_ GUARDED_BY(&lock_);
  SendTimeHistory send_time_history_ GUARDED_BY(&lock_);
  std::unique_ptr<DelayBasedBwe> delay_based_bwe_ GUARDED_BY(&bwe_lock_);
  RtcEventLog* const event_log_;
  Clock* const clock_;
  int64_t current_offset_ms_;
  int64_t last_timestamp_us_;
  BitrateController* const bitrate_controller_;
  std::vector<PacketInfo> last_packet_feedback_vector_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_ADAPTER_H_
