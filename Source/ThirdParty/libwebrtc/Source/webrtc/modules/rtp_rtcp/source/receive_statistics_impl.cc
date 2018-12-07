/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/receive_statistics_impl.h"

#include <math.h>
#include <cstdlib>
#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

const int64_t kStatisticsTimeoutMs = 8000;
const int64_t kStatisticsProcessIntervalMs = 1000;

StreamStatistician::~StreamStatistician() {}

StreamStatisticianImpl::StreamStatisticianImpl(
    uint32_t ssrc,
    Clock* clock,
    bool enable_retransmit_detection,
    int max_reordering_threshold,
    RtcpStatisticsCallback* rtcp_callback,
    StreamDataCountersCallback* rtp_callback)
    : ssrc_(ssrc),
      clock_(clock),
      incoming_bitrate_(kStatisticsProcessIntervalMs,
                        RateStatistics::kBpsScale),
      max_reordering_threshold_(max_reordering_threshold),
      enable_retransmit_detection_(enable_retransmit_detection),
      jitter_q4_(0),
      cumulative_loss_(0),
      last_receive_time_ms_(0),
      last_received_timestamp_(0),
      received_seq_first_(0),
      received_seq_max_(0),
      received_seq_wraps_(0),
      last_report_inorder_packets_(0),
      last_report_old_packets_(0),
      last_report_seq_max_(0),
      rtcp_callback_(rtcp_callback),
      rtp_callback_(rtp_callback) {}

StreamStatisticianImpl::~StreamStatisticianImpl() = default;

void StreamStatisticianImpl::OnRtpPacket(const RtpPacketReceived& packet) {
  StreamDataCounters counters = UpdateCounters(packet);
  if (rtp_callback_)
    rtp_callback_->DataCountersUpdated(counters, ssrc_);
}

StreamDataCounters StreamStatisticianImpl::UpdateCounters(
    const RtpPacketReceived& packet) {
  rtc::CritScope cs(&stream_lock_);
  RTC_DCHECK_EQ(ssrc_, packet.Ssrc());
  uint16_t sequence_number = packet.SequenceNumber();
  bool in_order =
      // First packet is always in order.
      last_receive_time_ms_ == 0 ||
      IsNewerSequenceNumber(sequence_number, received_seq_max_) ||
      // If we have a restart of the remote side this packet is still in order.
      !IsNewerSequenceNumber(sequence_number,
                             received_seq_max_ - max_reordering_threshold_);
  int64_t now_ms = clock_->TimeInMilliseconds();

  incoming_bitrate_.Update(packet.size(), now_ms);
  receive_counters_.transmitted.AddPacket(packet);
  if (!in_order && enable_retransmit_detection_ &&
      IsRetransmitOfOldPacket(packet, now_ms)) {
    receive_counters_.retransmitted.AddPacket(packet);
  }

  if (receive_counters_.transmitted.packets == 1) {
    received_seq_first_ = packet.SequenceNumber();
    receive_counters_.first_packet_time_ms = now_ms;
  }

  // Count only the new packets received. That is, if packets 1, 2, 3, 5, 4, 6
  // are received, 4 will be ignored.
  if (in_order) {
    // Wrong if we use RetransmitOfOldPacket.
    if (receive_counters_.transmitted.packets > 1 &&
        received_seq_max_ > packet.SequenceNumber()) {
      // Wrap around detected.
      received_seq_wraps_++;
    }
    // New max.
    received_seq_max_ = packet.SequenceNumber();

    // If new time stamp and more than one in-order packet received, calculate
    // new jitter statistics.
    if (packet.Timestamp() != last_received_timestamp_ &&
        (receive_counters_.transmitted.packets -
         receive_counters_.retransmitted.packets) > 1) {
      UpdateJitter(packet, now_ms);
    }
    last_received_timestamp_ = packet.Timestamp();
    last_receive_time_ms_ = now_ms;
  }
  return receive_counters_;
}

