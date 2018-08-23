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
#include <vector>

#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
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
    RtcpStatisticsCallback* rtcp_callback,
    StreamDataCountersCallback* rtp_callback)
    : ssrc_(ssrc),
      clock_(clock),
      incoming_bitrate_(kStatisticsProcessIntervalMs,
                        RateStatistics::kBpsScale),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold),
      jitter_q4_(0),
      last_receive_time_ms_(0),
      last_received_timestamp_(0),
      received_seq_first_(0),
      received_seq_max_(0),
      received_seq_wraps_(0),
      received_packet_overhead_(12),
      rtcp_callback_(rtcp_callback),
      rtp_callback_(rtp_callback) {}

StreamStatisticianImpl::~StreamStatisticianImpl() = default;

void StreamStatisticianImpl::IncomingPacket(const RTPHeader& header,
                                            size_t packet_length,
                                            bool retransmitted) {
  StreamDataCounters counters;
  RtcpStatistics rtcp_stats;
  {
    rtc::CritScope cs(&stream_lock_);
    counters = UpdateCounters(header, packet_length, retransmitted);
    // We only want to recalculate |fraction_lost| when sending an RTCP SR or
    // RR.
    rtcp_stats = CalculateRtcpStatistics(/*update_fraction_lost=*/false);
  }

  rtp_callback_->DataCountersUpdated(counters, ssrc_);
  rtcp_callback_->StatisticsUpdated(rtcp_stats, ssrc_);
}

StreamDataCounters StreamStatisticianImpl::UpdateCounters(
    const RTPHeader& header,
    size_t packet_length,
    bool retransmitted) {
  bool in_order = InOrderPacketInternal(header.sequenceNumber);
  RTC_DCHECK_EQ(ssrc_, header.ssrc);
  incoming_bitrate_.Update(packet_length, clock_->TimeInMilliseconds());
  receive_counters_.transmitted.AddPacket(packet_length, header);
  if (!in_order && retransmitted) {
    receive_counters_.retransmitted.AddPacket(packet_length, header);
  }

  if (receive_counters_.transmitted.packets == 1) {
    received_seq_first_ = header.sequenceNumber;
    receive_counters_.first_packet_time_ms = clock_->TimeInMilliseconds();
  }

  // Count only the new packets received. That is, if packets 1, 2, 3, 5, 4, 6
  // are received, 4 will be ignored.
  if (in_order) {
    // Current time in samples.
    NtpTime receive_time = clock_->CurrentNtpTime();

    // Wrong if we use RetransmitOfOldPacket.
    if (receive_counters_.transmitted.packets > 1 &&
        received_seq_max_ > header.sequenceNumber) {
      // Wrap around detected.
      received_seq_wraps_++;
    }
    // New max.
    received_seq_max_ = header.sequenceNumber;

    // If new time stamp and more than one in-order packet received, calculate
    // new jitter statistics.
    if (header.timestamp != last_received_timestamp_ &&
        (receive_counters_.transmitted.packets -
         receive_counters_.retransmitted.packets) > 1) {
      UpdateJitter(header, receive_time);
    }
    last_received_timestamp_ = header.timestamp;
    last_receive_time_ntp_ = receive_time;
    last_receive_time_ms_ = clock_->TimeInMilliseconds();
  }

  size_t packet_oh = header.headerLength + header.paddingLength;

  // Our measured overhead. Filter from RFC 5104 4.2.1.2:
  // avg_OH (new) = 15/16*avg_OH (old) + 1/16*pckt_OH,
  received_packet_overhead_ = (15 * received_packet_overhead_ + packet_oh) >> 4;
  return receive_counters_;
}

