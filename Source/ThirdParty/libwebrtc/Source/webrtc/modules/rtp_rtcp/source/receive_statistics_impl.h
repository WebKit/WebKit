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
                         bool enable_retransmit_detection,
                         RtcpStatisticsCallback* rtcp_callback,
                         StreamDataCountersCallback* rtp_callback);
  ~StreamStatisticianImpl() override;

  // |reset| here and in next method restarts calculation of fraction_lost stat.
  bool GetStatistics(RtcpStatistics* statistics, bool reset) override;
  bool GetActiveStatisticsAndReset(RtcpStatistics* statistics);
  void GetDataCounters(size_t* bytes_received,
                       uint32_t* packets_received) const override;
  void GetReceiveStreamDataCounters(
      StreamDataCounters* data_counters) const override;
  uint32_t BitrateReceived() const override;

  void IncomingPacket(const RTPHeader& rtp_header, size_t packet_length);
  void FecPacketReceived(const RTPHeader& header, size_t packet_length);
  void SetMaxReorderingThreshold(int max_reordering_threshold);
  void EnableRetransmitDetection(bool enable);

 private:
  bool IsRetransmitOfOldPacket(const RTPHeader& header) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  bool InOrderPacketInternal(uint16_t sequence_number) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  RtcpStatistics CalculateRtcpStatistics()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  void UpdateJitter(const RTPHeader& header, NtpTime receive_time)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);
  StreamDataCounters UpdateCounters(const RTPHeader& rtp_header,
                                    size_t packet_length,
                                    bool retransmitted)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stream_lock_);

  const uint32_t ssrc_;
  Clock* const clock_;
  rtc::CriticalSection stream_lock_;
  RateStatistics incoming_bitrate_ RTC_GUARDED_BY(&stream_lock_);
  // In number of packets or sequence numbers.
  int max_reordering_threshold_ RTC_GUARDED_BY(&stream_lock_);
  bool enable_retransmit_detection_ RTC_GUARDED_BY(&stream_lock_);

  // Stats on received RTP packets.
  uint32_t jitter_q4_ RTC_GUARDED_BY(&stream_lock_);
  uint32_t cumulative_loss_ RTC_GUARDED_BY(&stream_lock_);

  int64_t last_receive_time_ms_ RTC_GUARDED_BY(&stream_lock_);
  NtpTime last_receive_time_ntp_ RTC_GUARDED_BY(&stream_lock_);
  uint32_t last_received_timestamp_ RTC_GUARDED_BY(&stream_lock_);
  uint16_t received_seq_first_ RTC_GUARDED_BY(&stream_lock_);
  uint16_t received_seq_max_ RTC_GUARDED_BY(&stream_lock_);
  uint16_t received_seq_wraps_ RTC_GUARDED_BY(&stream_lock_);

  // Current counter values.
  size_t received_packet_overhead_ RTC_GUARDED_BY(&stream_lock_);
  StreamDataCounters receive_counters_ RTC_GUARDED_BY(&stream_lock_);

  // Counter values when we sent the last report.
  uint32_t last_report_inorder_packets_ RTC_GUARDED_BY(&stream_lock_);
  uint32_t last_report_old_packets_ RTC_GUARDED_BY(&stream_lock_);
  uint16_t last_report_seq_max_ RTC_GUARDED_BY(&stream_lock_);
  RtcpStatistics last_reported_statistics_ RTC_GUARDED_BY(&stream_lock_);

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

  // Implement RtpPacketSinkInterface
  void OnRtpPacket(const RtpPacketReceived& packet) override;

  // Implement ReceiveStatistics.
  void IncomingPacket(const RTPHeader& header, size_t packet_length) override;
  void FecPacketReceived(const RtpPacketReceived& packet) override;
  StreamStatistician* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void EnableRetransmitDetection(uint32_t ssrc, bool enable) override;

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
