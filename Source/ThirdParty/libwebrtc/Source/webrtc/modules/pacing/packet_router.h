/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACKET_ROUTER_H_
#define MODULES_PACING_PACKET_ROUTER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "api/transport/network_types.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RtpRtcp;

// PacketRouter keeps track of rtp send modules to support the pacer.
// In addition, it handles feedback messages, which are sent on a send
// module if possible (sender report), otherwise on receive module
// (receiver report). For the latter case, we also keep track of the
// receive modules.
class PacketRouter : public RemoteBitrateObserver,
                     public TransportFeedbackSenderInterface {
 public:
  PacketRouter();
  explicit PacketRouter(uint16_t start_transport_seq);
  ~PacketRouter() override;

  void AddSendRtpModule(RtpRtcp* rtp_module, bool remb_candidate);
  void RemoveSendRtpModule(RtpRtcp* rtp_module);

  void AddReceiveRtpModule(RtcpFeedbackSenderInterface* rtcp_sender,
                           bool remb_candidate);
  void RemoveReceiveRtpModule(RtcpFeedbackSenderInterface* rtcp_sender);

  virtual void SendPacket(std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& cluster_info);

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes);

  uint16_t CurrentTransportSequenceNumber() const;

  // Called every time there is a new bitrate estimate for a receive channel
  // group. This call will trigger a new RTCP REMB packet if the bitrate
  // estimate has decreased or if no RTCP REMB packet has been sent for
  // a certain time interval.
  // Implements RtpReceiveBitrateUpdate.
  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate_bps) override;

  // Ensures remote party notified of the receive bitrate limit no larger than
  // |bitrate_bps|.
  void SetMaxDesiredReceiveBitrate(int64_t bitrate_bps);

  // Send REMB feedback.
  bool SendRemb(int64_t bitrate_bps, const std::vector<uint32_t>& ssrcs);

  // Sends |packets| in one or more IP packets.
  bool SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets) override;

 private:
  void AddRembModuleCandidate(RtcpFeedbackSenderInterface* candidate_module,
                              bool media_sender)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(modules_crit_);
  void MaybeRemoveRembModuleCandidate(
      RtcpFeedbackSenderInterface* candidate_module,
      bool media_sender) RTC_EXCLUSIVE_LOCKS_REQUIRED(modules_crit_);
  void UnsetActiveRembModule() RTC_EXCLUSIVE_LOCKS_REQUIRED(modules_crit_);
  void DetermineActiveRembModule() RTC_EXCLUSIVE_LOCKS_REQUIRED(modules_crit_);
  void AddSendRtpModuleToMap(RtpRtcp* rtp_module, uint32_t ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(modules_crit_);
  void RemoveSendRtpModuleFromMap(uint32_t ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(modules_crit_);

  rtc::CriticalSection modules_crit_;
  // Ssrc to RtpRtcp module;
  std::unordered_map<uint32_t, RtpRtcp*> send_modules_map_
      RTC_GUARDED_BY(modules_crit_);
  std::list<RtpRtcp*> send_modules_list_ RTC_GUARDED_BY(modules_crit_);
  // The last module used to send media.
  RtpRtcp* last_send_module_ RTC_GUARDED_BY(modules_crit_);
  // Rtcp modules of the rtp receivers.
  std::vector<RtcpFeedbackSenderInterface*> rtcp_feedback_senders_
      RTC_GUARDED_BY(modules_crit_);

  // TODO(eladalon): remb_crit_ only ever held from one function, and it's not
  // clear if that function can actually be called from more than one thread.
  rtc::CriticalSection remb_crit_;
  // The last time a REMB was sent.
  int64_t last_remb_time_ms_ RTC_GUARDED_BY(remb_crit_);
  int64_t last_send_bitrate_bps_ RTC_GUARDED_BY(remb_crit_);
  // The last bitrate update.
  int64_t bitrate_bps_ RTC_GUARDED_BY(remb_crit_);
  int64_t max_bitrate_bps_ RTC_GUARDED_BY(remb_crit_);

  // Candidates for the REMB module can be RTP sender/receiver modules, with
  // the sender modules taking precedence.
  std::vector<RtcpFeedbackSenderInterface*> sender_remb_candidates_
      RTC_GUARDED_BY(modules_crit_);
  std::vector<RtcpFeedbackSenderInterface*> receiver_remb_candidates_
      RTC_GUARDED_BY(modules_crit_);
  RtcpFeedbackSenderInterface* active_remb_module_
      RTC_GUARDED_BY(modules_crit_);

  uint64_t transport_seq_ RTC_GUARDED_BY(modules_crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(PacketRouter);
};
}  // namespace webrtc
#endif  // MODULES_PACING_PACKET_ROUTER_H_
