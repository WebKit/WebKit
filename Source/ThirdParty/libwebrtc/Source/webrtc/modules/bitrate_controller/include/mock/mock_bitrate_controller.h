/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_BITRATE_CONTROLLER_INCLUDE_MOCK_MOCK_BITRATE_CONTROLLER_H_
#define MODULES_BITRATE_CONTROLLER_INCLUDE_MOCK_MOCK_BITRATE_CONTROLLER_H_

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockBitrateObserver : public BitrateObserver {
 public:
  MOCK_METHOD3(OnNetworkChanged,
               void(uint32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int64_t rtt_ms));
};

class MockBitrateController : public BitrateController {
 public:
  MOCK_METHOD0(CreateRtcpBandwidthObserver, RtcpBandwidthObserver*());
  MOCK_METHOD1(SetStartBitrate, void(int start_bitrate_bps));
  MOCK_METHOD2(SetMinMaxBitrate,
               void(int min_bitrate_bps, int max_bitrate_bps));
  MOCK_METHOD3(SetBitrates,
               void(int start_bitrate_bps,
                    int min_bitrate_bps,
                    int max_bitrate_bps));
  MOCK_METHOD3(ResetBitrates,
               void(int bitrate_bps, int min_bitrate_bps, int max_bitrate_bps));
  MOCK_METHOD1(UpdateDelayBasedEstimate, void(uint32_t bitrate_bps));
  MOCK_METHOD1(UpdateProbeBitrate, void(uint32_t bitrate_bps));
  MOCK_METHOD1(SetEventLog, void(RtcEventLog* event_log));
  MOCK_CONST_METHOD1(AvailableBandwidth, bool(uint32_t* bandwidth));
  MOCK_METHOD3(GetNetworkParameters,
               bool(uint32_t* bitrate, uint8_t* fraction_loss, int64_t* rtt));

  MOCK_METHOD0(Process, void());
  MOCK_METHOD0(TimeUntilNextProcess, int64_t());
};
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_BITRATE_CONTROLLER_INCLUDE_MOCK_MOCK_BITRATE_CONTROLLER_H_