void StreamStatisticianImpl::UpdateJitter(const RtpPacketReceived& packet,
                                          int64_t receive_time_ms) {
  int64_t receive_diff_ms = receive_time_ms - last_receive_time_ms_;
  RTC_DCHECK_GE(receive_diff_ms, 0);
  uint32_t receive_diff_rtp = static_cast<uint32_t>(
      (receive_diff_ms * packet.payload_type_frequency()) / 1000);
  int32_t time_diff_samples =
      receive_diff_rtp - (packet.Timestamp() - last_received_timestamp_);

  time_diff_samples = std::abs(time_diff_samples);

  // lib_jingle sometimes deliver crazy jumps in TS for the same stream.
  // If this happens, don't update jitter value. Use 5 secs video frequency
  // as the threshold.
  if (time_diff_samples < 450000) {
    // Note we calculate in Q4 to avoid using float.
    int32_t jitter_diff_q4 = (time_diff_samples << 4) - jitter_q4_;
    jitter_q4_ += ((jitter_diff_q4 + 8) >> 4);
  }
}

void StreamStatisticianImpl::FecPacketReceived(
    const RtpPacketReceived& packet) {
  StreamDataCounters counters;
  {
    rtc::CritScope cs(&stream_lock_);
    receive_counters_.fec.AddPacket(packet);
    counters = receive_counters_;
  }
  if (rtp_callback_)
    rtp_callback_->DataCountersUpdated(counters, ssrc_);
}

void StreamStatisticianImpl::SetMaxReorderingThreshold(
    int max_reordering_threshold) {
  rtc::CritScope cs(&stream_lock_);
  max_reordering_threshold_ = max_reordering_threshold;
}

void StreamStatisticianImpl::EnableRetransmitDetection(bool enable) {
  rtc::CritScope cs(&stream_lock_);
  enable_retransmit_detection_ = enable;
}

bool StreamStatisticianImpl::GetStatistics(RtcpStatistics* statistics,
                                           bool reset) {
  {
    rtc::CritScope cs(&stream_lock_);
    if (received_seq_first_ == 0 &&
        receive_counters_.transmitted.payload_bytes == 0) {
      // We have not received anything.
      return false;
    }

    if (!reset) {
      if (last_report_inorder_packets_ == 0) {
        // No report.
        return false;
      }
      // Just get last report.
      *statistics = last_reported_statistics_;
      return true;
    }

    *statistics = CalculateRtcpStatistics();
  }

  if (rtcp_callback_)
    rtcp_callback_->StatisticsUpdated(*statistics, ssrc_);
  return true;
}

bool StreamStatisticianImpl::GetActiveStatisticsAndReset(
    RtcpStatistics* statistics) {
  {
    rtc::CritScope cs(&stream_lock_);
    if (clock_->TimeInMilliseconds() - last_receive_time_ms_ >=
        kStatisticsTimeoutMs) {
      // Not active.
      return false;
    }
    if (received_seq_first_ == 0 &&
        receive_counters_.transmitted.payload_bytes == 0) {
      // We have not received anything.
      return false;
    }

    *statistics = CalculateRtcpStatistics();
  }

  if (rtcp_callback_)
    rtcp_callback_->StatisticsUpdated(*statistics, ssrc_);
  return true;
}

