/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_RTP_TRANSPORT_TEST_UTIL_H_
#define PC_TEST_RTP_TRANSPORT_TEST_UTIL_H_

#include "call/rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "pc/rtp_transport_internal.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace webrtc {

// Used to handle the signals when the RtpTransport receives an RTP/RTCP packet.
// Used in Rtp/Srtp/DtlsTransport unit tests.
class TransportObserver : public RtpPacketSinkInterface,
                          public sigslot::has_slots<> {
 public:
  TransportObserver() {}

  explicit TransportObserver(RtpTransportInternal* rtp_transport) {
    rtp_transport->SignalRtcpPacketReceived.connect(
        this, &TransportObserver::OnRtcpPacketReceived);
    rtp_transport->SignalReadyToSend.connect(this,
                                             &TransportObserver::OnReadyToSend);
    rtp_transport->SignalUnDemuxableRtpPacketReceived.connect(
        this, &TransportObserver::OnUndemuxableRtpPacket);
  }

  // RtpPacketInterface override.
  void OnRtpPacket(const RtpPacketReceived& packet) override {
    rtp_count_++;
    last_recv_rtp_packet_ = packet.Buffer();
  }

  void OnUndemuxableRtpPacket(const RtpPacketReceived& packet) {
    un_demuxable_rtp_count_++;
  }

  void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer* packet,
                            int64_t packet_time_us) {
    rtcp_count_++;
    last_recv_rtcp_packet_ = *packet;
  }

  int rtp_count() const { return rtp_count_; }
  int un_demuxable_rtp_count() const { return un_demuxable_rtp_count_; }
  int rtcp_count() const { return rtcp_count_; }

  rtc::CopyOnWriteBuffer last_recv_rtp_packet() {
    return last_recv_rtp_packet_;
  }

  rtc::CopyOnWriteBuffer last_recv_rtcp_packet() {
    return last_recv_rtcp_packet_;
  }

  void OnReadyToSend(bool ready) {
    ready_to_send_signal_count_++;
    ready_to_send_ = ready;
  }

  bool ready_to_send() { return ready_to_send_; }

  int ready_to_send_signal_count() { return ready_to_send_signal_count_; }

 private:
  bool ready_to_send_ = false;
  int rtp_count_ = 0;
  int un_demuxable_rtp_count_ = 0;
  int rtcp_count_ = 0;
  int ready_to_send_signal_count_ = 0;
  rtc::CopyOnWriteBuffer last_recv_rtp_packet_;
  rtc::CopyOnWriteBuffer last_recv_rtcp_packet_;
};

}  // namespace webrtc

#endif  // PC_TEST_RTP_TRANSPORT_TEST_UTIL_H_
