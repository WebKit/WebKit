/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/packet_router.h"

#include <algorithm>
#include <limits>

#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace {

constexpr int kRembSendIntervalMs = 200;

}  // namespace

PacketRouter::PacketRouter()
    : last_remb_time_ms_(rtc::TimeMillis()),
      last_send_bitrate_bps_(0),
      bitrate_bps_(0),
      max_bitrate_bps_(std::numeric_limits<decltype(max_bitrate_bps_)>::max()),
      active_remb_module_(nullptr),
      transport_seq_(0) {}

PacketRouter::~PacketRouter() {
  RTC_DCHECK(rtp_send_modules_.empty());
  RTC_DCHECK(rtcp_feedback_senders_.empty());
  RTC_DCHECK(sender_remb_candidates_.empty());
  RTC_DCHECK(receiver_remb_candidates_.empty());
  RTC_DCHECK(active_remb_module_ == nullptr);
}

void PacketRouter::AddSendRtpModule(RtpRtcp* rtp_module, bool remb_candidate) {
  rtc::CritScope cs(&modules_crit_);
  RTC_DCHECK(std::find(rtp_send_modules_.begin(), rtp_send_modules_.end(),
                       rtp_module) == rtp_send_modules_.end());
  // Put modules which can use regular payload packets (over rtx) instead of
  // padding first as it's less of a waste
  if ((rtp_module->RtxSendStatus() & kRtxRedundantPayloads) > 0) {
    rtp_send_modules_.push_front(rtp_module);
  } else {
    rtp_send_modules_.push_back(rtp_module);
  }

  if (remb_candidate) {
    AddRembModuleCandidate(rtp_module, /* media_sender = */ true);
  }
}

void PacketRouter::RemoveSendRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_crit_);
  MaybeRemoveRembModuleCandidate(rtp_module, /* media_sender = */ true);
  auto it =
      std::find(rtp_send_modules_.begin(), rtp_send_modules_.end(), rtp_module);
  RTC_DCHECK(it != rtp_send_modules_.end());
  rtp_send_modules_.erase(it);
}

void PacketRouter::AddReceiveRtpModule(RtcpFeedbackSenderInterface* rtcp_sender,
                                       bool remb_candidate) {
  rtc::CritScope cs(&modules_crit_);
  RTC_DCHECK(std::find(rtcp_feedback_senders_.begin(),
                       rtcp_feedback_senders_.end(),
                       rtcp_sender) == rtcp_feedback_senders_.end());

  rtcp_feedback_senders_.push_back(rtcp_sender);

  if (remb_candidate) {
    AddRembModuleCandidate(rtcp_sender, /* media_sender = */ false);
  }
}

void PacketRouter::RemoveReceiveRtpModule(
    RtcpFeedbackSenderInterface* rtcp_sender) {
  rtc::CritScope cs(&modules_crit_);
  MaybeRemoveRembModuleCandidate(rtcp_sender, /* media_sender = */ false);
  auto it = std::find(rtcp_feedback_senders_.begin(),
                      rtcp_feedback_senders_.end(), rtcp_sender);
  RTC_DCHECK(it != rtcp_feedback_senders_.end());
  rtcp_feedback_senders_.erase(it);
}

bool PacketRouter::TimeToSendPacket(uint32_t ssrc,
                                    uint16_t sequence_number,
                                    int64_t capture_timestamp,
                                    bool retransmission,
                                    const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_RUNS_SERIALIZED(&pacer_race_);
  rtc::CritScope cs(&modules_crit_);
  for (auto* rtp_module : rtp_send_modules_) {
    if (!rtp_module->SendingMedia())
      continue;
    if (ssrc == rtp_module->SSRC() || ssrc == rtp_module->FlexfecSsrc()) {
      return rtp_module->TimeToSendPacket(ssrc, sequence_number,
                                          capture_timestamp, retransmission,
                                          pacing_info);
    }
  }
  return true;
}

size_t PacketRouter::TimeToSendPadding(size_t bytes_to_send,
                                       const PacedPacketInfo& pacing_info) {
  RTC_DCHECK_RUNS_SERIALIZED(&pacer_race_);
  size_t total_bytes_sent = 0;
  rtc::CritScope cs(&modules_crit_);
  // Rtp modules are ordered by which stream can most benefit from padding.
  for (RtpRtcp* module : rtp_send_modules_) {
    if (module->SendingMedia() && module->HasBweExtensions()) {
      size_t bytes_sent = module->TimeToSendPadding(
          bytes_to_send - total_bytes_sent, pacing_info);
      total_bytes_sent += bytes_sent;
      if (total_bytes_sent >= bytes_to_send)
        break;
    }
  }
  return total_bytes_sent;
}

void PacketRouter::SetTransportWideSequenceNumber(uint16_t sequence_number) {
  rtc::AtomicOps::ReleaseStore(&transport_seq_, sequence_number);
}

uint16_t PacketRouter::AllocateSequenceNumber() {
  int prev_seq = rtc::AtomicOps::AcquireLoad(&transport_seq_);
  int desired_prev_seq;
  int new_seq;
  do {
    desired_prev_seq = prev_seq;
    new_seq = (desired_prev_seq + 1) & 0xFFFF;
    // Note: CompareAndSwap returns the actual value of transport_seq at the
    // time the CAS operation was executed. Thus, if prev_seq is returned, the
    // operation was successful - otherwise we need to retry. Saving the
    // return value saves us a load on retry.
    prev_seq = rtc::AtomicOps::CompareAndSwap(&transport_seq_, desired_prev_seq,
                                              new_seq);
  } while (prev_seq != desired_prev_seq);

  return new_seq;
}

