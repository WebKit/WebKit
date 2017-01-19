/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_PACKET_ROUTER_H_
#define WEBRTC_MODULES_PACING_PACKET_ROUTER_H_

#include <list>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtpRtcp;
namespace rtcp {
class TransportFeedback;
}  // namespace rtcp

// PacketRouter routes outgoing data to the correct sending RTP module, based
// on the simulcast layer in RTPVideoHeader.
class PacketRouter : public PacedSender::PacketSender,
                     public TransportSequenceNumberAllocator {
 public:
  PacketRouter();
  virtual ~PacketRouter();

  void AddRtpModule(RtpRtcp* rtp_module);
  void RemoveRtpModule(RtpRtcp* rtp_module);

  // Implements PacedSender::Callback.
  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_timestamp,
                        bool retransmission,
                        int probe_cluster_id) override;

  size_t TimeToSendPadding(size_t bytes, int probe_cluster_id) override;

  void SetTransportWideSequenceNumber(uint16_t sequence_number);
  uint16_t AllocateSequenceNumber() override;

  // Send transport feedback packet to send-side.
  virtual bool SendFeedback(rtcp::TransportFeedback* packet);

 private:
  rtc::ThreadChecker pacer_thread_checker_;
  rtc::CriticalSection modules_crit_;
  std::list<RtpRtcp*> rtp_modules_ GUARDED_BY(modules_crit_);

  volatile int transport_seq_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PacketRouter);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACING_PACKET_ROUTER_H_
