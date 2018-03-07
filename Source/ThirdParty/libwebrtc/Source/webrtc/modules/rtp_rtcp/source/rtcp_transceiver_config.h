/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_

#include <string>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
class ReceiveStatisticsProvider;
class Transport;

struct RtcpTransceiverConfig {
  RtcpTransceiverConfig();
  RtcpTransceiverConfig(const RtcpTransceiverConfig&);
  RtcpTransceiverConfig& operator=(const RtcpTransceiverConfig&);
  ~RtcpTransceiverConfig();

  // Logs the error and returns false if configuration miss key objects or
  // is inconsistant. May log warnings.
  bool Validate() const;

  // Used to prepend all log messages. Can be empty.
  std::string debug_id;

  // Ssrc to use as default sender ssrc, e.g. for transport-wide feedbacks.
  uint32_t feedback_ssrc = 1;

  // Canonical End-Point Identifier of the local particiapnt.
  // Defined in rfc3550 section 6 note 2 and section 6.5.1.
  std::string cname;

  // Maximum packet size outgoing transport accepts.
  size_t max_packet_size = 1200;

  // Transport to send rtcp packets to. Should be set.
  Transport* outgoing_transport = nullptr;

  // Queue for scheduling delayed tasks, e.g. sending periodic compound packets.
  rtc::TaskQueue* task_queue = nullptr;

  // Rtcp report block generator for outgoing receiver reports.
  ReceiveStatisticsProvider* receive_statistics = nullptr;

  // Configures if sending should
  //  enforce compound packets: https://tools.ietf.org/html/rfc4585#section-3.1
  //  or allow reduced size packets: https://tools.ietf.org/html/rfc5506
  // Receiving accepts both compound and reduced-size packets.
  RtcpMode rtcp_mode = RtcpMode::kCompound;
  //
  // Tuning parameters.
  //
  // Delay before 1st periodic compound packet.
  int initial_report_delay_ms = 500;

  // Period between periodic compound packets.
  int report_period_ms = 1000;

  //
  // Flags for features and experiments.
  //
  bool schedule_periodic_compound_packets = true;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_