void PacketRouter::OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                           uint32_t bitrate_bps) {
  // % threshold for if we should send a new REMB asap.
  const int64_t kSendThresholdPercent = 97;
  // TODO(danilchap): Remove receive_bitrate_bps variable and the cast
  // when OnReceiveBitrateChanged takes bitrate as int64_t.
  int64_t receive_bitrate_bps = static_cast<int64_t>(bitrate_bps);

  int64_t now_ms = rtc::TimeMillis();
  {
    rtc::CritScope lock(&remb_crit_);

    // If we already have an estimate, check if the new total estimate is below
    // kSendThresholdPercent of the previous estimate.
    if (last_send_bitrate_bps_ > 0) {
      int64_t new_remb_bitrate_bps =
          last_send_bitrate_bps_ - bitrate_bps_ + receive_bitrate_bps;

      if (new_remb_bitrate_bps <
          kSendThresholdPercent * last_send_bitrate_bps_ / 100) {
        // The new bitrate estimate is less than kSendThresholdPercent % of the
        // last report. Send a REMB asap.
        last_remb_time_ms_ = now_ms - kRembSendIntervalMs;
      }
    }
    bitrate_bps_ = receive_bitrate_bps;

    if (now_ms - last_remb_time_ms_ < kRembSendIntervalMs) {
      return;
    }
    // NOTE: Updated if we intend to send the data; we might not have
    // a module to actually send it.
    last_remb_time_ms_ = now_ms;
    last_send_bitrate_bps_ = receive_bitrate_bps;
    // Cap the value to send in remb with configured value.
    receive_bitrate_bps = std::min(receive_bitrate_bps, max_bitrate_bps_);
  }
  SendRemb(receive_bitrate_bps, ssrcs);
}

void PacketRouter::SetMaxDesiredReceiveBitrate(int64_t bitrate_bps) {
  RTC_DCHECK_GE(bitrate_bps, 0);
  {
    rtc::CritScope lock(&remb_crit_);
    max_bitrate_bps_ = bitrate_bps;
    if (rtc::TimeMillis() - last_remb_time_ms_ < kRembSendIntervalMs &&
        last_send_bitrate_bps_ > 0 &&
        last_send_bitrate_bps_ <= max_bitrate_bps_) {
      // Recent measured bitrate is already below the cap.
      return;
    }
  }
  SendRemb(bitrate_bps, /*ssrcs=*/{});
}

bool PacketRouter::SendRemb(int64_t bitrate_bps,
                            const std::vector<uint32_t>& ssrcs) {
  rtc::CritScope lock(&modules_crit_);

  if (!active_remb_module_) {
    return false;
  }

  // The Add* and Remove* methods above ensure that REMB is disabled on all
  // other modules, because otherwise, they will send REMB with stale info.
  active_remb_module_->SetRemb(bitrate_bps, ssrcs);

  return true;
}

bool PacketRouter::SendTransportFeedback(rtcp::TransportFeedback* packet) {
  RTC_DCHECK_RUNS_SERIALIZED(&pacer_race_);
  rtc::CritScope cs(&modules_crit_);
  // Prefer send modules.
  for (auto* rtp_module : rtp_send_modules_) {
    packet->SetSenderSsrc(rtp_module->SSRC());
    if (rtp_module->SendFeedbackPacket(*packet))
      return true;
  }
  for (auto* rtcp_sender : rtcp_feedback_senders_) {
    packet->SetSenderSsrc(rtcp_sender->SSRC());
    if (rtcp_sender->SendFeedbackPacket(*packet))
      return true;
  }
  return false;
}

void PacketRouter::AddRembModuleCandidate(
    RtcpFeedbackSenderInterface* candidate_module,
    bool media_sender) {
  RTC_DCHECK(candidate_module);
  std::vector<RtcpFeedbackSenderInterface*>& candidates =
      media_sender ? sender_remb_candidates_ : receiver_remb_candidates_;
  RTC_DCHECK(std::find(candidates.cbegin(), candidates.cend(),
                       candidate_module) == candidates.cend());
  candidates.push_back(candidate_module);
  DetermineActiveRembModule();
}

void PacketRouter::MaybeRemoveRembModuleCandidate(
    RtcpFeedbackSenderInterface* candidate_module,
    bool media_sender) {
  RTC_DCHECK(candidate_module);
  std::vector<RtcpFeedbackSenderInterface*>& candidates =
      media_sender ? sender_remb_candidates_ : receiver_remb_candidates_;
  auto it = std::find(candidates.begin(), candidates.end(), candidate_module);

  if (it == candidates.end()) {
    return;  // Function called due to removal of non-REMB-candidate module.
  }

  if (*it == active_remb_module_) {
    UnsetActiveRembModule();
  }
  candidates.erase(it);
  DetermineActiveRembModule();
}

void PacketRouter::UnsetActiveRembModule() {
  RTC_CHECK(active_remb_module_);
  active_remb_module_->UnsetRemb();
  active_remb_module_ = nullptr;
}

void PacketRouter::DetermineActiveRembModule() {
  // Sender modules take precedence over receiver modules, because SRs (sender
  // reports) are sent more frequently than RR (receiver reports).
  // When adding the first sender module, we should change the active REMB
  // module to be that. Otherwise, we remain with the current active module.

  RtcpFeedbackSenderInterface* new_active_remb_module;

  if (!sender_remb_candidates_.empty()) {
    new_active_remb_module = sender_remb_candidates_.front();
  } else if (!receiver_remb_candidates_.empty()) {
    new_active_remb_module = receiver_remb_candidates_.front();
  } else {
    new_active_remb_module = nullptr;
  }

  if (new_active_remb_module != active_remb_module_ && active_remb_module_) {
    UnsetActiveRembModule();
  }

  active_remb_module_ = new_active_remb_module;
}

}  // namespace webrtc
