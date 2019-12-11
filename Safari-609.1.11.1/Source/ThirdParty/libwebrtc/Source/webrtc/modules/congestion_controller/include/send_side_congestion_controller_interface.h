/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_INTERFACE_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_INTERFACE_H_

#include <memory>
#include <vector>

#include "modules/congestion_controller/include/network_changed_observer.h"
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/networkroute.h"

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

class SendSideCongestionControllerInterface : public CallStatsObserver,
                                              public Module,
                                              public TransportFeedbackObserver {
 public:
  SendSideCongestionControllerInterface() = default;
  ~SendSideCongestionControllerInterface() override = default;
  virtual void RegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) = 0;
  virtual void DeRegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) = 0;
  virtual void RegisterNetworkObserver(NetworkChangedObserver* observer) = 0;
  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps) = 0;
  virtual void SetAllocatedSendBitrateLimits(int64_t min_send_bitrate_bps,
                                             int64_t max_padding_bitrate_bps,
                                             int64_t max_total_bitrate_bps) = 0;
  virtual void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                                     int bitrate_bps,
                                     int min_bitrate_bps,
                                     int max_bitrate_bps) = 0;
  virtual void SignalNetworkState(NetworkState state) = 0;
  virtual RtcpBandwidthObserver* GetBandwidthObserver() = 0;
  virtual bool AvailableBandwidth(uint32_t* bandwidth) const = 0;
  virtual TransportFeedbackObserver* GetTransportFeedbackObserver() = 0;
  virtual void SetPerPacketFeedbackAvailable(bool available) = 0;
  virtual void EnablePeriodicAlrProbing(bool enable) = 0;
  virtual void OnSentPacket(const rtc::SentPacket& sent_packet) = 0;
  virtual void SetPacingFactor(float pacing_factor) = 0;
  virtual void SetAllocatedBitrateWithoutFeedback(uint32_t bitrate_bps) = 0;
  RTC_DISALLOW_COPY_AND_ASSIGN(SendSideCongestionControllerInterface);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_INTERFACE_H_
