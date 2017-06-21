/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/packet_router.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace webrtc {

PacketRouter::PacketRouter()
    : last_remb_time_ms_(rtc::TimeMillis()),
      last_send_bitrate_bps_(0),
      transport_seq_(0) {}

PacketRouter::~PacketRouter() {
  RTC_DCHECK(rtp_send_modules_.empty());
  RTC_DCHECK(rtp_receive_modules_.empty());
}

void PacketRouter::AddSendRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_crit_);
  RTC_DCHECK(std::find(rtp_send_modules_.begin(), rtp_send_modules_.end(),
                       rtp_module) == rtp_send_modules_.end());
  if (rtp_send_modules_.empty() && !rtp_receive_modules_.empty()) {
    rtp_receive_modules_.front()->SetREMBStatus(false);
  }

  // Put modules which can use regular payload packets (over rtx) instead of
  // padding first as it's less of a waste
  if ((rtp_module->RtxSendStatus() & kRtxRedundantPayloads) > 0) {
    if (!rtp_send_modules_.empty()) {
      rtp_send_modules_.front()->SetREMBStatus(false);
    }
    rtp_send_modules_.push_front(rtp_module);
    rtp_module->SetREMBStatus(true);
  } else {
    if (rtp_send_modules_.empty()) {
      rtp_module->SetREMBStatus(true);
    }

    rtp_send_modules_.push_back(rtp_module);
  }
}

void PacketRouter::RemoveSendRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_crit_);
  RTC_DCHECK(std::find(rtp_send_modules_.begin(), rtp_send_modules_.end(),
                       rtp_module) != rtp_send_modules_.end());
  rtp_send_modules_.remove(rtp_module);
  rtp_module->SetREMBStatus(false);
  if (!rtp_send_modules_.empty()) {
    rtp_send_modules_.front()->SetREMBStatus(true);
  } else if (!rtp_receive_modules_.empty()) {
    rtp_receive_modules_.front()->SetREMBStatus(true);
  }
}

void PacketRouter::AddReceiveRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_crit_);
  RTC_DCHECK(std::find(rtp_receive_modules_.begin(), rtp_receive_modules_.end(),
                       rtp_module) == rtp_receive_modules_.end());
  if (rtp_send_modules_.empty() && rtp_receive_modules_.empty()) {
    rtp_module->SetREMBStatus(true);
  }
  rtp_receive_modules_.push_back(rtp_module);
}

void PacketRouter::RemoveReceiveRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_crit_);
  const auto& it = std::find(rtp_receive_modules_.begin(),
                             rtp_receive_modules_.end(), rtp_module);
  RTC_DCHECK(it != rtp_receive_modules_.end());
  rtp_receive_modules_.erase(it);
  if (rtp_send_modules_.empty()) {
    rtp_module->SetREMBStatus(false);
    if (!rtp_receive_modules_.empty()) {
      rtp_receive_modules_.front()->SetREMBStatus(true);
    }
  }
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
  const int kRembSendIntervalMs = 200;

  // % threshold for if we should send a new REMB asap.
  const uint32_t kSendThresholdPercent = 97;

  int64_t now_ms = rtc::TimeMillis();
  {
    rtc::CritScope lock(&remb_crit_);

    // If we already have an estimate, check if the new total estimate is below
    // kSendThresholdPercent of the previous estimate.
    if (last_send_bitrate_bps_ > 0) {
      uint32_t new_remb_bitrate_bps =
          last_send_bitrate_bps_ - bitrate_bps_ + bitrate_bps;

      if (new_remb_bitrate_bps <
          kSendThresholdPercent * last_send_bitrate_bps_ / 100) {
        // The new bitrate estimate is less than kSendThresholdPercent % of the
        // last report. Send a REMB asap.
        last_remb_time_ms_ = now_ms - kRembSendIntervalMs;
      }
    }
    bitrate_bps_ = bitrate_bps;

    if (now_ms - last_remb_time_ms_ < kRembSendIntervalMs) {
      return;
    }
    // NOTE: Updated if we intend to send the data; we might not have
    // a module to actually send it.
    last_remb_time_ms_ = now_ms;
    last_send_bitrate_bps_ = bitrate_bps;
  }
  SendRemb(bitrate_bps, ssrcs);
}

bool PacketRouter::SendRemb(uint32_t bitrate_bps,
                            const std::vector<uint32_t>& ssrcs) {
  rtc::CritScope lock(&modules_crit_);
  RtpRtcp* remb_module;
  if (!rtp_send_modules_.empty())
    remb_module = rtp_send_modules_.front();
  else if (!rtp_receive_modules_.empty())
    remb_module = rtp_receive_modules_.front();
  else
    return false;
  // The Add* and Remove* methods above ensure that this (and only this) module
  // has REMB enabled. REMB should be disabled on all other modules, because
  // otherwise, they will send REMB with stale info.
  RTC_DCHECK(remb_module->REMB());
  remb_module->SetREMBData(bitrate_bps, ssrcs);
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
  for (auto* rtp_module : rtp_receive_modules_) {
    packet->SetSenderSsrc(rtp_module->SSRC());
    if (rtp_module->SendFeedbackPacket(*packet))
      return true;
  }
  return false;
}

}  // namespace webrtc
