/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
#define MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_

#include <map>
#include <vector>

#include "call/rtp_packet_sink_interface.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

class Clock;

class ReceiveStatisticsProvider {
 public:
  virtual ~ReceiveStatisticsProvider() = default;
  // Collects receive statistic in a form of rtcp report blocks.
  // Returns at most |max_blocks| report blocks.
  virtual std::vector<rtcp::ReportBlock> RtcpReportBlocks(
      size_t max_blocks) = 0;
};

class StreamStatistician {
 public:
  virtual ~StreamStatistician();

  virtual bool GetStatistics(RtcpStatistics* statistics, bool reset) = 0;
  virtual void GetDataCounters(size_t* bytes_received,
                               uint32_t* packets_received) const = 0;

  // Gets received stream data counters (includes reset counter values).
  virtual void GetReceiveStreamDataCounters(
      StreamDataCounters* data_counters) const = 0;

  virtual uint32_t BitrateReceived() const = 0;
};

class ReceiveStatistics : public ReceiveStatisticsProvider,
                          public RtpPacketSinkInterface {
 public:
  ~ReceiveStatistics() override = default;

  static ReceiveStatistics* Create(Clock* clock);

  // Updates the receive statistics with this packet.
  // TODO(bugs.webrtc.org/8016): Deprecated. Delete as soon as
  // downstream code is updated to use OnRtpPacket.
  RTC_DEPRECATED
  virtual void IncomingPacket(const RTPHeader& rtp_header,
                              size_t packet_length) = 0;

  // Increment counter for number of FEC packets received.
  virtual void FecPacketReceived(const RtpPacketReceived& packet) = 0;

  // Returns a pointer to the statistician of an ssrc.
  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const = 0;

  // Sets the max reordering threshold in number of packets.
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;
  // Detect retransmissions, enabling updates of the retransmitted counters. The
  // default is false.
  virtual void EnableRetransmitDetection(uint32_t ssrc, bool enable) = 0;

  // Called on new RTCP stats creation.
  virtual void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) = 0;

  // Called on new RTP stats creation.
  virtual void RegisterRtpStatisticsCallback(
      StreamDataCountersCallback* callback) = 0;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
