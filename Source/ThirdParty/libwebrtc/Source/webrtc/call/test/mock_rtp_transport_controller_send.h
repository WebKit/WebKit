/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/bitrate_constraints.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/congestion_controller/include/network_changed_observer.h"
#include "modules/pacing/packet_router.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/socket.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtpTransportControllerSend
    : public RtpTransportControllerSendInterface {
 public:
  MOCK_METHOD9(
      CreateRtpVideoSender,
      RtpVideoSenderInterface*(const std::vector<uint32_t>&,
                               std::map<uint32_t, RtpState>,
                               const std::map<uint32_t, RtpPayloadState>&,
                               const RtpConfig&,
                               const RtcpConfig&,
                               Transport*,
                               const RtpSenderObservers&,
                               RtcEventLog*,
                               std::unique_ptr<FecController>));
  MOCK_METHOD1(DestroyRtpVideoSender, void(RtpVideoSenderInterface*));
  MOCK_METHOD0(GetWorkerQueue, rtc::TaskQueue*());
  MOCK_METHOD0(packet_router, PacketRouter*());
  MOCK_METHOD0(transport_feedback_observer, TransportFeedbackObserver*());
  MOCK_METHOD0(packet_sender, RtpPacketSender*());
  MOCK_CONST_METHOD0(keepalive_config, RtpKeepAliveConfig&());
  MOCK_METHOD3(SetAllocatedSendBitrateLimits, void(int, int, int));
  MOCK_METHOD1(SetPacingFactor, void(float));
  MOCK_METHOD1(SetQueueTimeLimit, void(int));
  MOCK_METHOD0(GetCallStatsObserver, CallStatsObserver*());
  MOCK_METHOD1(RegisterPacketFeedbackObserver, void(PacketFeedbackObserver*));
  MOCK_METHOD1(DeRegisterPacketFeedbackObserver, void(PacketFeedbackObserver*));
  MOCK_METHOD1(RegisterTargetTransferRateObserver,
               void(TargetTransferRateObserver*));
  MOCK_METHOD2(OnNetworkRouteChanged,
               void(const std::string&, const rtc::NetworkRoute&));
  MOCK_METHOD1(OnNetworkAvailability, void(bool));
  MOCK_METHOD0(GetBandwidthObserver, RtcpBandwidthObserver*());
  MOCK_CONST_METHOD0(GetPacerQueuingDelayMs, int64_t());
  MOCK_CONST_METHOD0(GetFirstPacketTimeMs, int64_t());
  MOCK_METHOD1(SetPerPacketFeedbackAvailable, void(bool));
  MOCK_METHOD1(EnablePeriodicAlrProbing, void(bool));
  MOCK_METHOD1(OnSentPacket, void(const rtc::SentPacket&));
  MOCK_METHOD1(SetSdpBitrateParameters, void(const BitrateConstraints&));
  MOCK_METHOD1(SetClientBitratePreferences, void(const BitrateSettings&));
  MOCK_METHOD1(SetAllocatedBitrateWithoutFeedback, void(uint32_t));
  MOCK_METHOD1(OnTransportOverheadChanged, void(size_t));
};
}  // namespace webrtc
#endif  // CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_
