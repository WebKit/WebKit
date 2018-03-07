/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_REPORT_BLOCK_STATS_H_
#define VIDEO_REPORT_BLOCK_STATS_H_

#include <map>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

// Helper class for rtcp statistics.
class ReportBlockStats {
 public:
  typedef std::map<uint32_t, RTCPReportBlock> ReportBlockMap;
  typedef std::vector<RTCPReportBlock> ReportBlockVector;
  ReportBlockStats();
  ~ReportBlockStats() {}

  // Updates stats and stores report blocks.
  // Returns an aggregate of the |report_blocks|.
  RTCPReportBlock AggregateAndStore(const ReportBlockVector& report_blocks);

  // Updates stats and stores report block.
  void Store(const RtcpStatistics& rtcp_stats,
             uint32_t remote_ssrc,
             uint32_t source_ssrc);

  // Returns the total fraction of lost packets (or -1 if less than two report
  // blocks have been stored).
  int FractionLostInPercent() const;

 private:
  // Updates the total number of packets/lost packets.
  // Stores the report block.
  // Returns the number of packets/lost packets since previous report block.
  void StoreAndAddPacketIncrement(const RTCPReportBlock& report_block,
                                  uint32_t* num_sequence_numbers,
                                  uint32_t* num_lost_sequence_numbers);

  // The total number of packets/lost packets.
  uint32_t num_sequence_numbers_;
  uint32_t num_lost_sequence_numbers_;

  // Map holding the last stored report block (mapped by the source SSRC).
  ReportBlockMap prev_report_blocks_;
};

}  // namespace webrtc

#endif  // VIDEO_REPORT_BLOCK_STATS_H_

