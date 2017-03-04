/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_CONGESTION_CONTROLLER_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_CONGESTION_CONTROLLER_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/socket.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {

class MockCongestionObserver : public CongestionController::Observer {
 public:
  // TODO(minyue): remove this when old OnNetworkChanged is deprecated. See
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=6796
  using CongestionController::Observer::OnNetworkChanged;

  MOCK_METHOD4(OnNetworkChanged,
               void(uint32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int64_t rtt_ms,
                    int64_t probing_interval_ms));
};

class MockCongestionController : public CongestionController {
 public:
  MockCongestionController(Clock* clock,
                           Observer* observer,
                           RemoteBitrateObserver* remote_bitrate_observer,
                           RtcEventLog* event_log,
                           PacketRouter* packet_router)
      : CongestionController(clock,
                             observer,
                             remote_bitrate_observer,
                             event_log,
                             packet_router) {}
  MOCK_METHOD3(OnReceivedPacket,
               void(int64_t arrival_time_ms,
                    size_t payload_size,
                    const RTPHeader& header));
  MOCK_METHOD3(SetBweBitrates,
               void(int min_bitrate_bps,
                    int start_bitrate_bps,
                    int max_bitrate_bps));
  MOCK_METHOD1(SignalNetworkState, void(NetworkState state));
  MOCK_CONST_METHOD0(GetBitrateController, BitrateController*());
  MOCK_METHOD1(GetRemoteBitrateEstimator,
                     RemoteBitrateEstimator*(bool send_side_bwe));
  MOCK_CONST_METHOD0(GetPacerQueuingDelayMs, int64_t());
  MOCK_METHOD0(pacer, PacedSender*());
  MOCK_METHOD0(GetTransportFeedbackObserver, TransportFeedbackObserver*());
  MOCK_METHOD3(UpdatePacerBitrate,
               void(int bitrate_kbps,
                    int max_bitrate_kbps,
                    int min_bitrate_kbps));
  MOCK_METHOD1(OnSentPacket, void(const rtc::SentPacket& sent_packet));

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(MockCongestionController);
};
}  // namespace test
}  // namespace webrtc
#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_CONGESTION_CONTROLLER_H_
