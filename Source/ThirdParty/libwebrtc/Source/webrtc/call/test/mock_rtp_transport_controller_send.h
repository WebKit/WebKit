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
  MOCK_METHOD(RtpVideoSenderInterface*,
              CreateRtpVideoSender,
              ((std::map<uint32_t, RtpState>),
               (const std::map<uint32_t, RtpPayloadState>&),
               const RtpConfig&,
               int rtcp_report_interval_ms,
               Transport*,
               const RtpSenderObservers&,
               RtcEventLog*,
               std::unique_ptr<FecController>,
               const RtpSenderFrameEncryptionConfig&,
               rtc::scoped_refptr<FrameTransformerInterface>),
              (override));
  MOCK_METHOD(void,
              DestroyRtpVideoSender,
              (RtpVideoSenderInterface*),
              (override));
  MOCK_METHOD(rtc::TaskQueue*, GetWorkerQueue, (), (override));
  MOCK_METHOD(PacketRouter*, packet_router, (), (override));
  MOCK_METHOD(NetworkStateEstimateObserver*,
              network_state_estimate_observer,
              (),
              (override));
  MOCK_METHOD(TransportFeedbackObserver*,
              transport_feedback_observer,
              (),
              (override));
  MOCK_METHOD(RtpPacketSender*, packet_sender, (), (override));
  MOCK_METHOD(void,
              SetAllocatedSendBitrateLimits,
              (BitrateAllocationLimits),
              (override));
  MOCK_METHOD(void, SetPacingFactor, (float), (override));
  MOCK_METHOD(void, SetQueueTimeLimit, (int), (override));
  MOCK_METHOD(StreamFeedbackProvider*,
              GetStreamFeedbackProvider,
              (),
              (override));
  MOCK_METHOD(void,
              RegisterTargetTransferRateObserver,
              (TargetTransferRateObserver*),
              (override));
  MOCK_METHOD(void,
              OnNetworkRouteChanged,
              (const std::string&, const rtc::NetworkRoute&),
              (override));
  MOCK_METHOD(void, OnNetworkAvailability, (bool), (override));
  MOCK_METHOD(RtcpBandwidthObserver*, GetBandwidthObserver, (), (override));
  MOCK_METHOD(int64_t, GetPacerQueuingDelayMs, (), (const, override));
  MOCK_METHOD(absl::optional<Timestamp>,
              GetFirstPacketTime,
              (),
              (const, override));
  MOCK_METHOD(void, EnablePeriodicAlrProbing, (bool), (override));
  MOCK_METHOD(void, OnSentPacket, (const rtc::SentPacket&), (override));
  MOCK_METHOD(void,
              SetSdpBitrateParameters,
              (const BitrateConstraints&),
              (override));
  MOCK_METHOD(void,
              SetClientBitratePreferences,
              (const BitrateSettings&),
              (override));
  MOCK_METHOD(void, OnTransportOverheadChanged, (size_t), (override));
  MOCK_METHOD(void, AccountForAudioPacketsInPacedSender, (bool), (override));
  MOCK_METHOD(void, IncludeOverheadInPacedSender, (), (override));
  MOCK_METHOD(void, OnReceivedPacket, (const ReceivedPacket&), (override));
  MOCK_METHOD(void, EnsureStarted, (), (override));
};
}  // namespace webrtc
#endif  // CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_
