/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_

#include <memory>
#include <string>
#include <vector>

#include "modules/rtp_rtcp/source/rtcp_transceiver_config.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {
//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is thread-safe wrapper of RtcpTransceiverImpl
class RtcpTransceiver {
 public:
  explicit RtcpTransceiver(const RtcpTransceiverConfig& config);
  ~RtcpTransceiver();

  // Handles incoming rtcp packets.
  void ReceivePacket(rtc::CopyOnWriteBuffer packet);

  // Sends RTCP packets starting with a sender or receiver report.
  void SendCompoundPacket();

  // (REMB) Receiver Estimated Max Bitrate.
  // Includes REMB in following compound packets.
  void SetRemb(int bitrate_bps, std::vector<uint32_t> ssrcs);
  // Stops sending REMB in following compound packets.
  void UnsetRemb();

  // Reports missing packets, https://tools.ietf.org/html/rfc4585#section-6.2.1
  void SendNack(uint32_t ssrc, std::vector<uint16_t> sequence_numbers);

  // Requests new key frame.
  // using PLI, https://tools.ietf.org/html/rfc4585#section-6.3.1.1
  void SendPictureLossIndication(std::vector<uint32_t> ssrcs);
  // using FIR, https://tools.ietf.org/html/rfc5104#section-4.3.1.2
  void SendFullIntraRequest(std::vector<uint32_t> ssrcs);

 private:
  rtc::TaskQueue* const task_queue_;
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver_;
  rtc::WeakPtrFactory<RtcpTransceiverImpl> ptr_factory_;
  // TaskQueue, and thus tasks posted to it, may outlive this.
  // Thus when Posting task class always pass copy of the weak_ptr to access
  // the RtcpTransceiver and never guarantee it still will be alive when task
  // runs.
  rtc::WeakPtr<RtcpTransceiverImpl> ptr_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiver);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_
