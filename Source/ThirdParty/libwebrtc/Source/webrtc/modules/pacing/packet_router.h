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
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/race_checker.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class RtpRtcp;
namespace rtcp {
class TransportFeedback;
}  // namespace rtcp

// PacketRouter keeps track of rtp send modules to support the pacer.
// In addition, it handles feedback messages, which are sent on a send
// module if possible (sender report), otherwise on receive module
// (receiver report). For the latter case, we also keep track of the
// receive modules.
class PacketRouter : public PacedSender::PacketSender,
                     public TransportSequenceNumberAllocator,
                     public RemoteBitrateObserver {
 public:
  PacketRouter();
  ~PacketRouter() override;

  // TODO(nisse): Delete, as soon as downstream app is updated.
  RTC_DEPRECATED void AddRtpModule(RtpRtcp* rtp_module) {
    AddReceiveRtpModule(rtp_module);
  }
  RTC_DEPRECATED void RemoveRtpModule(RtpRtcp* rtp_module) {
    RemoveReceiveRtpModule(rtp_module);
  }
  void AddSendRtpModule(RtpRtcp* rtp_module);
  void RemoveSendRtpModule(RtpRtcp* rtp_module);

  void AddReceiveRtpModule(RtpRtcp* rtp_module);
  void RemoveReceiveRtpModule(RtpRtcp* rtp_module);

  // Implements PacedSender::Callback.
  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_timestamp,
                        bool retransmission,
                        const PacedPacketInfo& packet_info) override;

  size_t TimeToSendPadding(size_t bytes,
                           const PacedPacketInfo& packet_info) override;

  void SetTransportWideSequenceNumber(uint16_t sequence_number);
  uint16_t AllocateSequenceNumber() override;

  // Called every time there is a new bitrate estimate for a receive channel
  // group. This call will trigger a new RTCP REMB packet if the bitrate
  // estimate has decreased or if no RTCP REMB packet has been sent for
  // a certain time interval.
  // Implements RtpReceiveBitrateUpdate.
  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate_bps) override;

  // Send REMB feedback.
  virtual bool SendRemb(uint32_t bitrate_bps,
                        const std::vector<uint32_t>& ssrcs);

  // Send transport feedback packet to send-side.
  virtual bool SendTransportFeedback(rtcp::TransportFeedback* packet);

 private:
  rtc::RaceChecker pacer_race_;
  rtc::CriticalSection modules_crit_;
  std::list<RtpRtcp*> rtp_send_modules_ GUARDED_BY(modules_crit_);
  std::vector<RtpRtcp*> rtp_receive_modules_ GUARDED_BY(modules_crit_);

  rtc::CriticalSection remb_crit_;
  // The last time a REMB was sent.
  int64_t last_remb_time_ms_ GUARDED_BY(remb_crit_);
  uint32_t last_send_bitrate_bps_ GUARDED_BY(remb_crit_);
  // The last bitrate update.
  uint32_t bitrate_bps_ GUARDED_BY(remb_crit_);

  volatile int transport_seq_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PacketRouter);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACING_PACKET_ROUTER_H_
