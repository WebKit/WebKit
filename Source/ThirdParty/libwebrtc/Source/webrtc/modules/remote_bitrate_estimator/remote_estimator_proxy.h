/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_ESTIMATOR_PROXY_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_ESTIMATOR_PROXY_H_

#include <map>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

namespace webrtc {

class Clock;
class PacketRouter;
namespace rtcp {
class TransportFeedback;
}

// Class used when send-side BWE is enabled: This proxy is instantiated on the
// receive side. It buffers a number of receive timestamps and then sends
// transport feedback messages back too the send side.

class RemoteEstimatorProxy : public RemoteBitrateEstimator {
 public:
  RemoteEstimatorProxy(const Clock* clock, PacketRouter* packet_router);
  virtual ~RemoteEstimatorProxy();

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override;
  void RemoveStream(uint32_t ssrc) override {}
  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override {}
  void SetMinBitrate(int min_bitrate_bps) override {}
  int64_t TimeUntilNextProcess() override;
  void Process() override;
  void OnBitrateChanged(int bitrate);

  static const int kMinSendIntervalMs;
  static const int kMaxSendIntervalMs;
  static const int kDefaultSendIntervalMs;
  static const int kBackWindowMs;

 private:
  void OnPacketArrival(uint16_t sequence_number, int64_t arrival_time)
      EXCLUSIVE_LOCKS_REQUIRED(&lock_);
  bool BuildFeedbackPacket(rtcp::TransportFeedback* feedback_packet);

  const Clock* const clock_;
  PacketRouter* const packet_router_;
  int64_t last_process_time_ms_;

  rtc::CriticalSection lock_;

  uint32_t media_ssrc_ GUARDED_BY(&lock_);
  uint8_t feedback_sequence_ GUARDED_BY(&lock_);
  SequenceNumberUnwrapper unwrapper_ GUARDED_BY(&lock_);
  int64_t window_start_seq_ GUARDED_BY(&lock_);
  // Map unwrapped seq -> time.
  std::map<int64_t, int64_t> packet_arrival_times_ GUARDED_BY(&lock_);
  int64_t send_interval_ms_ GUARDED_BY(&lock_);
};

}  // namespace webrtc

#endif  //  WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_ESTIMATOR_PROXY_H_
