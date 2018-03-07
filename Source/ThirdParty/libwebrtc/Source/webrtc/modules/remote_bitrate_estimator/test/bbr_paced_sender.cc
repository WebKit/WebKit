/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/bbr_paced_sender.h"

#include <algorithm>
#include <queue>
#include <set>
#include <vector>

#include "modules/pacing/paced_sender.h"
#include "modules/remote_bitrate_estimator/test/estimators/congestion_window.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

BbrPacedSender::BbrPacedSender(const Clock* clock,
                               PacedSender::PacketSender* packet_sender,
                               RtcEventLog* event_log)
    : clock_(clock),
      packet_sender_(packet_sender),
      estimated_bitrate_bps_(100000),
      min_send_bitrate_kbps_(0),
      pacing_bitrate_kbps_(0),
      time_last_update_us_(clock->TimeInMicroseconds()),
      time_last_update_ms_(clock->TimeInMilliseconds()),
      next_packet_send_time_(clock_->TimeInMilliseconds()),
      rounding_error_time_ms_(0.0f),
      packets_(),
      max_data_inflight_bytes_(10000),
      congestion_window_(new testing::bwe::CongestionWindow()) {}
BbrPacedSender::~BbrPacedSender() {}

void BbrPacedSender::SetEstimatedBitrateAndCongestionWindow(
    uint32_t bitrate_bps,
    bool in_probe_rtt,
    uint64_t congestion_window) {
  estimated_bitrate_bps_ = bitrate_bps;
  max_data_inflight_bytes_ = congestion_window;
}

void BbrPacedSender::SetMinBitrate(int min_send_bitrate_bps) {
  min_send_bitrate_kbps_ = min_send_bitrate_bps / 1000;
  pacing_bitrate_kbps_ =
      std::max(min_send_bitrate_kbps_, estimated_bitrate_bps_ / 1000);
}

void BbrPacedSender::InsertPacket(RtpPacketSender::Priority priority,
                                  uint32_t ssrc,
                                  uint16_t sequence_number,
                                  int64_t capture_time_ms,
                                  size_t bytes,
                                  bool retransmission) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  if (capture_time_ms < 0)
    capture_time_ms = now_ms;
  packets_.push_back(new Packet(priority, ssrc, sequence_number,
                                capture_time_ms, now_ms, bytes,
                                retransmission));
}

int64_t BbrPacedSender::TimeUntilNextProcess() {
  // Once errors absolute value hits 1 millisecond, add compensating term to
  // the |next_packet_send_time_|, so that we can send packet earlier or later,
  // depending on the error.
  rounding_error_time_ms_ = std::min(rounding_error_time_ms_, 1.0f);
  if (rounding_error_time_ms_ < -0.9f)
    rounding_error_time_ms_ = -1.0f;
  int64_t result =
      std::max<int64_t>(next_packet_send_time_ + time_last_update_ms_ -
                            clock_->TimeInMilliseconds(),
                        0);
  if (rounding_error_time_ms_ == 1.0f || rounding_error_time_ms_ == -1.0f) {
    next_packet_send_time_ -= rounding_error_time_ms_;
    result = std::max<int64_t>(next_packet_send_time_ + time_last_update_ms_ -
                                   clock_->TimeInMilliseconds(),
                               0);
    rounding_error_time_ms_ = 0;
  }
  return result;
}

void BbrPacedSender::OnBytesAcked(size_t bytes) {
  congestion_window_->AckReceived(bytes);
}

void BbrPacedSender::Process() {
  pacing_bitrate_kbps_ =
      std::max(min_send_bitrate_kbps_, estimated_bitrate_bps_ / 1000);
  // If we have nothing to send, try sending again in 1 millisecond.
  if (packets_.empty()) {
    next_packet_send_time_ = 1;
    return;
  }
  // If congestion window doesn't allow sending, try again in 1 millisecond.
  if (packets_.front()->size_in_bytes + congestion_window_->data_inflight() >
      max_data_inflight_bytes_) {
    next_packet_send_time_ = 1;
    return;
  }
  bool sent = TryToSendPacket(packets_.front());
  if (sent) {
    congestion_window_->PacketSent(packets_.front()->size_in_bytes);
    delete packets_.front();
    packets_.pop_front();
    time_last_update_ms_ = clock_->TimeInMilliseconds();
    if (!packets_.empty()) {
      // Calculate in what time we should send current packet.
      next_packet_send_time_ = (packets_.front()->size_in_bytes * 8000 +
                                estimated_bitrate_bps_ / 2) /
                               estimated_bitrate_bps_;
      // As rounding errors may happen, |rounding_error_time_ms_| could be
      // positive or negative depending on packet was sent earlier or later,
      // after it hits certain threshold we will send a packet earlier or later
      // depending on error we had so far.
      rounding_error_time_ms_ +=
          (next_packet_send_time_ - packets_.front()->size_in_bytes * 8000.0f /
                                        estimated_bitrate_bps_ * 1.0f);
    } else {
      // If sending was unsuccessful try again in 1 millisecond.
      next_packet_send_time_ = 1;
    }
  }
}

bool BbrPacedSender::TryToSendPacket(Packet* packet) {
  PacedPacketInfo pacing_info;
  return packet_sender_->TimeToSendPacket(packet->ssrc, packet->sequence_number,
                                          packet->capture_time_ms,
                                          packet->retransmission, pacing_info);
}

}  // namespace webrtc
