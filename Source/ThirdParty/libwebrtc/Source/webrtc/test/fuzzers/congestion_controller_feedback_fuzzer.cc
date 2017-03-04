/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {

class NullBitrateObserver : public CongestionController::Observer,
                            public RemoteBitrateObserver {
 public:
  ~NullBitrateObserver() override {}

  // TODO(minyue): remove this when old OnNetworkChanged is deprecated. See
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=6796
  using CongestionController::Observer::OnNetworkChanged;

  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt_ms,
                        int64_t probing_interval_ms) override {}
  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) override {}
};

void FuzzOneInput(const uint8_t* data, size_t size) {
  size_t i = 0;
  if (size < sizeof(int64_t) + sizeof(uint8_t) + sizeof(uint32_t))
    return;
  SimulatedClock clock(data[i++]);
  NullBitrateObserver observer;
  RtcEventLogNullImpl event_log;
  PacketRouter packet_router;
  CongestionController cc(&clock, &observer, &observer, &event_log,
                          &packet_router);
  RemoteBitrateEstimator* rbe = cc.GetRemoteBitrateEstimator(true);
  RTPHeader header;
  header.ssrc = ByteReader<uint32_t>::ReadBigEndian(&data[i]);
  i += sizeof(uint32_t);
  header.extension.hasTransportSequenceNumber = true;
  int64_t arrival_time_ms =
      std::max<int64_t>(ByteReader<int64_t>::ReadBigEndian(&data[i]), 0);
  i += sizeof(int64_t);
  const size_t kMinPacketSize =
      sizeof(size_t) + sizeof(uint16_t) + sizeof(uint8_t);
  while (i + kMinPacketSize < size) {
    size_t payload_size = ByteReader<size_t>::ReadBigEndian(&data[i]) % 1500;
    i += sizeof(size_t);
    header.extension.transportSequenceNumber =
        ByteReader<uint16_t>::ReadBigEndian(&data[i]);
    i += sizeof(uint16_t);
    rbe->IncomingPacket(arrival_time_ms, payload_size, header);
    clock.AdvanceTimeMilliseconds(5);
    arrival_time_ms += ByteReader<uint8_t>::ReadBigEndian(&data[i]);
    arrival_time_ms += sizeof(uint8_t);
  }
  rbe->Process();
}
}  // namespace webrtc
