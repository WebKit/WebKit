/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_

#include "modules/rtp_rtcp/include/receive_statistics.h"

#include <math.h>

#include <algorithm>
#include <map>
#include <vector>

#include "rtc_base/criticalsection.h"
#include "rtc_base/rate_statistics.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

class StreamStatisticianImpl : public StreamStatistician {
 public:
  StreamStatisticianImpl(uint32_t ssrc,
                         Clock* clock,
                         RtcpStatisticsCallback* rtcp_callback,
                         StreamDataCountersCallback* rtp_callback);
  ~StreamStatisticianImpl() override;

  bool GetStatistics(RtcpStatistics* statistics,
                     bool update_fraction_lost) override;
  bool GetActiveStatisticsAndReset(RtcpStatistics* statistics);
  void GetDataCounters(size_t* bytes_received,
                       uint32_t* packets_received) const override;
  void GetReceiveStreamDataCounters(
      StreamDataCounters* data_counters) const override;
  uint32_t BitrateReceived() const override;
  bool IsRetransmitOfOldPacket(const RTPHeader& header) const override;

  void IncomingPacket(const RTPHeader& rtp_header,
                      size_t packet_length,
                      bool retransmitted);
  void FecPacketReceived(const RTPHeader& header, size_t packet_length);
  void SetMaxReorderingThreshold(int max_reordering_threshold);

 private:
  bool InOrderPacketInternal(uint16_t sequence_number) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  RtcpStatistics CalculateRtcpStatistics(bool update_fraction_lost)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  void UpdateJitter(const RTPHeader& header, NtpTime receive_time);
  StreamDataCounters UpdateCounters(const RTPHeader& rtp_header,
                                    size_t packet_length,
                                    bool retransmitted)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);

  const uint32_t ssrc_;
  Clock* const clock_;
  rtc::CriticalSection stream_lock_;
  RateStatistics incoming_bitrate_;
  int max_reordering_threshold_;  // In number of packets or sequence numbers.

  // Stats on received RTP packets.
  uint32_t jitter_q4_;

  int64_t last_receive_time_ms_;
  NtpTime last_receive_time_ntp_;
  uint32_t last_received_timestamp_;
  uint16_t received_seq_first_;
  uint16_t received_seq_max_;
  uint16_t received_seq_wraps_;

  // Current counter values.
  size_t received_packet_overhead_;
  StreamDataCounters receive_counters_ RTC_GUARDED_BY(stream_lock_);

  // Used to calculate fraction_lost between reports.
  uint32_t last_report_received_packets_ = 0;
  uint32_t last_report_extended_seq_max_ = 0;
  uint8_t last_fraction_lost_ = 0;

  // stream_lock_ shouldn't be held when calling callbacks.
  RtcpStatisticsCallback* const rtcp_callback_;
  StreamDataCountersCallback* const rtp_callback_;
};

class ReceiveStatisticsImpl : public ReceiveStatistics,
                              public RtcpStatisticsCallback,
                              public StreamDataCountersCallback {
 public:
  explicit ReceiveStatisticsImpl(Clock* clock);

  ~ReceiveStatisticsImpl() override;

  // Implement ReceiveStatisticsProvider.
  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override;

  // Implement ReceiveStatistics.
  void IncomingPacket(const RTPHeader& header,
                      size_t packet_length,
                      bool retransmitted) override;
  void FecPacketReceived(const RTPHeader& header,
                         size_t packet_length) override;
  StreamStatistician* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;

  void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) override;

  void RegisterRtpStatisticsCallback(
      StreamDataCountersCallback* callback) override;

 private:
  void StatisticsUpdated(const RtcpStatistics& statistics,
                         uint32_t ssrc) override;
  void CNameChanged(const char* cname, uint32_t ssrc) override;
  void DataCountersUpdated(const StreamDataCounters& counters,
                           uint32_t ssrc) override;

  Clock* const clock_;
  rtc::CriticalSection receive_statistics_lock_;
  uint32_t last_returned_ssrc_;
  std::map<uint32_t, StreamStatisticianImpl*> statisticians_;

  RtcpStatisticsCallback* rtcp_stats_callback_;
  StreamDataCountersCallback* rtp_stats_callback_;
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