RtcpStatistics StreamStatisticianImpl::CalculateRtcpStatistics() {
  RtcpStatistics stats;

  if (last_report_inorder_packets_ == 0) {
    // First time we send a report.
    last_report_seq_max_ = received_seq_first_ - 1;
  }

  // Calculate fraction lost.
  uint16_t exp_since_last = (received_seq_max_ - last_report_seq_max_);

  if (last_report_seq_max_ > received_seq_max_) {
    // Can we assume that the seq_num can't go decrease over a full RTCP period?
    exp_since_last = 0;
  }

  // Number of received RTP packets since last report, counts all packets but
  // not re-transmissions.
  uint32_t rec_since_last = (receive_counters_.transmitted.packets -
                             receive_counters_.retransmitted.packets) -
                            last_report_inorder_packets_;

  // With NACK we don't know the expected retransmissions during the last
  // second. We know how many "old" packets we have received. We just count
  // the number of old received to estimate the loss, but it still does not
  // guarantee an exact number since we run this based on time triggered by
  // sending of an RTP packet. This should have a minimum effect.

  // With NACK we don't count old packets as received since they are
  // re-transmitted. We use RTT to decide if a packet is re-ordered or
  // re-transmitted.
  uint32_t retransmitted_packets =
      receive_counters_.retransmitted.packets - last_report_old_packets_;
  rec_since_last += retransmitted_packets;

  int32_t missing = 0;
  if (exp_since_last > rec_since_last) {
    missing = (exp_since_last - rec_since_last);
  }
  uint8_t local_fraction_lost = 0;
  if (exp_since_last) {
    // Scale 0 to 255, where 255 is 100% loss.
    local_fraction_lost = static_cast<uint8_t>(255 * missing / exp_since_last);
  }
  stats.fraction_lost = local_fraction_lost;

  // We need a counter for cumulative loss too.
  // TODO(danilchap): Ensure cumulative loss is below maximum value of 2^24.
  cumulative_loss_ += missing;
  stats.packets_lost = cumulative_loss_;
  stats.extended_highest_sequence_number =
      (received_seq_wraps_ << 16) + received_seq_max_;
  // Note: internal jitter value is in Q4 and needs to be scaled by 1/16.
  stats.jitter = jitter_q4_ >> 4;

  // Store this report.
  last_reported_statistics_ = stats;

  // Only for report blocks in RTCP SR and RR.
  last_report_inorder_packets_ = receive_counters_.transmitted.packets -
                                 receive_counters_.retransmitted.packets;
  last_report_old_packets_ = receive_counters_.retransmitted.packets;
  last_report_seq_max_ = received_seq_max_;
  BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "cumulative_loss_pkts",
                                  clock_->TimeInMilliseconds(),
                                  cumulative_loss_, ssrc_);
  BWE_TEST_LOGGING_PLOT_WITH_SSRC(
      1, "received_seq_max_pkts", clock_->TimeInMilliseconds(),
      (received_seq_max_ - received_seq_first_), ssrc_);

  return stats;
}

void StreamStatisticianImpl::GetDataCounters(size_t* bytes_received,
                                             uint32_t* packets_received) const {
  rtc::CritScope cs(&stream_lock_);
  if (bytes_received) {
    *bytes_received = receive_counters_.transmitted.payload_bytes +
                      receive_counters_.transmitted.header_bytes +
                      receive_counters_.transmitted.padding_bytes;
  }
  if (packets_received) {
    *packets_received = receive_counters_.transmitted.packets;
  }
}

void StreamStatisticianImpl::GetReceiveStreamDataCounters(
    StreamDataCounters* data_counters) const {
  rtc::CritScope cs(&stream_lock_);
  *data_counters = receive_counters_;
}

uint32_t StreamStatisticianImpl::BitrateReceived() const {
  rtc::CritScope cs(&stream_lock_);
  return incoming_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

bool StreamStatisticianImpl::IsRetransmitOfOldPacket(
    const RtpPacketReceived& packet,
    int64_t now_ms) const {
  uint32_t frequency_khz = packet.payload_type_frequency() / 1000;
  RTC_DCHECK_GT(frequency_khz, 0);

  int64_t time_diff_ms = now_ms - last_receive_time_ms_;

  // Diff in time stamp since last received in order.
  uint32_t timestamp_diff = packet.Timestamp() - last_received_timestamp_;
  uint32_t rtp_time_stamp_diff_ms = timestamp_diff / frequency_khz;

  int64_t max_delay_ms = 0;

  // Jitter standard deviation in samples.
  float jitter_std = sqrt(static_cast<float>(jitter_q4_ >> 4));

  // 2 times the standard deviation => 95% confidence.
  // And transform to milliseconds by dividing by the frequency in kHz.
  max_delay_ms = static_cast<int64_t>((2 * jitter_std) / frequency_khz);

  // Min max_delay_ms is 1.
  if (max_delay_ms == 0) {
    max_delay_ms = 1;
  }
  return time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms;
}

std::unique_ptr<ReceiveStatistics> ReceiveStatistics::Create(
    Clock* clock,
    RtcpStatisticsCallback* rtcp_callback,
    StreamDataCountersCallback* rtp_callback) {
  return absl::make_unique<ReceiveStatisticsImpl>(clock, rtcp_callback,
                                                  rtp_callback);
}

ReceiveStatisticsImpl::ReceiveStatisticsImpl(
    Clock* clock,
    RtcpStatisticsCallback* rtcp_callback,
    StreamDataCountersCallback* rtp_callback)
    : clock_(clock),
      last_returned_ssrc_(0),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold),
      rtcp_stats_callback_(rtcp_callback),
      rtp_stats_callback_(rtp_callback) {}