void StreamStatisticianImpl::UpdateJitter(const RTPHeader& header,
                                          NtpTime receive_time) {
  uint32_t receive_time_rtp =
      NtpToRtp(receive_time, header.payload_type_frequency);
  uint32_t last_receive_time_rtp =
      NtpToRtp(last_receive_time_ntp_, header.payload_type_frequency);
  int32_t time_diff_samples = (receive_time_rtp - last_receive_time_rtp) -
                              (header.timestamp - last_received_timestamp_);

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

void StreamStatisticianImpl::FecPacketReceived(const RTPHeader& header,
                                               size_t packet_length) {
  StreamDataCounters counters;
  {
    rtc::CritScope cs(&stream_lock_);
    receive_counters_.fec.AddPacket(packet_length, header);
    counters = receive_counters_;
  }
  rtp_callback_->DataCountersUpdated(counters, ssrc_);
}

void StreamStatisticianImpl::SetMaxReorderingThreshold(
    int max_reordering_threshold) {
  rtc::CritScope cs(&stream_lock_);
  max_reordering_threshold_ = max_reordering_threshold;
}

bool StreamStatisticianImpl::GetStatistics(RtcpStatistics* statistics,
                                           bool update_fraction_lost) {
  {
    rtc::CritScope cs(&stream_lock_);
    if (received_seq_first_ == 0 &&
        receive_counters_.transmitted.payload_bytes == 0) {
      // We have not received anything.
      return false;
    }

    *statistics = CalculateRtcpStatistics(update_fraction_lost);
  }

  if (update_fraction_lost) {
    rtcp_callback_->StatisticsUpdated(*statistics, ssrc_);
  }
  return true;
}

bool StreamStatisticianImpl::GetActiveStatisticsAndReset(
    RtcpStatistics* statistics) {
  {
    rtc::CritScope cs(&stream_lock_);
    if (clock_->CurrentNtpInMilliseconds() - last_receive_time_ntp_.ToMs() >=
        kStatisticsTimeoutMs) {
      // Not active.
      return false;
    }
    if (received_seq_first_ == 0 &&
        receive_counters_.transmitted.payload_bytes == 0) {
      // We have not received anything.
      return false;
    }

    *statistics = CalculateRtcpStatistics(/*update_fraction_lost=*/true);
  }

  rtcp_callback_->StatisticsUpdated(*statistics, ssrc_);
  return true;
}

RtcpStatistics StreamStatisticianImpl::CalculateRtcpStatistics(
    bool update_fraction_lost) {
  RtcpStatistics statistics;

  uint32_t extended_seq_max = (received_seq_wraps_ << 16) + received_seq_max_;

  if (update_fraction_lost) {
    if (last_report_received_packets_ == 0) {
      // First time we're calculating fraction lost.
      last_report_extended_seq_max_ = received_seq_first_ - 1;
    }

    uint32_t exp_since_last =
        (extended_seq_max - last_report_extended_seq_max_);

    // Number of received RTP packets since last report; counts all packets
    // including retransmissions.
    uint32_t rec_since_last =
        receive_counters_.transmitted.packets - last_report_received_packets_;

    // Calculate fraction lost according to RFC3550 Appendix A.3. Snap to 0 if
    // negative (which is possible with duplicate packets).
    uint8_t local_fraction_lost = 0;
    if (exp_since_last > rec_since_last) {
      // Scale 0 to 255, where 255 is 100% loss.
      local_fraction_lost = static_cast<uint8_t>(
          255 * (exp_since_last - rec_since_last) / exp_since_last);
    }

    last_fraction_lost_ = local_fraction_lost;
    last_report_received_packets_ = receive_counters_.transmitted.packets;
    last_report_extended_seq_max_ = extended_seq_max;
  }

  statistics.fraction_lost = last_fraction_lost_;
  // Calculate cumulative loss, according to RFC3550 Appendix A.3.
  uint32_t total_expected_packets = extended_seq_max - received_seq_first_ + 1;
  statistics.packets_lost =
      total_expected_packets - receive_counters_.transmitted.packets;
  // Since cumulative loss is carried in a signed 24-bit field in RTCP, we may
  // need to clamp it.
  statistics.packets_lost = std::min(statistics.packets_lost, 0x7fffff);
  // TODO(bugs.webrtc.org/9598): This packets_lost should be signed according to
  // RFC3550. However, old WebRTC implementations reads it as unsigned.
  // Therefore we limit this to 0.
  statistics.packets_lost = std::max(statistics.packets_lost, 0);
  statistics.extended_highest_sequence_number = extended_seq_max;
  // Note: internal jitter value is in Q4 and needs to be scaled by 1/16.
  statistics.jitter = jitter_q4_ >> 4;

  BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "cumulative_loss_pkts",
                                  clock_->TimeInMilliseconds(),
                                  statistics.packets_lost, ssrc_);
  BWE_TEST_LOGGING_PLOT_WITH_SSRC(
      1, "received_seq_max_pkts", clock_->TimeInMilliseconds(),
      (received_seq_max_ - received_seq_first_), ssrc_);

  return statistics;
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
    const RTPHeader& header) const {
  rtc::CritScope cs(&stream_lock_);
  if (InOrderPacketInternal(header.sequenceNumber)) {
    return false;
  }
  uint32_t frequency_khz = header.payload_type_frequency / 1000;
  assert(frequency_khz > 0);

  int64_t time_diff_ms = clock_->TimeInMilliseconds() - last_receive_time_ms_;

  // Diff in time stamp since last received in order.
  uint32_t timestamp_diff = header.timestamp - last_received_timestamp_;
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

bool StreamStatisticianImpl::InOrderPacketInternal(
    uint16_t sequence_number) const {
  // First packet is always in order.
  if (receive_counters_.transmitted.packets == 0)
    return true;

  if (IsNewerSequenceNumber(sequence_number, received_seq_max_)) {
    return true;
  } else {
    // If we have a restart of the remote side this packet is still in order.
    return !IsNewerSequenceNumber(
        sequence_number, received_seq_max_ - max_reordering_threshold_);
  }
}

ReceiveStatistics* ReceiveStatistics::Create(Clock* clock) {
  return new ReceiveStatisticsImpl(clock);
}

ReceiveStatisticsImpl::ReceiveStatisticsImpl(Clock* clock)
    : clock_(clock),
      last_returned_ssrc_(0),
      rtcp_stats_callback_(NULL),
      rtp_stats_callback_(NULL) {}

ReceiveStatisticsImpl::~ReceiveStatisticsImpl() {
  while (!statisticians_.empty()) {
    delete statisticians_.begin()->second;
    statisticians_.erase(statisticians_.begin());
  }
}

void ReceiveStatisticsImpl::IncomingPacket(const RTPHeader& header,
                                           size_t packet_length,
                                           bool retransmitted) {
  StreamStatisticianImpl* impl;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    auto it = statisticians_.find(header.ssrc);
    if (it != statisticians_.end()) {
      impl = it->second;
    } else {
      impl = new StreamStatisticianImpl(header.ssrc, clock_, this, this);
      statisticians_[header.ssrc] = impl;
    }
  }
  // StreamStatisticianImpl instance is created once and only destroyed when
  // this whole ReceiveStatisticsImpl is destroyed. StreamStatisticianImpl has
  // it's own locking so don't hold receive_statistics_lock_ (potential
  // deadlock).
  impl->IncomingPacket(header, packet_length, retransmitted);
}

