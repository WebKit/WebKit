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

#include "api/crypto/crypto_options.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/transport/bitrate_settings.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/pacing/packet_router.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/rate_limiter.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtpTransportControllerSend
    : public RtpTransportControllerSendInterface {
 public:
  MOCK_METHOD10(
      CreateRtpVideoSender,
      RtpVideoSenderInterface*(std::map<uint32_t, RtpState>,
                               const std::map<uint32_t, RtpPayloadState>&,
                               const RtpConfig&,
                               int rtcp_report_interval_ms,
                               Transport*,
                               const RtpSenderObservers&,
                               RtcEventLog*,
                               std::unique_ptr<FecController>,
                               const RtpSenderFrameEncryptionConfig&,
                               rtc::scoped_refptr<FrameTransformerInterface>));
  MOCK_METHOD1(DestroyRtpVideoSender, void(RtpVideoSenderInterface*));
  MOCK_METHOD0(GetWorkerQueue, rtc::TaskQueue*());
  MOCK_METHOD0(packet_router, PacketRouter*());
  MOCK_METHOD0(network_state_estimate_observer,
               NetworkStateEstimateObserver*());
  MOCK_METHOD0(transport_feedback_observer, TransportFeedbackObserver*());
  MOCK_METHOD0(packet_sender, RtpPacketSender*());
  MOCK_METHOD1(SetAllocatedSendBitrateLimits, void(BitrateAllocationLimits));
  MOCK_METHOD1(SetPacingFactor, void(float));
  MOCK_METHOD1(SetQueueTimeLimit, void(int));
  MOCK_METHOD0(GetStreamFeedbackProvider, StreamFeedbackProvider*());
  MOCK_METHOD1(RegisterTargetTransferRateObserver,
               void(TargetTransferRateObserver*));
  MOCK_METHOD2(OnNetworkRouteChanged,
               void(const std::string&, const rtc::NetworkRoute&));
  MOCK_METHOD1(OnNetworkAvailability, void(bool));
  MOCK_METHOD0(GetBandwidthObserver, RtcpBandwidthObserver*());
  MOCK_CONST_METHOD0(GetPacerQueuingDelayMs, int64_t());
  MOCK_CONST_METHOD0(GetFirstPacketTime, absl::optional<Timestamp>());
  MOCK_METHOD1(EnablePeriodicAlrProbing, void(bool));
  MOCK_METHOD1(OnSentPacket, void(const rtc::SentPacket&));
  MOCK_METHOD1(SetSdpBitrateParameters, void(const BitrateConstraints&));
  MOCK_METHOD1(SetClientBitratePreferences, void(const BitrateSettings&));
  MOCK_METHOD1(OnTransportOverheadChanged, void(size_t));
  MOCK_METHOD1(AccountForAudioPacketsInPacedSender, void(bool));
  MOCK_METHOD0(IncludeOverheadInPacedSender, void());
  MOCK_METHOD1(OnReceivedPacket, void(const ReceivedPacket&));
};
}  // namespace webrtc
#endif  // CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_
