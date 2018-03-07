/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_

#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/delay_based_bwe.h"
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/paced_sender.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/race_checker.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class BitrateController;
class Clock;
class AcknowledgedBitrateEstimator;
class ProbeController;
class RateLimiter;
class RtcEventLog;

class SendSideCongestionController : public CallStatsObserver,
                                     public Module,
                                     public TransportFeedbackObserver {
 public:
  // Observer class for bitrate changes announced due to change in bandwidth
  // estimate or due to that the send pacer is full. Fraction loss and rtt is
  // also part of this callback to allow the observer to optimize its settings
  // for different types of network environments. The bitrate does not include
  // packet headers and is measured in bits per second.
  class Observer {
   public:
    virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                  uint8_t fraction_loss,  // 0 - 255.
                                  int64_t rtt_ms,
                                  int64_t probing_interval_ms) = 0;

   protected:
    virtual ~Observer() {}
  };
  SendSideCongestionController(const Clock* clock,
                               Observer* observer,
                               RtcEventLog* event_log,
                               PacedSender* pacer);
  ~SendSideCongestionController() override;

  void RegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);
  void DeRegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);

  // Currently, there can be at most one observer.
  // TODO(nisse): The RegisterNetworkObserver method is needed because we first
  // construct this object (as part of RtpTransportControllerSend), then pass a
  // reference to Call, which then registers itself as the observer. We should
  // try to break this circular chain of references, and make the observer a
  // construction time constant.
  void RegisterNetworkObserver(Observer* observer);
  void DeRegisterNetworkObserver(Observer* observer);

  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);
  // Resets the BWE state. Note the first argument is the bitrate_bps.
  virtual void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                                     int bitrate_bps,
                                     int min_bitrate_bps,
                                     int max_bitrate_bps);
  virtual void SignalNetworkState(NetworkState state);
  virtual void SetTransportOverhead(size_t transport_overhead_bytes_per_packet);

  // Deprecated: Use GetBandwidthObserver instead.
  RTC_DEPRECATED virtual BitrateController* GetBitrateController() const;

  virtual RtcpBandwidthObserver* GetBandwidthObserver() const;

  virtual bool AvailableBandwidth(uint32_t* bandwidth) const;
  virtual int64_t GetPacerQueuingDelayMs() const;
  virtual int64_t GetFirstPacketTimeMs() const;

  virtual TransportFeedbackObserver* GetTransportFeedbackObserver();

  RateLimiter* GetRetransmissionRateLimiter();
  void EnablePeriodicAlrProbing(bool enable);

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet);

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Implements TransportFeedbackObserver.
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
  std::vector<PacketFeedback> GetTransportFeedbackVector() const override;

 private:
  void MaybeTriggerOnNetworkChanged();

  bool IsSendQueueFull() const;
  bool IsNetworkDown() const;
  bool HasNetworkParametersToReportChanged(uint32_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  void LimitOutstandingBytes(size_t num_outstanding_bytes);
  const Clock* const clock_;
  rtc::CriticalSection observer_lock_;
  Observer* observer_ RTC_GUARDED_BY(observer_lock_);
  RtcEventLog* const event_log_;
  PacedSender* const pacer_;
  const std::unique_ptr<BitrateController> bitrate_controller_;
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator_;
  const std::unique_ptr<ProbeController> probe_controller_;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;
  TransportFeedbackAdapter transport_feedback_adapter_;
  rtc::CriticalSection network_state_lock_;
  uint32_t last_reported_bitrate_bps_ RTC_GUARDED_BY(network_state_lock_);
  uint8_t last_reported_fraction_loss_ RTC_GUARDED_BY(network_state_lock_);
  int64_t last_reported_rtt_ RTC_GUARDED_BY(network_state_lock_);
  NetworkState network_state_ RTC_GUARDED_BY(network_state_lock_);
  bool pause_pacer_ RTC_GUARDED_BY(network_state_lock_);
  // Duplicate the pacer paused state to avoid grabbing a lock when
  // pausing the pacer. This can be removed when we move this class
  // over to the task queue.
  bool pacer_paused_;
  rtc::CriticalSection bwe_lock_;
  int min_bitrate_bps_ RTC_GUARDED_BY(bwe_lock_);
  std::unique_ptr<DelayBasedBwe> delay_based_bwe_ RTC_GUARDED_BY(bwe_lock_);
  bool in_cwnd_experiment_;
  int64_t accepted_queue_ms_;
  bool was_in_alr_;

  rtc::RaceChecker worker_race_;

  bool pacer_pushback_experiment_ = false;
  float encoding_rate_ = 1.0;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendSideCongestionController);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