void ReceiveStatisticsImpl::FecPacketReceived(const RTPHeader& header,
                                              size_t packet_length) {
  StreamStatisticianImpl* impl;
  {
    rtc::CritScope cs(&receive_statistics_lock_);
    auto it = statisticians_.find(header.ssrc);
    // Ignore FEC if it is the first packet.
    if (it == statisticians_.end())
      return;
    impl = it->second;
  }
  impl->FecPacketReceived(header, packet_length);
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
  rtc::CritScope cs(&receive_statistics_lock_);
  for (auto& statistician : statisticians_) {
    statistician.second->SetMaxReorderingThreshold(max_reordering_threshold);
  }
}

void ReceiveStatisticsImpl::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  rtc::CritScope cs(&receive_statistics_lock_);
  if (callback != NULL)
    assert(rtcp_stats_callback_ == NULL);
  rtcp_stats_callback_ = callback;
}

void ReceiveStatisticsImpl::StatisticsUpdated(const RtcpStatistics& statistics,
                                              uint32_t ssrc) {
  rtc::CritScope cs(&receive_statistics_lock_);
  if (rtcp_stats_callback_)
    rtcp_stats_callback_->StatisticsUpdated(statistics, ssrc);
}

void ReceiveStatisticsImpl::CNameChanged(const char* cname, uint32_t ssrc) {
  rtc::CritScope cs(&receive_statistics_lock_);
  if (rtcp_stats_callback_)
    rtcp_stats_callback_->CNameChanged(cname, ssrc);
}

void ReceiveStatisticsImpl::RegisterRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  rtc::CritScope cs(&receive_statistics_lock_);
  if (callback != NULL)
    assert(rtp_stats_callback_ == NULL);
  rtp_stats_callback_ = callback;
}

void ReceiveStatisticsImpl::DataCountersUpdated(const StreamDataCounters& stats,
                                                uint32_t ssrc) {
  rtc::CritScope cs(&receive_statistics_lock_);
  if (rtp_stats_callback_) {
    rtp_stats_callback_->DataCountersUpdated(stats, ssrc);
  }
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
