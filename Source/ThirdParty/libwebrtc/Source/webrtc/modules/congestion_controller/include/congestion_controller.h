/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/networkroute.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "webrtc/modules/congestion_controller/include/send_side_congestion_controller.h"
#include "webrtc/modules/congestion_controller/transport_feedback_adapter.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/pacing/packet_router.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class BitrateController;
class Clock;
class ProbeController;
class RateLimiter;
class RemoteBitrateEstimator;
class RemoteBitrateObserver;
class RtcEventLog;

class CongestionController : public CallStatsObserver,
                             public Module,
                             public TransportFeedbackObserver {
 public:
  using Observer = SendSideCongestionController::Observer;

  CongestionController(const Clock* clock,
                       Observer* observer,
                       RemoteBitrateObserver* /* remote_bitrate_observer */,
                       RtcEventLog* event_log,
                       PacketRouter* packet_router)
      : send_side_cc_(clock, observer, event_log, packet_router),
        receive_side_cc_(clock, packet_router) {}
  CongestionController(const Clock* clock,
                       Observer* observer,
                       RemoteBitrateObserver* /* remote_bitrate_observer */,
                       RtcEventLog* event_log,
                       PacketRouter* packet_router,
                       std::unique_ptr<PacedSender> pacer)
      : send_side_cc_(clock, observer, event_log, std::move(pacer)),
        receive_side_cc_(clock, packet_router) {}

  virtual ~CongestionController() {}

  virtual void OnReceivedPacket(int64_t arrival_time_ms,
                                size_t payload_size,
                                const RTPHeader& header);

  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);
  // Resets both the BWE state and the bitrate estimator. Note the first
  // argument is the bitrate_bps.
  virtual void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                                     int bitrate_bps,
                                     int min_bitrate_bps,
                                     int max_bitrate_bps);
  virtual void SignalNetworkState(NetworkState state);
  virtual void SetTransportOverhead(size_t transport_overhead_bytes_per_packet);

  virtual BitrateController* GetBitrateController() const;
  virtual RemoteBitrateEstimator* GetRemoteBitrateEstimator(
      bool send_side_bwe);
  virtual int64_t GetPacerQueuingDelayMs() const;
  // TODO(nisse): Delete this accessor function. The pacer should be
  // internal to the congestion controller. Currently needed by Call,
  // to register the pacer module on the right thread.
  virtual PacedSender* pacer() { return send_side_cc_.pacer(); }
  // TODO(nisse): Delete this method, as soon as downstream projects
  // are updated.
  virtual TransportFeedbackObserver* GetTransportFeedbackObserver() {
    return this;
  }
  RateLimiter* GetRetransmissionRateLimiter();
  void EnablePeriodicAlrProbing(bool enable);

  // SetAllocatedSendBitrateLimits sets bitrates limits imposed by send codec
  // settings.
  // |min_send_bitrate_bps| is the total minimum send bitrate required by all
  // sending streams.  This is the minimum bitrate the PacedSender will use.
  // Note that CongestionController::OnNetworkChanged can still be called with
  // a lower bitrate estimate.
  // |max_padding_bitrate_bps| is the max bitrate the send streams request for
  // padding. This can be higher than the current network estimate and tells
  // the PacedSender how much it should max pad unless there is real packets to
  // send.
  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps);

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
  SendSideCongestionController send_side_cc_;
  ReceiveSideCongestionController receive_side_cc_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CongestionController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_
