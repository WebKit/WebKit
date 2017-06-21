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

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

class PacketFeedbackObserver;

namespace rtcp {
class TransportFeedback;
}  // namespace rtcp

class TransportFeedbackAdapter {
 public:
  explicit TransportFeedbackAdapter(const Clock* clock);
  virtual ~TransportFeedbackAdapter();

  void RegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);
  void DeRegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);

  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info);
  void OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  // TODO(holmer): This method should return DelayBasedBwe::Result so that we
  // can get rid of the dependency on BitrateController. Requires changes
  // to the CongestionController interface.
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback);
  std::vector<PacketFeedback> GetTransportFeedbackVector() const;

  void SetTransportOverhead(int transport_overhead_bytes_per_packet);

  void SetNetworkIds(uint16_t local_id, uint16_t remote_id);

 private:
  std::vector<PacketFeedback> GetPacketFeedbackVector(
      const rtcp::TransportFeedback& feedback);

  const bool send_side_bwe_with_overhead_;
  rtc::CriticalSection lock_;
  int transport_overhead_bytes_per_packet_ GUARDED_BY(&lock_);
  SendTimeHistory send_time_history_ GUARDED_BY(&lock_);
  const Clock* const clock_;
  int64_t current_offset_ms_;
  int64_t last_timestamp_us_;
  std::vector<PacketFeedback> last_packet_feedback_vector_;
  uint16_t local_net_id_ GUARDED_BY(&lock_);
  uint16_t remote_net_id_ GUARDED_BY(&lock_);

  rtc::CriticalSection observers_lock_;
  std::vector<PacketFeedbackObserver*> observers_ GUARDED_BY(&observers_lock_);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_TRANSPORT_FEEDBACK_ADAPTER_H_