ReceiveStatisticsImpl::~ReceiveStatisticsImpl() {
  while (!statisticians_.empty()) {
    delete statisticians_.begin()->second;
    statisticians_.erase(statisticians_.begin());
  }
}

void ReceiveStatisticsImpl::OnRtpPacket(const RtpPacketReceived& packet) {
  StreamStatisticianImpl* impl;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    auto it = statisticians_.find(packet.Ssrc());
    if (it != statisticians_.end()) {
      impl = it->second;
    } else {
      impl = new StreamStatisticianImpl(
          packet.Ssrc(), clock_, /* enable_retransmit_detection = */ false,
          max_reordering_threshold_, rtcp_stats_callback_, rtp_stats_callback_);
      statisticians_[packet.Ssrc()] = impl;
    }
  }
  // StreamStatisticianImpl instance is created once and only destroyed when
  // this whole ReceiveStatisticsImpl is destroyed. StreamStatisticianImpl has
  // it's own locking so don't hold receive_statistics_lock_ (potential
  // deadlock).
  impl->OnRtpPacket(packet);
}

void ReceiveStatisticsImpl::FecPacketReceived(const RtpPacketReceived& packet) {
  StreamStatisticianImpl* impl;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    auto it = statisticians_.find(packet.Ssrc());
    // Ignore FEC if it is the first packet.
    if (it == statisticians_.end())
      return;
    impl = it->second;
  }
  impl->FecPacketReceived(packet);
}

StreamStatistician* ReceiveStatisticsImpl::GetStatistician(
    uint32_t ssrc) const {
  rtc::CritScope cs(&receive_statistics_lock_);
  auto it = statisticians_.find(ssrc);
  if (it == statisticians_.end())
    return NULL;
  return it->second;
}

void ReceiveStatisticsImpl::SetMaxReorderingThreshold(
    int max_reordering_threshold) {
  std::map<uint32_t, StreamStatisticianImpl*> statisticians;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    max_reordering_threshold_ = max_reordering_threshold;
    statisticians = statisticians_;
  }
  for (auto& statistician : statisticians) {
    statistician.second->SetMaxReorderingThreshold(max_reordering_threshold);
  }
}

void ReceiveStatisticsImpl::EnableRetransmitDetection(uint32_t ssrc,
                                                      bool enable) {
  StreamStatisticianImpl* impl;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    StreamStatisticianImpl*& impl_ref = statisticians_[ssrc];
    if (impl_ref == nullptr) {  // new element
      impl_ref = new StreamStatisticianImpl(
          ssrc, clock_, enable, max_reordering_threshold_, rtcp_stats_callback_,
          rtp_stats_callback_);
      return;
    }
    impl = impl_ref;
  }
  impl->EnableRetransmitDetection(enable);
}

std::vector<rtcp::ReportBlock> ReceiveStatisticsImpl::RtcpReportBlocks(
    size_t max_blocks) {
  std::map<uint32_t, StreamStatisticianImpl*> statisticians;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    statisticians = statisticians_;
  }
  std::vector<rtcp::ReportBlock> result;
  result.reserve(std::min(max_blocks, statisticians.size()));
  auto add_report_block = [&result](uint32_t media_ssrc,
                                    StreamStatisticianImpl* statistician) {
    // Do we have receive statistics to send?
    RtcpStatistics stats;
    if (!statistician->GetActiveStatisticsAndReset(&stats))
      return;
    result.emplace_back();
    rtcp::ReportBlock& block = result.back();
    block.SetMediaSsrc(media_ssrc);
    block.SetFractionLost(stats.fraction_lost);
    if (!block.SetCumulativeLost(stats.packets_lost)) {
      RTC_LOG(LS_WARNING) << "Cumulative lost is oversized.";
      result.pop_back();
      return;
    }
    block.SetExtHighestSeqNum(stats.extended_highest_sequence_number);
    block.SetJitter(stats.jitter);
  };

  const auto start_it = statisticians.upper_bound(last_returned_ssrc_);
  for (auto it = start_it;
       result.size() < max_blocks && it != statisticians.end(); ++it)
    add_report_block(it->first, it->second);
  for (auto it = statisticians.begin();
       result.size() < max_blocks && it != start_it; ++it)
    add_report_block(it->first, it->second);

  if (!result.empty())
    last_returned_ssrc_ = result.back().source_ssrc();
  return result;
}

}  // namespace webrtc
