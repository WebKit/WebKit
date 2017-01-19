/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
#define WEBRTC_MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_

#include <map>

#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;

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

  // Returns true if the packet with RTP header |header| is likely to be a
  // retransmitted packet, false otherwise.
  virtual bool IsRetransmitOfOldPacket(const RTPHeader& header,
                                       int64_t min_rtt) const = 0;

  // Returns true if |sequence_number| is received in order, false otherwise.
  virtual bool IsPacketInOrder(uint16_t sequence_number) const = 0;
};

typedef std::map<uint32_t, StreamStatistician*> StatisticianMap;

class ReceiveStatistics {
 public:
  virtual ~ReceiveStatistics() {}

  static ReceiveStatistics* Create(Clock* clock);

  // Updates the receive statistics with this packet.
  virtual void IncomingPacket(const RTPHeader& rtp_header,
                              size_t packet_length,
                              bool retransmitted) = 0;

  // Increment counter for number of FEC packets received.
  virtual void FecPacketReceived(const RTPHeader& header,
                                 size_t packet_length) = 0;

  // Returns a map of all statisticians which have seen an incoming packet
  // during the last two seconds.
  virtual StatisticianMap GetActiveStatisticians() const = 0;

  // Returns a pointer to the statistician of an ssrc.
  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const = 0;

  // Sets the max reordering threshold in number of packets.
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;

  // Called on new RTCP stats creation.
  virtual void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) = 0;

  // Called on new RTP stats creation.
  virtual void RegisterRtpStatisticsCallback(
      StreamDataCountersCallback* callback) = 0;
};

class NullReceiveStatistics : public ReceiveStatistics {
 public:
  void IncomingPacket(const RTPHeader& rtp_header,
                      size_t packet_length,
                      bool retransmitted) override;
  void FecPacketReceived(const RTPHeader& header,
                         size_t packet_length) override;
  StatisticianMap GetActiveStatisticians() const override;
  StreamStatistician* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) override;
  void RegisterRtpStatisticsCallback(
      StreamDataCountersCallback* callback) override;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
