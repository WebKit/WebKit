/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_
#define WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/call/flexfec_receive_stream.h"

namespace webrtc {

class FlexfecReceiver;
class ProcessThread;
class ReceiveStatistics;
class RecoveredPacketReceiver;
class RtcpRttStats;
class RtpPacketReceived;
class RtpRtcp;

class FlexfecReceiveStreamImpl : public FlexfecReceiveStream {
 public:
  FlexfecReceiveStreamImpl(const Config& config,
                           RecoveredPacketReceiver* recovered_packet_receiver,
                           RtcpRttStats* rtt_stats,
                           ProcessThread* process_thread);
  ~FlexfecReceiveStreamImpl() override;

  const Config& GetConfig() const { return config_; }

  // TODO(nisse): Intended to be part of an RtpPacketReceiver interface.
  void OnRtpPacket(const RtpPacketReceived& packet);

  // Implements FlexfecReceiveStream.
  void Start() override;
  void Stop() override;
  Stats GetStats() const override;

 private:
  // Config.
  const Config config_;
  bool started_ GUARDED_BY(crit_);
  rtc::CriticalSection crit_;

  // Erasure code interfacing.
  const std::unique_ptr<FlexfecReceiver> receiver_;

  // RTCP reporting.
  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  const std::unique_ptr<RtpRtcp> rtp_rtcp_;
  ProcessThread* process_thread_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_
