/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/transport/network_control.h"
#include "call/rtp_bitrate_configurator.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/rtp_video_sender.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/include/send_side_congestion_controller_interface.h"
#include "modules/pacing/packet_router.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
class Clock;
class RtcEventLog;

// TODO(nisse): When we get the underlying transports here, we should
// have one object implementing RtpTransportControllerSendInterface
// per transport, sharing the same congestion controller.
class RtpTransportControllerSend final
    : public RtpTransportControllerSendInterface,
      public NetworkChangedObserver {
 public:
  RtpTransportControllerSend(
      Clock* clock,
      RtcEventLog* event_log,
      NetworkControllerFactoryInterface* controller_factory,
      const BitrateConstraints& bitrate_config);
  ~RtpTransportControllerSend() override;

  RtpVideoSenderInterface* CreateRtpVideoSender(
      const std::vector<uint32_t>& ssrcs,
      std::map<uint32_t, RtpState> suspended_ssrcs,
      const std::map<uint32_t, RtpPayloadState>&
          states,  // move states into RtpTransportControllerSend
      const RtpConfig& rtp_config,
      const RtcpConfig& rtcp_config,
      Transport* send_transport,
      const RtpSenderObservers& observers,
      RtcEventLog* event_log) override;
  void DestroyRtpVideoSender(
      RtpVideoSenderInterface* rtp_video_sender) override;

  // Implements NetworkChangedObserver interface.
  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt_ms,
                        int64_t probing_interval_ms) override;

  // Implements RtpTransportControllerSendInterface
  rtc::TaskQueue* GetWorkerQueue() override;
  PacketRouter* packet_router() override;

  TransportFeedbackObserver* transport_feedback_observer() override;
  RtpPacketSender* packet_sender() override;
  const RtpKeepAliveConfig& keepalive_config() const override;

  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps,
                                     int max_total_bitrate_bps) override;

  void SetKeepAliveConfig(const RtpKeepAliveConfig& config);
  void SetPacingFactor(float pacing_factor) override;
  void SetQueueTimeLimit(int limit_ms) override;
  CallStatsObserver* GetCallStatsObserver() override;
  void RegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void DeRegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void RegisterTargetTransferRateObserver(
      TargetTransferRateObserver* observer) override;
  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override;
  void OnNetworkAvailability(bool network_available) override;
  RtcpBandwidthObserver* GetBandwidthObserver() override;
  int64_t GetPacerQueuingDelayMs() const override;
  int64_t GetFirstPacketTimeMs() const override;
  void SetPerPacketFeedbackAvailable(bool available) override;
  void EnablePeriodicAlrProbing(bool enable) override;
  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  void SetSdpBitrateParameters(const BitrateConstraints& constraints) override;
  void SetClientBitratePreferences(const BitrateSettings& preferences) override;

  void SetAllocatedBitrateWithoutFeedback(uint32_t bitrate_bps) override;

 private:
  const Clock* const clock_;
  PacketRouter packet_router_;
  std::vector<std::unique_ptr<RtpVideoSenderInterface>> video_rtp_senders_;
  PacedSender pacer_;
  RtpKeepAliveConfig keepalive_;
  RtpBitrateConfigurator bitrate_configurator_;
  std::map<std::string, rtc::NetworkRoute> network_routes_;
  const std::unique_ptr<ProcessThread> process_thread_;
  rtc::CriticalSection observer_crit_;
  TargetTransferRateObserver* observer_ RTC_GUARDED_BY(observer_crit_);
  std::unique_ptr<SendSideCongestionControllerInterface> send_side_cc_;
  RateLimiter retransmission_rate_limiter_;

  // TODO(perkj): |task_queue_| is supposed to replace |process_thread_|.
  // |task_queue_| is defined last to ensure all pending tasks are cancelled
  // and deleted before any other members.
  rtc::TaskQueue task_queue_;
  RTC_DISALLOW_COPY_AND_ASSIGN(RtpTransportControllerSend);
};

}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
