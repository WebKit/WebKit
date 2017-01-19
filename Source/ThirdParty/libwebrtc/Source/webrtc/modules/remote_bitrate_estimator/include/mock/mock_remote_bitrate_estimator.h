/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_REMOTE_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_REMOTE_BITRATE_ESTIMATOR_H_

#include <vector>

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockRemoteBitrateObserver : public RemoteBitrateObserver {
 public:
  MOCK_METHOD2(OnReceiveBitrateChanged,
               void(const std::vector<uint32_t>& ssrcs, uint32_t bitrate));
};

class MockRemoteBitrateEstimator : public RemoteBitrateEstimator {
 public:
  MOCK_METHOD1(IncomingPacketFeedbackVector,
               void(const std::vector<PacketInfo>&));
  MOCK_METHOD3(IncomingPacket, void(int64_t, size_t, const RTPHeader&));
  MOCK_METHOD1(RemoveStream, void(uint32_t));
  MOCK_CONST_METHOD2(LatestEstimate, bool(std::vector<uint32_t>*, uint32_t*));

  // From CallStatsObserver;
  MOCK_METHOD2(OnRttUpdate, void(int64_t, int64_t));

  // From Module.
  MOCK_METHOD0(TimeUntilNextProcess, int64_t());
  MOCK_METHOD0(Process, void());
  MOCK_METHOD1(SetMinBitrate, void(int));
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_MOCK_MOCK_REMOTE_BITRATE_ESTIMATOR_H_
