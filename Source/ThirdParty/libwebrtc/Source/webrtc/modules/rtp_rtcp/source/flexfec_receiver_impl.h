/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FLEXFEC_RECEIVER_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FLEXFEC_RECEIVER_IMPL_H_

#include <memory>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/call.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

class FlexfecReceiverImpl : public FlexfecReceiver {
 public:
  FlexfecReceiverImpl(uint32_t flexfec_ssrc,
                      uint32_t protected_media_ssrc,
                      RecoveredPacketReceiver* callback);
  ~FlexfecReceiverImpl();

  // Implements FlexfecReceiver.
  bool AddAndProcessReceivedPacket(const uint8_t* packet, size_t packet_length);
  FecPacketCounter GetPacketCounter() const;

 private:
  bool AddReceivedPacket(const uint8_t* packet, size_t packet_length);
  bool ProcessReceivedPackets();

  // Config.
  const uint32_t flexfec_ssrc_;
  const uint32_t protected_media_ssrc_;

  // Erasure code interfacing and callback.
  std::unique_ptr<ForwardErrorCorrection> erasure_code_;
  ForwardErrorCorrection::ReceivedPacketList received_packets_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packets_;
  RecoveredPacketReceiver* const callback_;

  // Logging and stats.
  Clock* const clock_;
  int64_t last_recovered_packet_ms_;
  FecPacketCounter packet_counter_;

  rtc::SequencedTaskChecker sequence_checker_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_FLEXFEC_RECEIVER_IMPL_H_
