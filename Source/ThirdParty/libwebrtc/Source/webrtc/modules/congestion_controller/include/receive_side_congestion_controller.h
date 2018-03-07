/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_RECEIVE_SIDE_CONGESTION_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_RECEIVE_SIDE_CONGESTION_CONTROLLER_H_

#include <memory>
#include <vector>

#include "modules/remote_bitrate_estimator/remote_estimator_proxy.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {
class RemoteBitrateEstimator;
class RemoteBitrateObserver;

// This class represents the congestion control state for receive
// streams. For send side bandwidth estimation, this is simply
// relaying for each received RTP packet back to the sender. While for
// receive side bandwidth estimation, we do the estimation locally and
// send our results back to the sender.
class ReceiveSideCongestionController : public CallStatsObserver,
                                        public Module {
 public:
  ReceiveSideCongestionController(
      const Clock* clock,
      PacketRouter* packet_router);

  virtual ~ReceiveSideCongestionController() {}

  virtual void OnReceivedPacket(int64_t arrival_time_ms,
                                size_t payload_size,
                                const RTPHeader& header);

  // TODO(nisse): Delete these methods, design a more specific interface.
  virtual RemoteBitrateEstimator* GetRemoteBitrateEstimator(bool send_side_bwe);
  virtual const RemoteBitrateEstimator* GetRemoteBitrateEstimator(
      bool send_side_bwe) const;

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // This is send bitrate, used to control the rate of feedback messages.
  void OnBitrateChanged(int bitrate_bps);

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 private:
  class WrappingBitrateEstimator : public RemoteBitrateEstimator {
   public:
    WrappingBitrateEstimator(RemoteBitrateObserver* observer,
                             const Clock* clock);

    virtual ~WrappingBitrateEstimator() {}

    void IncomingPacket(int64_t arrival_time_ms,
                        size_t payload_size,
                        const RTPHeader& header) override;

    void Process() override;

    int64_t TimeUntilNextProcess() override;

    void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

    void RemoveStream(unsigned int ssrc) override;

    bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                        unsigned int* bitrate_bps) const override;

    void SetMinBitrate(int min_bitrate_bps) override;

   private:
    void PickEstimatorFromHeader(const RTPHeader& header)
        RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
    void PickEstimator() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
    RemoteBitrateObserver* observer_;
    const Clock* const clock_;
    rtc::CriticalSection crit_sect_;
    std::unique_ptr<RemoteBitrateEstimator> rbe_;
    bool using_absolute_send_time_;
    uint32_t packets_since_absolute_send_time_;
    int min_bitrate_bps_;

    RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
  };

  WrappingBitrateEstimator remote_bitrate_estimator_;
  RemoteEstimatorProxy remote_estimator_proxy_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_RECEIVE_SIDE_CONGESTION_CONTROLLER_H_
